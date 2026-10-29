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

/* Eight-channel streams carry their layout in a program config element, so
 * the channel an element lands on comes from PCE tags, not packet order. Each
 * input channel gets its own tone; the decode must put every tone back on the
 * channel it came from, whether the PCE arrives in the ASC, in the first ADTS
 * frame, or in a later repeat that a joining decoder starts from. */
#include <math.h>
#include <stdio.h>
#include "faac.h"
#include "faad3.h"
#include "../../tests/ftest.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum { RATE = 48000, CH = 8, FRAMES = RATE * 2, MAX_AU = 256, PCE_REPEAT = 32 };

/* WAVE order FL FR FC LFE BL BR SL SR; the LFE tone stays inside its band. */
static const double tone_hz[CH] = { 600, 1000, 1500, 70, 2200, 3000, 4100, 5200 };
#define AMP 6000.0

typedef struct {
    uint8_t *data;
    size_t len, au_start[MAX_AU], au_len[MAX_AU];
    uint32_t aus;
    uint8_t asc[14];
    uint32_t asc_size;
} stream;

static void encode(stream *s, enum faac_stream_format format)
{
    faac_params p;
    faac_encoder *enc = NULL;
    faac_encoder_info info = { .struct_size = sizeof(info) };
    FT_REQUIRE(faac_params_init(&p, sizeof(p)) == FAAC_OK);
    p.sample_rate = RATE;
    p.channels = CH;
    p.object_type = FAAC_OBJ_LC;
    p.bit_rate = 64000u * CH;
    p.stream_format = format;
    p.input_format = FAAC_INPUT_16BIT;
    p.channel_layout = FAAC_LAYOUT_WAV;
    FT_REQUIRE(faac_encoder_open(&p, &enc) == FAAC_OK);
    FT_REQUIRE(faac_encoder_get_info(enc, &info) == FAAC_OK);
    s->asc_size = info.asc_size;
    memcpy(s->asc, info.asc, sizeof(s->asc));

    int16_t *in = (int16_t *)malloc((size_t)FRAMES * CH * sizeof(*in));
    FT_REQUIRE(in);
    for (uint32_t i = 0; i < FRAMES; i++)
        for (uint32_t c = 0; c < CH; c++)
            in[(size_t)i * CH + c] = (int16_t)(AMP * sin(2.0 * M_PI * tone_hz[c] * i / RATE));

    size_t cap = (size_t)info.max_output_bytes * MAX_AU;
    s->data = (uint8_t *)malloc(cap);
    FT_REQUIRE(s->data);
    s->len = 0;
    s->aus = 0;
    for (uint32_t done = 0, last = 0; !last;) {
        uint32_t got = 0, n = FRAMES - done < info.frame_samples ? FRAMES - done : info.frame_samples;
        if (n)
            FT_REQUIRE(faac_encoder_encode(enc, in + (size_t)done * CH, n * CH, s->data + s->len,
                                           (uint32_t)(cap - s->len), &got) == FAAC_OK);
        else {
            FT_REQUIRE(faac_encoder_encode(enc, NULL, 0, s->data + s->len, (uint32_t)(cap - s->len), &got) == FAAC_OK);
            last = !got;
        }
        done += n;
        if (got) {
            FT_REQUIRE(s->aus < MAX_AU);
            s->au_start[s->aus] = s->len;
            s->au_len[s->aus++] = got;
            s->len += got;
        }
    }
    FT_REQUIRE(faac_encoder_close(&enc) == FAAC_OK);
    free(in);
}

/* Amplitude of the sinusoid at `hz` over frames [from, to) of one channel (Goertzel). */
static double tone_amplitude(const int16_t *pcm, uint32_t channels, uint32_t channel,
                             size_t from, size_t to, double hz)
{
    double w = 2.0 * M_PI * hz / RATE, c = 2.0 * cos(w), s1 = 0, s2 = 0;
    for (size_t i = from; i < to; i++) {
        double s0 = pcm[i * channels + channel] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return 2.0 * sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / (double)(to - from);
}

/* Decodes access units [first, aus) and checks that tone c sits on output
 * channel c at about its input level and on no other channel. */
static void decode_and_check(const stream *s, enum faad_stream_format format, uint32_t first, const char *name)
{
    faad_config cfg;
    faad_decoder *dec = NULL;
    faad_stream_info si = { .struct_size = sizeof(si) };
    FT_REQUIRE(faad_config_init(&cfg, sizeof(cfg)) == FAAD_OK);
    cfg.stream_format = format;
    cfg.output_format = FAAD_OUTPUT_16BIT;
    FT_REQUIRE(faad_decoder_open(&cfg, format == FAAD_STREAM_RAW ? s->asc : NULL,
                                 format == FAAD_STREAM_RAW ? s->asc_size : 0, &dec) == FAAD_OK);
    size_t out_cap = (size_t)FRAMES * 4 * CH * sizeof(int16_t), out_len = 0;
    uint8_t *out = (uint8_t *)malloc(out_cap);
    FT_REQUIRE(out);
    for (uint32_t a = first; a < s->aus; a++) {
        uint32_t used = 0, written = 0, flags = 0;
        FT_REQUIRE(faad_decode_frame(dec, s->data + s->au_start[a], (uint32_t)s->au_len[a], &used,
                                     out + out_len, (uint32_t)(out_cap - out_len), &written, &flags) == FAAD_OK);
        FT_CHECK_INT(used, s->au_len[a]);
        out_len += written;
    }
    FT_REQUIRE(faad_decoder_get_info(dec, &si) == FAAD_OK);
    FT_CHECK_INT(si.channels, CH);
    FT_CHECK_INT(si.channel_mask, 0x63f);
    size_t frames = out_len / sizeof(int16_t) / CH;
    FT_REQUIRE(frames > (size_t)RATE);
    const int16_t *pcm = (const int16_t *)out;
    size_t from = RATE / 2, to = RATE;
    for (uint32_t c = 0; c < CH; c++) {
        double own = tone_amplitude(pcm, CH, c, from, to, tone_hz[c]);
        if (fabs(20.0 * log10(own / AMP)) > 3.0)
            fprintf(stderr, "%s: channel %u carries %.0f of %.0f\n", name, c, own, AMP);
        FT_CHECK(fabs(20.0 * log10(own / AMP)) <= 3.0);
        for (uint32_t other = 0; other < CH; other++) {
            if (other == c) continue;
            double leak = tone_amplitude(pcm, CH, other, from, to, tone_hz[c]);
            if (leak > own * 0.05)
                fprintf(stderr, "%s: tone %u leaks onto channel %u\n", name, c, other);
            FT_CHECK(leak <= own * 0.05);
        }
    }
    FT_REQUIRE(faad_decoder_close(&dec) == FAAD_OK);
    free(out);
}

/* The PCE of an ADTS frame follows the 7-byte header: element id 5 (PCE), tag 0. */
static int has_pce(const stream *s, uint32_t au)
{
    return s->au_len[au] > 8 && s->data[s->au_start[au] + 7] == 0xa0;
}

int main(void)
{
    faac_library_info enc = { .struct_size = sizeof(enc) };
    faad_library_info dec = { .struct_size = sizeof(dec) };
    FT_REQUIRE(faac_get_library_info(&enc) == FAAC_OK);
    FT_REQUIRE(faad_get_library_info(&dec) == FAAD_OK);
    if (enc.max_channels < CH || dec.max_channels < CH) return 77;

    stream adts = { 0 }, raw = { 0 }, mpeg2 = { 0 };
    encode(&adts, FAAC_STREAM_ADTS);
    FT_REQUIRE(adts.aus > PCE_REPEAT + 8);
    FT_CHECK(has_pce(&adts, 0));
    FT_CHECK(!has_pce(&adts, 1));
    FT_CHECK(has_pce(&adts, PCE_REPEAT));
    decode_and_check(&adts, FAAD_STREAM_ADTS, 0, "ADTS from the first frame");
    decode_and_check(&adts, FAAD_STREAM_ADTS, PCE_REPEAT, "ADTS joined at a repeated PCE");

    encode(&raw, FAAC_STREAM_RAW);
    FT_CHECK(raw.asc_size > 2);
    decode_and_check(&raw, FAAD_STREAM_RAW, 0, "RAW with the ASC");

    /* MPEG-2 ADTS never builds an ASC; its in-band PCE must still be complete. */
    encode(&mpeg2, FAAC_STREAM_ADTS_MPEG2);
    FT_CHECK(mpeg2.asc_size == 0);
    FT_CHECK(has_pce(&mpeg2, 0));
    FT_CHECK_MEM(mpeg2.data + mpeg2.au_start[0] + 7, adts.data + adts.au_start[0] + 7, 9);

    free(adts.data);
    free(raw.data);
    free(mpeg2.data);
    return ft_done();
}
