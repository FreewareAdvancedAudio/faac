/*
 * FAAC - Freeware Advanced Audio Coder
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

/* Unit tests for converting AAC stream configuration to ADTS headers. */
#include "../adts.h"
#undef NDEBUG
#include <assert.h>
#include <string.h>

static void test_header_roundtrip(void)
{
    AscInfo asc = { .object_type = 2, .sample_rate = 44100, .num_channels = 2 };
    adts_params params;
    assert(adts_params_from_asc(&asc, false, &params) == NULL);
    assert(params.profile == 1 && params.sr_idx == 4 && params.channel_cfg == 2
           && params.raw_blocks == 0);

    uint8_t header[ADTS_HEADER_SIZE];
    adts_write_header(&params, 100, header);
    const uint8_t expected[] = { 0xff, 0xf1, 0x50, 0x80, 0x0d, 0x7f, 0xfc };
    assert(memcmp(header, expected, sizeof(expected)) == 0);
    assert(adts_header_ok(header));
    assert(adts_frame_length(header) == sizeof(header) + 100);
    assert(adts_header_length(header) == ADTS_HEADER_SIZE);
    adts_params parsed;
    adts_parse_header(header, &parsed);
    assert(parsed.profile == params.profile && parsed.sr_idx == params.sr_idx
           && parsed.channel_cfg == params.channel_cfg && parsed.raw_blocks == 0);
}

static void test_adts_constraints(void)
{
    adts_params params;
    AscInfo asc = { .object_type = 2, .sample_rate = 12345, .num_channels = 2 };
    assert(adts_params_from_asc(&asc, false, &params) != NULL);
    asc.sample_rate = 44100;
    asc.object_type = 5;
    assert(adts_params_from_asc(&asc, false, &params) != NULL);
    assert(adts_params_from_asc(&asc, true, &params) == NULL && params.profile == 1);
    asc.object_type = 2;
    asc.num_channels = 0;
    assert(adts_params_from_asc(&asc, false, &params) != NULL);
    assert(adts_params_from_asc(&asc, true, &params) == NULL && params.channel_cfg == 0);
    asc.num_channels = 8;
    assert(adts_params_from_asc(&asc, false, &params) != NULL);
    assert(adts_params_from_asc(&asc, true, &params) == NULL && params.channel_cfg == 0);
}

int main(void)
{
    test_header_roundtrip();
    test_adts_constraints();
    return 0;
}
