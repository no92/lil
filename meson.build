project('lil', ['c', 'cpp'], default_options: ['warning_level=2'])

if meson.get_compiler('cpp').get_id() != 'gcc' or meson.get_compiler('cpp').version().version_compare('<13')
	error('At least GCC G++ 13 is needed to compile lil.')
endif

add_project_arguments(['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wimplicit-fallthrough'], language: ['c', 'cpp'])
add_project_arguments(['-Werror=incompatible-pointer-types'], language: ['c'])
add_project_arguments(['-std=gnu++2b'], language: ['cpp'])

c_std = 'gnu2x'

sources = files(
    'src/edid.c',
    'src/gmbus.cpp',
    'src/intel.c',
    'src/lvds.c',
    'src/pci.c',

    'src/ivy_bridge/crtc.c',
    'src/ivy_bridge/gtt.c',
    'src/ivy_bridge/interrupt.c',
    'src/ivy_bridge/ivb.c',
    'src/ivy_bridge/plane.c',

    'src/kaby_lake/avi.c',
    'src/kaby_lake/cdclk.c',
    'src/kaby_lake/crtc.c',
    'src/kaby_lake/ddi.c',
    'src/kaby_lake/ddi-translations.c',
    'src/kaby_lake/disable.c',
    'src/kaby_lake/dp.c',
    'src/kaby_lake/dp-aux.cpp',
    'src/kaby_lake/edp.c',
    'src/kaby_lake/encoder.c',
    'src/kaby_lake/gtt.c',
    'src/kaby_lake/hdmi.c',
    'src/kaby_lake/hpd.c',
    'src/kaby_lake/kbl.c',
    'src/kaby_lake/link-training.c',
    'src/kaby_lake/pch.c',
    'src/kaby_lake/pci.c',
    'src/kaby_lake/pcode.c',
    'src/kaby_lake/pipe.c',
    'src/kaby_lake/plane.c',
    'src/kaby_lake/pll.c',
    'src/kaby_lake/setup.c',
    'src/kaby_lake/transcoder.c',

	'src/vbt/parsing.c',
	'src/vbt/vbt.c',
)

util_vbt_sources = files(
	'utils/vbt/vbt.c',
)

include_directories = include_directories('include')
library = static_library('lil', sources, include_directories: include_directories, pic: false)
lil_dep = declare_dependency(link_with: library, include_directories: include_directories)

if get_option('build_utils')
	util_vbt = executable('vbt', util_vbt_sources, dependencies: dependency, native: true, install: true)
endif
