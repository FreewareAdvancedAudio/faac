test_common_endian = executable('test_endian', 'test_endian.c',
    include_directories: common_inc)
test('test_unit_common_endian', test_common_endian)

fft_c_args = ['-DCOMMON_PREFIX=faad_']
if not get_option('decoder-dab')
    fft_c_args += ['-DFAAD_DISABLE_DAB']
endif
test_common_fft = executable('test_fft', 'test_fft.c',
    c_args: fft_c_args,
    include_directories: common_inc,
    dependencies: [libm, codec_dep])
test('test_unit_common_fft', test_common_fft)
