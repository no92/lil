project('lil', 'c')

add_project_arguments(['-Wno-unused-function', '-Wno-unused-variable'], language: 'c')

c_std = 'gnu2x'

sources = files(
    'src/edid.c',
    'src/gmbus.c',
    'src/intel.c',
    'src/lvds.c',
    'src/pci.c',

    'src/ivy_bridge/crtc.c',
    'src/ivy_bridge/gtt.c',
    'src/ivy_bridge/interrupt.c',
    'src/ivy_bridge/ivb.c',
    'src/ivy_bridge/plane.c',

    'src/coffee_lake/cfl.c',
    'src/coffee_lake/crtc.c',
    'src/coffee_lake/dp.c',
    'src/coffee_lake/gtt.c',
    'src/coffee_lake/plane.c',

	'src/vbt/fw_cfg.c',
	'src/vbt/parsing.c',
	'src/vbt/vbt.c',
)

include_directories = include_directories('include')
library = static_library('lil', sources, include_directories: include_directories, pic: false)
dependency = declare_dependency(link_with: library, include_directories: include_directories)
