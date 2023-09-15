# Kaby Lake driver
This driver aims to support cards from Gen9 up.

Feature Matrix:
| Code Name    | Generation | Status | Notes                              |
|--------------|------------|--------|------------------------------------|
| Sky Lake     | Gen9       | N/A    |                                    |
| Broxton      | Gen9       | N/A    | Basically Sky Lake on a diet.      |
| Gemini Lake  | Gen9.?     | WIP    | Borrows a lot from Gen9 and Gen9.5 |
| Kaby Lake    | Gen9.5     | WIP    |                                    |
| Coffee Lake  | Gen9.5     | N/A    |                                    |
| Amber Lake   | Gen9.5     | N/A    |                                    |
| Whiskey Lake | Gen9.5     | N/A    |                                    |
| Ice Lake     | Gen11      | N/A    |                                    |

Index:
- N/A: no work has been done; there is no support yet.
- WIP: work in progress; there has been work done, but not everything works yet. 
- WORKS: works; generally "stable"

---
# Gemini Lake
Gemini Lake sits between Broxton and Kaby Lake. It differs from Kaby Lake in the following ways:
- There are no port detection strap registers (SFUSE_STRAP).
- HDMI has a quirk: for some reason one needs to wait some time after having disabled the HDMI port, otherwise weird things happen
  - i915 does this

---
# Kaby Lake
The origin of this driver. TODO
