if get_option('decoder')
    # Compile the raw 960-sample fuzz harness in regular builds, without libFuzzer's main.
    static_library('fuzz_faad_raw960', 'fuzz_faad_raw960.c',
        include_directories: ['../../include'], install: false)
endif
