# Config values are substituted by Meson instead of probed at find_package()
# time, so the installed package always describes this build. Plain
# configure_file keeps cmake out of the build requirements.

built_components = ['faam', 'faar']
libm_components = ['faar']
if get_option('encoder')
    built_components += ['faac']
    libm_components += ['faac']
endif
if get_option('decoder')
    built_components += ['faad3']
    libm_components += ['faad3']
endif

cmake_conf = configuration_data()
cmake_conf.set('FAAC_BUILT_COMPONENTS', ';'.join(built_components))
# Only libraries that really link libm, and only where the platform has one.
cmake_conf.set('FAAC_LIBM_COMPONENTS', libm.found() ? ';'.join(libm_components) : '')
cmake_conf.set('FAAC_LIBDIR', get_option('libdir'))
cmake_conf.set('FAAC_INCLUDEDIR', get_option('includedir'))

cmake_install_dir = get_option('libdir') / 'cmake' / 'faac'

# The config sits in <libdir>/cmake/faac, so climbing out of it reaches the prefix.
prefix_rel = ['..', '..']
foreach part: get_option('libdir').split('/')
    prefix_rel += ['..']
endforeach
cmake_conf.set('FAAC_PREFIX_REL', '/'.join(prefix_rel))
cmake_conf.set('FAAC_VERSION', meson.project_version())
cmake_conf.set('FAAC_VERSION_MAJOR', meson.project_version().split('.')[0])

configure_file(
    input: 'faac-config.cmake.in',
    output: 'faacConfig.cmake',
    configuration: cmake_conf,
    install: true,
    install_dir: cmake_install_dir,
)
configure_file(
    input: 'faac-config-version.cmake.in',
    output: 'faacConfigVersion.cmake',
    configuration: cmake_conf,
    install: true,
    install_dir: cmake_install_dir,
)
