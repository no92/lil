#include <expected>

#include <lil/intel.h>
#include <lil/imports.h>

#include "src/coffee_lake/dp.h"
#include "src/edid.h"
#include "src/gmbus.h"
#include "src/regs.h"

#define NO_AUX_HANDSHAKE_LINK_TRAINING (1 << 6)

#define DPCD_POWER_D0 1
#define DPCD_POWER_D3 2

#define MG_DP_X1 (1 << 6)
#define MG_DP_X2 (1 << 7)

static inline uint64_t get_fia_base(uint32_t fia) {
    switch (fia) {
        case 1: return 0x163000;
        case 2: return 0x16E000;
        case 3: return 0x16F000;
        default: lil_panic("Unknown FIA");
    }
}

#define PORT_TX_DFLEXPA(fia) (get_fia_base(fia) + 0x880)
#define DP_PIN_ASSIGN_SHIFT(i) ((i) * 4)
#define DP_PIN_ASSIGN_MASK(i) (0xF << ((i) * 4))

typedef struct AuxRequest {
    uint8_t request;
    uint32_t address;
    uint8_t size;
    uint8_t tx[16];
} AuxRequest;

typedef struct AuxResponse {
    uint8_t reply, size;
    uint8_t data[20];
} AuxResponse;

static uint32_t dp_pack_aux(uint8_t* data, uint8_t size) {
    uint32_t v = 0;
    if(size > 4)
        size = 4;

    for(uint8_t i = 0; i < size; i++)
        v |= ((uint32_t)data[i]) << ((3 - i) * 8);

    return v;
}

static void dp_unpack_aux(uint32_t src, uint8_t* data, uint8_t size) {
    if(size > 4)
        size = 4;

    for(uint8_t i = 0; i < size; i++)
        data[i] = (src >> ((3 - i) * 8));
}

static uint32_t dp_get_aux_clock_div(struct LilGpu* gpu, int i) {
    if(gpu->gen >= GEN_SKL) {
        return i ? 0 : 1; // Skylake and up will automatically derive the divider
    } else {
        lil_panic("DP Unknown GPU Gen");
    }
}

static uint32_t dp_get_aux_send_ctl(struct LilGpu* gpu, uint8_t txsize) {
    if(gpu->gen >= GEN_SKL) {
        return DDI_AUX_CTL_BUSY | DDI_AUX_CTL_DONE | DDI_AUX_CTL_TIMEOUT | DDI_AUX_CTL_TIMEOUT_VAL_MAX | DDI_AUX_CTL_RX_ERR | DDI_AUX_SET_MSG_SIZE(txsize) | DDI_AUX_CTL_SKL_FW_SYNC_PULSE(32) | DDI_AUX_CTL_SKL_SYNC_PULSE(32);
    } else {
        lil_panic("DP Unknown GPU gen");
    }
}

static uint32_t dp_aux_wait(struct LilGpu* gpu, LilConnector *con) {
    int timeout = 10;

    while(timeout > 0) {
        if((REG(DDI_AUX_CTL(con->aux_ch)) & DDI_AUX_CTL_BUSY) == 0)
            return REG(DDI_AUX_CTL(con->aux_ch));

        lil_sleep(1);
        timeout--;
    }

    return REG(DDI_AUX_CTL(con->aux_ch));
}

static std::expected<uint8_t, LilError> dp_aux_xfer(struct LilGpu* gpu, LilConnector *con, uint8_t* tx, uint8_t tx_size, uint8_t* rx, uint8_t rxsize) {
    volatile uint32_t *data = REG_PTR(DDI_AUX_DATA(con->aux_ch));

    // Try to wait for any previous AUX cmds
    for(uint8_t i = 0; i < 3; i++) {
        if((REG(DDI_AUX_CTL(con->aux_ch)) & DDI_AUX_CTL_BUSY) == 0)
            break;
        lil_sleep(1);
    }

    if(REG(DDI_AUX_CTL(con->aux_ch)) & DDI_AUX_CTL_BUSY)
        lil_panic("DP AUX Busy");

    if(tx_size > 20 || rxsize > 20)
        lil_panic("DP AUX xfer too big");

    int clock = 0;
    bool done = false;
    uint32_t status = 0;
    uint32_t aux_clock_divider = 0;
    while((aux_clock_divider = dp_get_aux_clock_div(gpu, clock++))) {
        uint32_t send = dp_get_aux_send_ctl(gpu, tx_size);

        for(uint8_t i = 0; i < 5; i++) {
            for(size_t j = 0; j < tx_size; j += 4)
                data[j / 4] = dp_pack_aux(tx + j, tx_size - j);

            REG(DDI_AUX_CTL(con->aux_ch)) = send;

            status = dp_aux_wait(gpu, con);

            REG(DDI_AUX_CTL(con->aux_ch)) = status | DDI_AUX_CTL_DONE | DDI_AUX_CTL_TIMEOUT | DDI_AUX_CTL_RX_ERR;

            if(status & DDI_AUX_CTL_TIMEOUT) {
                lil_sleep(1);
                continue;
            }

            if(status & DDI_AUX_CTL_RX_ERR) {
                lil_sleep(1);
                continue;
            }

            if(status & DDI_AUX_CTL_DONE) {
                done = true;
                break;
            }
        }
    }

    if(!done)
        return std::unexpected(LilError::Timeout);

    if(status & DDI_AUX_CTL_TIMEOUT)
        lil_panic("DP AUX Timeout");

    if(status & DDI_AUX_CTL_RX_ERR)
        lil_panic("DP AUX Receive error");


    uint8_t received_bytes = DDI_AUX_GET_MSG_SIZE(status);
    if(received_bytes == 0 || received_bytes > 20)
        lil_panic("DP AUX Unknown recv bytes");

    if(received_bytes > rxsize)
        received_bytes = rxsize;

    for(size_t i = 0; i < received_bytes; i += 4)
        dp_unpack_aux(data[i / 4], rx + i, received_bytes - i);

    return received_bytes;
}

static std::expected<AuxResponse, LilError> dp_aux_cmd(struct LilGpu* gpu, LilConnector *con, AuxRequest req) {
    AuxResponse res = {};
    uint8_t tx[20] = {0};
    uint8_t rx[20] = {0};

    // CMD Header
    tx[0] = (req.request << 4) | ((req.address >> 16) & 0xF);
    tx[1] = (req.address >> 8) & 0xFF;
    tx[2] = req.address & 0xFF;
    tx[3] = req.size - 1;

    uint8_t op = req.request & ~DDI_AUX_I2C_MOT;
    if(op == DDI_AUX_I2C_WRITE || op == DDI_AUX_NATIVE_WRITE) {
        uint8_t tx_size = req.size ? (req.size + 4) : 3,
                rx_size = 2;

        for(size_t i = 0; i < req.size; i++)
            tx[i + 4] = req.tx[i];

        auto ret = dp_aux_xfer(gpu, con, tx, tx_size, rx, rx_size);
        if(ret && *ret > 0) {
            res.reply = rx[0] >> 4;
        } else if(!ret) {
			return std::unexpected(ret.error());
		}
    } else if(op == DDI_AUX_I2C_READ || op == DDI_AUX_NATIVE_READ) {
        uint8_t tx_size = req.size ? 4 : 3,
                rx_size = req.size + 1;

        auto ret = dp_aux_xfer(gpu, con, tx, tx_size, rx, rx_size);
        if(ret && *ret > 0) {
            res.reply = rx[0] >> 4;
            res.size = *ret - 1;

            for(uint8_t i = 0; i < res.size; i++)
                res.data[i] = rx[i + 1];
        } else if(!ret) {
			return std::unexpected(ret.error());
		}
    } else {
        lil_panic("Unknown DP AUX cmd");
    }

    return res;
}

// TODO: Check res.reply for any NACKs or DEFERs, instead of assuming i2c success, same for dp_aux_i2c_write
LilError dp_aux_i2c_read(struct LilGpu* gpu, LilConnector *con, uint16_t addr, uint8_t len, uint8_t* buf) {
    AuxRequest req = {};
    AuxResponse res = {};
    req.request = DDI_AUX_I2C_READ | DDI_AUX_I2C_MOT;
    req.address = addr;
    req.size = 0;
    auto ret = dp_aux_cmd(gpu, con, req);

	if(!ret)
		return ret.error();

    for(size_t i = 0; i < len; i++) {
        req.size = 1;

		ret = dp_aux_cmd(gpu, con, req);

		if(!ret)
			return ret.error();

        res = *ret;
        buf[i] = res.data[0];
    }

    req.request = DDI_AUX_I2C_READ;
    req.address = 0;
    req.size = 0;
    ret = dp_aux_cmd(gpu, con, req);

	if(!ret)
		return ret.error();

	return LilError::Success;
}

LilError dp_aux_i2c_write(struct LilGpu* gpu, LilConnector *con,  uint16_t addr, uint8_t len, uint8_t* buf) {
    AuxRequest req = {};
    req.request = DDI_AUX_I2C_WRITE | DDI_AUX_I2C_MOT;
    req.address = addr;
    req.size = 0;
    auto ret = dp_aux_cmd(gpu, con, req);

	if(!ret)
		return ret.error();

    for(size_t i = 0; i < len; i++) {
        req.size = 1;
        req.tx[0] = buf[i];

        ret = dp_aux_cmd(gpu, con, req);
		if(!ret)
			return ret.error();
    }

    req.request = DDI_AUX_I2C_READ;
    req.address = 0;
    req.size = 0;
    ret = dp_aux_cmd(gpu, con, req);
	if(!ret)
		return ret.error();

	return LilError::Success;
}

uint8_t dp_aux_native_read(struct LilGpu* gpu, LilConnector *con,  uint16_t addr) {
    AuxRequest req = {};
    req.request = DDI_AUX_NATIVE_READ;
    req.address = addr;
    req.size = 1;
    AuxResponse res = *dp_aux_cmd(gpu, con, req);

    return res.data[0];
}

void dp_aux_native_readn(struct LilGpu* gpu, LilConnector *con,  uint16_t addr, size_t n, void *buf) {
    AuxRequest req = {};
    req.request = DDI_AUX_NATIVE_READ;
    req.address = addr;
    req.size = n;
    AuxResponse res = *dp_aux_cmd(gpu, con, req);

    memcpy(buf, res.data, n);
}

void dp_aux_native_write(struct LilGpu* gpu, LilConnector *con,  uint16_t addr, uint8_t v) {
    AuxRequest req = {};
    req.request = DDI_AUX_NATIVE_WRITE;
    req.address = addr;
    req.size = 1;
    req.tx[0] = v;
    dp_aux_cmd(gpu, con, req);
}

void dp_aux_native_writen(struct LilGpu* gpu, LilConnector *con,  uint16_t addr, size_t n, void *buf) {
    AuxRequest req = {};
    req.request = DDI_AUX_NATIVE_WRITE;
    req.address = addr;
    req.size = n;
    memcpy(req.tx, buf, n);
    dp_aux_cmd(gpu, con, req);
}

#define DDC_SEGMENT 0x30
#define DDC_ADDR 0x50
#define EDID_SIZE 128

static void dp_aux_read_edid(struct LilGpu* gpu, LilConnector *con,  DisplayData* buf) {
    uint32_t block = 0;

    uint8_t segment = block / 2;
    dp_aux_i2c_write(gpu, con, DDC_SEGMENT, 1, &segment);

    uint8_t start = block * EDID_SIZE;
    dp_aux_i2c_write(gpu, con, DDC_ADDR, 1, &start);

    dp_aux_i2c_read(gpu, con, DDC_ADDR, EDID_SIZE, (uint8_t*)buf);
}

bool dp_dual_mode_read(LilGpu *gpu, LilConnector *con, uint8_t offset, void *buffer, size_t size) {
	if(gmbus_read(gpu, con, 0x40, offset, size, reinterpret_cast<uint8_t *>(buffer)) != LilError::Success)
		return false;

	return true;
}

bool lil_cfl_dp_is_connected (struct LilGpu* gpu, struct LilConnector* connector) {
    return true;
    
	if(connector->ddi_id == DDI_A) {
		return REG(DDI_BUF_CTL(DDI_A)) & DDI_BUF_CTL_DISPLAY_DETECTED;
	}

	uint32_t sfuse_mask = 0;
	uint32_t sde_isr_mask = 0;

	switch(connector->ddi_id) {
		case DDI_B: {
			sde_isr_mask = (1 << 4);
			sfuse_mask = (1 << 2);
			break;
		}
		case DDI_C: {
			sde_isr_mask = (1 << 5);
			sfuse_mask = (1 << 1);
			break;
		}
		case DDI_D: {
			sfuse_mask = (1 << 0);
			break;
		}
		default: {
			lil_panic("unhandled DDI id");
		}
	}

	if((REG(SFUSE_STRAP) & sfuse_mask) == 0) {
		return false;
	}

	return REG(SDEISR) & sde_isr_mask;
}

LilConnectorInfo lil_cfl_dp_get_connector_info (struct LilGpu* gpu, struct LilConnector* connector) {
    (void)connector;
    LilConnectorInfo ret = {};
    LilModeInfo* info = (LilModeInfo *) lil_malloc(sizeof(LilModeInfo) * 4);

    DisplayData edid = {};
    dp_aux_read_edid(gpu, connector, &edid);

    uint32_t edp_max_pixel_clock = 990 * gpu->boot_cdclk_freq;

    int j = 0;
    for(int i = 0; i < 4; i++) { // Maybe 4 Detailed Timings
        if(edid.detailTimings[i].pixelClock == 0)
            continue; // Not a timing descriptor

        if(connector->type == EDP && edp_max_pixel_clock && (edid.detailTimings[i].pixelClock * 10) > edp_max_pixel_clock) {
            lil_log(WARNING, "EDID: skipping detail timings %u: pixel clock (%u KHz) > max (%u KHz)\n", i, (edid.detailTimings[i].pixelClock * 10), edp_max_pixel_clock);
            continue;
        }

        edid_timing_to_mode(&edid, edid.detailTimings[i], &info[j++]);
    }

    ret.modes = info;
    ret.num_modes = j;
    return ret;
}

void lil_cfl_dp_set_state (struct LilGpu* gpu, struct LilConnector* connector, uint32_t state) {
	lil_panic("unimplemented");
}

uint32_t lil_cfl_dp_get_state (struct LilGpu* gpu, struct LilConnector* connector) {
	lil_panic("unimplemented");
}

static uint32_t get_pp_control(struct LilGpu* gpu) {
    uint32_t v = REG(PP_CONTROL);

    // Completely undocumented? Linux does it under the guise of "Unlocking registers??"
    if((v >> 16) != 0xABCD) {
        v &= ~(0xFFFFu << 16);
        v |= (0xABCDu << 16);
    }

    return v;
}

static void edp_panel_on(struct LilGpu* gpu, struct LilConnector* connector) {
    if(connector->type != EDP)
        return;

    uint32_t v = get_pp_control(gpu);
    v |= PP_CONTROL_ON | PP_CONTROL_BACKLIGHT | PP_CONTROL_RESET;

    REG(PP_CONTROL) = v;

    while(true) {
        v = REG(PP_STATUS);
        if(v & PP_STATUS_ON_STATUS && PP_STATUS_GET_SEQUENCE_PROGRESS(v) == PP_STATUS_SEQUENCE_NONE) // Wait until the display is on, and not in a power sequence
            break;
    }
}

static void edp_panel_off(struct LilGpu* gpu, struct LilConnector* connector) {
    if(connector->type != EDP)
        return;

    uint32_t v = get_pp_control(gpu);
    v &= ~(PP_CONTROL_ON | PP_CONTROL_RESET | PP_CONTROL_BACKLIGHT | PP_CONTROL_FORCE_VDD);

    REG(PP_CONTROL) = v;

    while(true) {
        v = REG(PP_STATUS);
        if((v & PP_STATUS_ON_STATUS) == 0 && PP_STATUS_GET_SEQUENCE_PROGRESS(v) == PP_STATUS_SEQUENCE_NONE) // Wait until the display is off, and not in a power sequence
            break;
    }
}

static void edp_panel_vdd_on(struct LilGpu* gpu, struct LilConnector* connector) {
    if(connector->type != EDP)
        return;

    uint32_t v = get_pp_control(gpu);
    v |= PP_CONTROL_FORCE_VDD;

    REG(PP_CONTROL) = v;
}

static void edp_panel_backlight_off(struct LilGpu* gpu, struct LilConnector* connector) {
    if(connector->type != EDP)
        return;

    uint32_t v = get_pp_control(gpu);
    v &= ~PP_CONTROL_BACKLIGHT;
    REG(PP_CONTROL) = v;

    /*volatile uint32_t* blc_ctl2 = (uint32_t*)(gpu->mmio_start + BLC_PWM_CPU_CTL2);
    volatile uint32_t* blc_ctl = (uint32_t*)(gpu->mmio_start + BLC_PWM_CPU_CTL);

    *blc_ctl2 &= ~(1u << 31);
    *blc_ctl &= ~(1u << 31);

    v = *blc_ctl;
    v &= ~0xFFFF; // Duty Cycle 0
    *blc_ctl = v;*/
}

static void dp_set_sink_power(struct LilGpu* gpu, struct LilConnector* connector, bool on) {
    uint8_t rev = dp_aux_native_read(gpu, connector, DPCD_REV);
    if(rev < 0x11)
        return;

    if(on) {
        lil_panic("TODO: Turn Sink on");
    } else {
        uint8_t downstream = dp_aux_native_read(gpu, connector, DOWNSTREAMPORT_PRESENT);
        if(rev == 0x11 && (downstream & 1)) {
            uint8_t port0 = dp_aux_native_read(gpu, connector, DOWNSTREAM_PORT0_CAP);
            if(port0 & (1 << 3)) { // HPD Aware
                return;
            }
        }

        dp_aux_native_write(gpu, connector, SET_POWER, DPCD_POWER_D3);
    }
}

void lil_cfl_dp_init(struct LilGpu* gpu, struct LilConnector* connector) {
	// here, we assume everything (power wells, DDIs, AUX) is already powered up by GOP
    // see Skylake PRMs Vol Display, AUX Programming sequence for details of what that includes

    uint8_t cap = dp_aux_native_read(gpu, connector, EDP_CONFIGURATION_CAP);
    connector->type = (cap != 0) ? EDP : DISPLAYPORT; // Hacky, but it should work on any eDP display that is semi-modern, better option is to parse VBIOS

    edp_panel_on(gpu, connector);
}

void lil_cfl_dp_disable(struct LilGpu* gpu, struct LilConnector* connector) {
    edp_panel_backlight_off(gpu, connector);
}

void lil_cfl_dp_post_disable(struct LilGpu* gpu, struct LilConnector* connector) {
    // uint32_t v = REG(VIDEO_DIP_CTL(connector->crtc->transcoder));
    // v &= ~((1 << 12) | (1 << 16) | (1 << 8) | (1 << 4) | (1 << 0) | (1 << 28) | (1 << 20));
    // REG(VIDEO_DIP_CTL(connector->crtc->transcoder)) = v;

    //dp_set_sink_power(gpu, connector, false);

    if(connector->crtc->transcoder != TRANSCODER_EDP) {
        REG(TRANS_CLK_SEL(connector->crtc->transcoder)) = 0;
    }

    bool wait = false;
    uint32_t v = REG(DDI_BUF_CTL(connector->crtc->pipe_id));
    if(v & DDI_BUF_CTL_ENABLE) {
        v &= ~DDI_BUF_CTL_ENABLE;
        REG(DDI_BUF_CTL(connector->crtc->pipe_id)) = v;
        wait = true;
    }

    v = REG(DP_TP_CTL(connector->crtc->pipe_id));
    v &= ~(DP_TP_CTL_ENABLE | DP_TP_CTL_TRAIN_MASK);
    v |= DP_TP_CTL_TRAIN_PATTERN1;
    REG(DP_TP_CTL(connector->crtc->pipe_id)) = v;

    if(wait)
        lil_sleep(1); // 518us - 1000us, just give it some time

    //edp_panel_off(gpu, connector);
    edp_panel_vdd_on(gpu, connector);

    // v = REG(DPLL_CTRL2);
    // v |= DPLL_CTRL2_DDI_CLK_OFF(connector->crtc->pipe_id);
    // REG(DPLL_CTRL2) = v;
}

static uint32_t dpcd_speed_to_dpll(uint8_t dpll, uint8_t rate) {
    if(rate == 0x14) return DPLL_CTRL1_LINK_RATE(dpll, 0);
    else if(rate == 0xA) return DPLL_CTRL1_LINK_RATE(dpll, 1);
    else if(rate == 0x6) return DPLL_CTRL1_LINK_RATE(dpll, 2);
    else lil_panic("Unknown DCPD Speed");
}

void lil_cfl_dp_pre_enable(struct LilGpu* gpu, struct LilConnector* connector) {
    /*edp_panel_on(gpu, connector);



    volatile uint32_t* mg_dp_ln0 = (uint32_t*)(gpu->mmio_start + MG_DP_MODE(0, connector->crtc->pipe_id));
    volatile uint32_t* mg_dp_ln1 = (uint32_t*)(gpu->mmio_start + MG_DP_MODE(1, connector->crtc->pipe_id));
    uint32_t ln0 = *mg_dp_ln0;
    uint32_t ln1 = *mg_dp_ln1;

    volatile uint32_t* pin_assign = (uint32_t)(gpu->mmio_start + PORT_TX_DFLEXPA(1)); // TODO: What is a FIA
    uint32_t pin_mask = (*pin_assign & DP_PIN_ASSIGN_MASK(1)) >> DP_PIN_ASSIGN_SHIFT(1);

    volatile uint32_t* ddi_buf_ctl = (uint32_t*)(gpu->mmio_start + DDI_BUF_CTL(connector->crtc->pipe_id));
    uint8_t width = DDI_BUF_CTL_DP_PORT_WIDTH(*ddi_buf_ctl) + 1;

    switch (pin_mask) {
        case 0:
            if(width == 1) {
                ln1 |= MG_DP_X1;
            } else {
                ln0 |= MG_DP_X2;
                ln1 |= MG_DP_X2;
            }
            break;
        case 1:
            if(width == 4) {
                ln0 |= MG_DP_X2;
                ln1 |= MG_DP_X2;
            }
            break;
        case 2:
            if(width == 2) {
                ln0 |= MG_DP_X2;
                ln1 |= MG_DP_X2;
            }
            break;
        case 3:
        case 5:
            if(width == 1) {
                ln0 |= MG_DP_X1;
                ln1 |= MG_DP_X1;
            } else {
                ln0 |= MG_DP_X2;
                ln1 |= MG_DP_X2;
            }
            break;
        case 4:
        case 6:
            if(width == 1) {
                ln0 |= MG_DP_X1;
                ln1 |= MG_DP_X1;
            } else {
                ln0 |= MG_DP_X2;
                ln1 |= MG_DP_X2;
            }
            break;
    }

    *mg_dp_ln0 = ln0;
    *mg_dp_ln1 = ln1;*/


   edp_panel_on(gpu, connector);

    uint32_t dpll_sel[] = {1, 3, 2};
    uint32_t dpll = dpll_sel[connector->crtc->pipe_id];
    if(connector->type == EDP)
        dpll = 0;

    uint32_t v = REG(DPLL_CTRL1);
    v |= DPLL_CTRL1_PROGRAM_ENABLE(dpll);
    v &= ~DPLL_CTRL1_HDMI_MODE(dpll); // DP mode
    v &= ~DPLL_CTRL1_LINK_RATE_MASK(dpll);
    v |= DPLL_CTRL1_LINK_RATE(dpll, dp_aux_native_read(gpu, connector, MAX_LINK_RATE));
    REG(DPLL_CTRL1) = v;

    if(dpll == 0) {
        REG(LCPLL1_CTL) |= LCPLL1_ENABLE;

        while((REG(LCPLL1_CTL) & LCPLL1_LOCK) == 0)
            ;
    } else if(dpll == 1) {
        REG(LCPLL2_CTL) |= LCPLL2_ENABLE;

        while((REG(DPLL_STATUS) & DPLL_STATUS_LOCK(dpll)) == 0)
            ;
    } else {
        lil_panic("TODO");
    }

    v = REG(DPLL_CTRL2);
    v &= ~(DPLL_CTRL2_DDI_CLK_OFF(connector->crtc->pipe_id) | DPLL_CTRL2_DDI_CLK_SEL_MASK(connector->crtc->pipe_id));
    v |= (DPLL_CTRL2_DDI_CLK_SEL(dpll, connector->crtc->pipe_id) | DPLL_CTRL2_DDI_SEL_OVERRIDE(connector->crtc->pipe_id)); // TODO: What is a DPLL ID?
    REG(DPLL_CTRL2) = v;

    v = REG(DP_TP_CTL(connector->crtc->pipe_id));
    v &= ~DP_TP_CTL_TRAIN_MASK;
    v |= (DP_TP_CTL_ENABLE | DP_TP_CTL_TRAIN_PATTERN1);
    REG(DP_TP_CTL(connector->crtc->pipe_id)) = v;

    REG(DDI_BUF_CTL(connector->crtc->pipe_id)) |= DDI_BUF_CTL_ENABLE;

    lil_sleep(5);

    if(dp_aux_native_read(gpu, connector, DPCD_REV) == 0x11 && dp_aux_native_read(gpu, connector, MAX_DOWNSPREAD) & NO_AUX_HANDSHAKE_LINK_TRAINING) {
        lil_sleep(2);
        v = REG(DDI_BUF_CTL(connector->crtc->pipe_id));
        v &= ~DP_TP_CTL_TRAIN_MASK;
        v |= DP_TP_CTL_TRAIN_PATTERN2;
        REG(DDI_BUF_CTL(connector->crtc->pipe_id)) = v;

        lil_sleep(2);
        v = REG(DDI_BUF_CTL(connector->crtc->pipe_id));
        v &= ~DP_TP_CTL_TRAIN_MASK;
        v |= DP_TP_CTL_TRAIN_PATTERN_IDLE;
        REG(DDI_BUF_CTL(connector->crtc->pipe_id)) = v;

        lil_sleep(2);
        v = REG(DDI_BUF_CTL(connector->crtc->pipe_id));
        v &= ~DP_TP_CTL_TRAIN_MASK;
        v |= DP_TP_CTL_TRAIN_PATTERN_NORMAL;
        REG(DDI_BUF_CTL(connector->crtc->pipe_id)) = v;
    } else {
        lil_panic("TODO: Full DP link training");
    }
}

#define DATA_N_MAX 0x800000
#define LINK_N_MAX 0x100000
#define M_N_MAX ((1 << 24) - 1)

static uint64_t round_n(uint64_t n, uint64_t n_max) {
    uint64_t rn = 0, rn2 = n_max;
    while(rn2 >= n) {
        rn = rn2;
        rn2 /= 2;
    }

    return rn;
}

static void cancel_m_n(uint64_t* m, uint64_t* n, uint64_t n_max) {
    const uint64_t orig_n = *n;

    *n = round_n(*n, n_max);
    *m = (*m * *n) / orig_n;

    while(*m > M_N_MAX) {
        *m /= 2;
        *n /= 2;
    }
}

LilDpMnValues lil_cfl_dp_calculate_mn(LilGpu* gpu, LilConnector *con, LilModeInfo* mode) {
    LilDpMnValues ret = {};

    uint64_t m = 3 * mode->bpc * mode->clock * 1000;

    uint8_t link_rate = dp_aux_native_read(gpu, con, MAX_LINK_RATE);
    uint64_t symbol_rate = 0;
    if(link_rate == 0x6)
        symbol_rate = 162000000;
    else if(link_rate == 0xA)
        symbol_rate = 270000000;
    else if(link_rate == 0x14)
        symbol_rate = 540000000;
    else
        lil_panic("Unknown DP Link Speed");

    uint64_t n = 8 * symbol_rate * (dp_aux_native_read(gpu, con, MAX_LANE_COUNT) & 0xF);
    cancel_m_n(&m, &n, DATA_N_MAX);
    ret.data_m = m;
    ret.data_n = n;

    m = mode->clock * 1000;
    n = symbol_rate;
    cancel_m_n(&m, &n, LINK_N_MAX);
    ret.link_m = m;
    ret.link_n = n;

    return ret;
}
