# Unit tests; encoder tests reach libfaac only through the static archive.
if is_variable('libfaac_static')
    foreach t : ['chunking', 'frames', 'bitstream']
        test('test_unit_faac_' + t,
            executable('test_faac_' + t, 'test_faac_' + t + '.c',
                include_directories: ['..', '../..', '../../include'],
                link_with: libfaac_static,
                dependencies: [libm]))
    endforeach

    test('test_unit_faac_tns',
        executable('test_faac_tns', 'test_faac_tns.c',
            include_directories: ['..', '../..', '../../include', '../../common'],
            c_args: common_args['c_args'] + ['-DFAAC_STATIC'],
            link_with: libfaac_static,
            dependencies: [libm]))
endif

# quantize.c is included into the test so the static kernels are reachable.
if is_variable('libfaac_static') and config_h.get('HAVE_SSE2')
    test('test_unit_faac_quantize',
        executable('test_faac_quantize', 'test_faac_quantize.c',
            include_directories: ['..', '../..', '../../include', common_inc],
            c_args: common_args['c_args'],
            link_with: [libfaac_static, quantize_sse],
            dependencies: [libm, codec_dep]))
endif
