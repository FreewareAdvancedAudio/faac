/*
 * FAAD - Freeware Advanced Audio Decoder
 * Copyright (C) 2026 Nils Schimmelmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

/*
 * libFuzzer harness for libfaad's ADTS and RAW decoder API.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "faad3.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2)
        return 0;

    faad_config cfg;
    faad_config_init(&cfg, sizeof(cfg));

    uint8_t mode = data[0];
    const uint8_t *payload = data + 1;
    size_t payload_len = size - 1;

    cfg.stream_format = (mode & 1) ? FAAD_STREAM_ADTS : FAAD_STREAM_RAW;
    cfg.output_format = (enum faad_output_format)(1 + ((mode >> 1) & 3)); /* 16BIT, 24BIT, 32BIT, FLOAT */
    cfg.downmix_mode  = (enum faad_downmix_mode)((mode >> 3) & 3);

    faad_decoder *dec = NULL;
    static const uint8_t raw_asc[] = { 0x12, 0x10 }; /* AAC-LC, 44.1 kHz, stereo, 1024 samples */
    faad_status st = faad_decoder_open(&cfg,
        cfg.stream_format == FAAD_STREAM_RAW ? raw_asc : NULL,
        cfg.stream_format == FAAD_STREAM_RAW ? sizeof(raw_asc) : 0, &dec);
    if (st != FAAD_OK)
        return 0;

    faad_stream_info info;
    info.struct_size = sizeof(info);
    (void)faad_decoder_get_info(dec, &info);

    uint32_t pcm_capacity = info.max_output_bytes;
    uint8_t *pcm = malloc(pcm_capacity);
    if (!pcm) { faad_decoder_close(&dec); return 0; }
    size_t pos = 0;
    int frames = 0;

    while (pos < payload_len && frames < 128) {
        uint32_t consumed = 0;
        uint32_t written = 0;
        uint32_t flags = 0;

        st = faad_decode_frame(dec, payload + pos, (uint32_t)(payload_len - pos),
                               &consumed, pcm, pcm_capacity, &written, &flags);

        if (flags & FAAD_FRAME_FORMAT_CHANGED) {
            info.struct_size = sizeof(info);
            (void)faad_decoder_get_info(dec, &info);
        }

        if (st != FAAD_OK && st != FAAD_ERR_NEED_MORE_DATA && st != FAAD_ERR_SYNC_LOST && st != FAAD_ERR_DECODE_FAILED) {
            break;
        }

        if (consumed == 0) {
            pos++;
        } else {
            pos += consumed;
        }
        frames++;

        if (frames % 32 == 0) {
            (void)faad_decoder_flush(dec);
        }
    }

    free(pcm);
    faad_decoder_close(&dec);
    return 0;
}
