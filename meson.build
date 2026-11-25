project('faac', 'c',
    version: '3.0.0',
    default_options: [
        'default_library=both',
        'buildtype=release',
        'warning_level=3',
        'c_std=gnu11,c11',
        'b_lto=true'
    ])

# Shared-library ABI versions, separate from the project release version.
# These are the single source for the SONAMEs. The public version macros in the
# installed headers, and so the version each library reports, are the project
# release version; Meson checks that they match it. libfaac already shipped
# SONAMEs 0-2 with other interfaces; the other libraries start at 0.
library_abi_versions = {
    'FAAC': '3.0.0',
    'FAAD': '0.0.0',
    'FAAM': '0.0.0',
    'FAAR': '0.0.0',
}
shared_library_args = {}

# Windows otherwise installs shared-library DLLs in bindir. Static frontends
# do not need those DLLs at runtime, so keep them with the other libraries.
if get_option('frontend') and get_option('frontend-static')
    shared_library_args += {
        'install_dir': get_option('libdir'),
    }
endif

cc = meson.get_compiler('c')
if cc.get_argument_syntax() == 'msvc'
    # _s variants buy nothing here and aren't portable; suppress instead.
    add_global_arguments('-D_CRT_SECURE_NO_WARNINGS', language: 'c')
endif

# Static frontends link whole archives' objects; per-function sections let the
# linker drop what no frontend calls. Shared builds already hide and strip it.
gc_link_args = []
if get_option('frontend-static')
    if cc.has_link_argument('-Wl,--gc-sections')
        gc_link_args += ['-Wl,--gc-sections']
    elif cc.has_link_argument('-Wl,-dead_strip')
        gc_link_args += ['-Wl,-dead_strip']
    endif
    if cc.has_argument('-ffunction-sections')
        add_project_arguments('-ffunction-sections', '-fdata-sections', language: 'c')
    endif
endif

config_h = configuration_data()
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())

foreach h: ['getopt.h', 'immintrin.h']
    config_h.set('HAVE_' + h.underscorify().to_upper(), cc.has_header(h))
endforeach


libm = cc.find_library('m', required: false)

cpu_family = target_machine.cpu_family()

config_h.set('HAVE_SSE2', (cpu_family in ['x86', 'x86_64']) and config_h.get('HAVE_IMMINTRIN_H'))
encoder_sbr = get_option('encoder-sbr')
config_h.set10('FAAC_ENCODER_SBR', encoder_sbr)
encoder_ps = get_option('encoder-ps') and get_option('max-channels') >= 2 and encoder_sbr
config_h.set10('FAAC_ENCODER_PS', encoder_ps)
encoder_drc = get_option('encoder-drc')
config_h.set10('FAAC_ENCODER_DRC', encoder_drc)

config_h.set('FAAC_SBR_DECIMATION', get_option('sbr-decimation'))

config_h.set('FAAC_STATS', get_option('stats'))
config_h.set('FAAD_STATS', get_option('stats'))

config_h.set('MAX_CHANNELS', get_option('max-channels'))

c_args = []
if cc.get_argument_syntax() == 'msvc'
    c_args += ['-DWIN32', '-DNDEBUG', '-D_WINDOWS', '-D_USRDLL']
endif

configure_file(
    output: 'config.h',
    configuration: config_h
)

# Force config.h into every TU instead of relying on each file's own include.
if cc.get_argument_syntax() == 'msvc'
    add_project_arguments('/FI' + meson.current_build_dir() / 'config.h', language: 'c')
else
    add_project_arguments('-include', meson.current_build_dir() / 'config.h', language: 'c')
endif

# sqrtf() inlines to one instruction only when it need not set errno. Nothing
# here reads errno after a math call, so drop it: otherwise every coefficient
# of the scalar quantizer (no SSE2 path) calls into libm.
extra_args = cc.get_supported_arguments([
    '-Werror=unused-parameter',
    '/we4100',
    '-fno-math-errno',
])
add_project_arguments(extra_args, language: 'c')

inc = include_directories('include')
# Scale test time limits for slow targets. A test setup marked as default also
# applies to a plain `meson test`, which is what packaging builds run, and it
# covers tests that rely on Meson's 30 s default as well as explicit timeouts.
test_timeout_multiplier = get_option('test-timeout-multiplier')
if test_timeout_multiplier == 0
    fast_cpu = host_machine.cpu_family() in ['x86', 'x86_64', 'arm', 'aarch64', 'ppc64', 'loongarch64']
    # Binaries run under an emulator (qemu-user) are slow whatever their CPU.
    emulated = meson.is_cross_build() and meson.can_run_host_binaries() and host_machine.cpu_family() != build_machine.cpu_family()
    if fast_cpu and host_machine.endian() == 'little' and host_machine.system() != 'gnu' and not emulated
        test_timeout_multiplier = 1
    else
        test_timeout_multiplier = 10
    endif
endif
# A subproject must not claim the default setup: Meson allows one per build.
if test_timeout_multiplier > 1 and not meson.is_subproject()
    add_test_setup('scaled_timeouts',
        timeout_multiplier: test_timeout_multiplier,
        env: {'FAAC_TEST_TIMEOUT_SCALE': test_timeout_multiplier.to_string()},
        is_default: true)
endif

# Tests that take the longest on slow targets are started first (higher priority
# runs earlier), so they do not all pile up at the end of a run and leave most
# workers idle while two long tests finish. Only start order changes. Tiers, not
# ranks: put a test in `long` if it takes about a minute under qemu-user on m68k,
# `medium` for about half a minute.
long_test_priority = 100
medium_test_priority = 50

subdir('docs')
subdir('include')
subdir('common')
subdir('libfaar')

if get_option('encoder')
    subdir('libfaac')
endif
subdir('libfaam')

if get_option('decoder')
    subdir('libfaad')
endif

subdir('cmake')

if get_option('frontend')
    subdir('frontend')
endif

# Fuzzing is part of this, and needs the libraries and frontend defined above.
subdir('tests')
