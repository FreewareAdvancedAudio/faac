/*
 * FAAM - Freeware Advanced Audio Muxer
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

/* Without iTunSMPB and with a shortened last packet in stts, priming + edit +
 * padding must still cover every decoded sample. */
#include <stdio.h>
#include <string.h>
#include "faam.h"
#include "mem_io.h"
#include "../../tests/faam_test_helpers.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

enum { PACKETS = 90, SHORT_PACKET = 88, PRIMING = 2112, EDIT = 88888,
       MOVIE_TIMESCALE = 1000, SEGMENT = 2016, SCALED_EDIT = (SEGMENT * 44100 + MOVIE_TIMESCALE / 2) / MOVIE_TIMESCALE };

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

int main(void)
{
    mem_file f = {0};
    faam_io io = mem_file_io(&f);
    faam_muxer_config cfg;
    faam_track_config tracks[8];
    CHECK(faam_muxer_config_init(&cfg, sizeof(cfg)) == FAAM_OK);
    faam_track_config track = { .struct_size = sizeof(track), .track_type = FAAM_TRACK_AUDIO,
                                .codec_id = FAAM_CODEC_AAC, .timescale = 44100, .sample_rate = 44100, .channels = 2 };
    unsigned char asc[] = {0x12, 0x10};
    track.codec_data = asc; track.codec_data_len = sizeof(asc);
    CHECK(test_append_track(&cfg, tracks, &track, NULL) == FAAM_OK);
    faam_muxer *m;
    CHECK(faam_muxer_open(&cfg, &io, &m) == FAAM_OK);
    unsigned char frame[100] = {0};
    for (unsigned i = 0; i < PACKETS; i++)
        CHECK(faam_muxer_write_frame(m, 1, frame, sizeof(frame), i == SHORT_PACKET ? 888 : 1024, 0, FAAM_FRAME_KEYFRAME) == FAAM_OK);
    faam_gapless_info gapless = { sizeof(faam_gapless_info), PRIMING, 0, EDIT };
    CHECK(faam_muxer_update(m, &(faam_muxer_update_params){ .struct_size = sizeof(faam_muxer_update_params),
                            .flags = FAAM_UPDATE_GAPLESS, .gapless = &gapless }) == FAAM_OK);
    CHECK(faam_muxer_finalize(m) == FAAM_OK);
    faam_muxer_close(&m);

    /* Hide iTunSMPB so the edit list alone describes the trimming. */
    unsigned hidden = 0;
    for (size_t i = 0; i + 8 <= f.len; i++)
        if (!memcmp(f.buf + i, "iTunSMPB", 8)) { memcpy(f.buf + i, "iTunXXXX", 8); hidden++; }
    CHECK(hidden == 1);

    /* Muxers with a coarser movie timescale make the segment scale to the track's. */
    size_t mvhd = 0, elst = 0;
    for (size_t i = 4; i + 8 <= f.len; i++) {
        if (!mvhd && !memcmp(f.buf + i, "mvhd", 4)) mvhd = i;
        if (!elst && !memcmp(f.buf + i, "elst", 4)) elst = i;
    }
    CHECK(mvhd && elst && f.buf[mvhd + 4] == 0 && f.buf[elst + 4] == 0);
    put_be32(f.buf + mvhd + 16, MOVIE_TIMESCALE);
    put_be32(f.buf + elst + 12, SEGMENT);

    f.pos = 0;
    faam_demuxer *d;
    CHECK(faam_demuxer_open(NULL, &io, &d) == FAAM_OK);
    faam_track_info info = { .struct_size = sizeof(info) };
    CHECK(faam_demuxer_get_track_info(d, 0, &info) == FAAM_OK);
    faam_gapless_info got = { .struct_size = sizeof(got) };
    CHECK(faam_demuxer_get_track_gapless(d, info.track_id, &got) == FAAM_OK);
    CHECK(got.encoder_delay == PRIMING);
    CHECK(got.total_samples == SCALED_EDIT);
    CHECK(got.end_padding == PACKETS * 1024 - PRIMING - SCALED_EDIT);
    faam_demuxer_close(&d);
    free(f.buf);
    return 0;
}
