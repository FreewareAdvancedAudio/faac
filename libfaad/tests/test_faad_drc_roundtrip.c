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

/* What libfaac writes with a DRC profile is what libfaad applies: a loud
 * passage is cut and the quiet one after it boosted, measured as the energy
 * change against a decode with the decoder's DRC scales at zero. Without a
 * profile the encoder writes no dynamic_range_info at all. */
#include <math.h>
#include <stdio.h>
#include "faac.h"
#include "faad3.h"
#include "../../tests/ftest.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum { RATE = 48000, CH = 2, LOUD_S = 6, QUIET_S = 10, FRAME = 1024, MAX_AU = 1024 };
#define LOUD_AMP  16000.0   /* about -6 dBFS */
#define QUIET_AMP 330.0     /* about -40 dBFS */

typedef struct {
    uint8_t *data;
    size_t start[MAX_AU], len[MAX_AU];
    uint32_t aus;
} stream;

static void encode(stream *s, enum faac_drc_profile profile)
{
    faac_params p;
    faac_encoder *enc = NULL;
    faac_encoder_info info = { .struct_size = sizeof(info) };
    const uint32_t total = (LOUD_S + QUIET_S) * RATE;
    FT_REQUIRE(faac_params_init(&p, sizeof(p)) == FAAC_OK);
    p.sample_rate = RATE;
    p.channels = CH;
    p.object_type = FAAC_OBJ_LC;
    p.bit_rate = 128000;
    p.stream_format = FAAC_STREAM_ADTS;
    p.input_format = FAAC_INPUT_16BIT;
    p.drc_profile = profile;
    FT_REQUIRE(faac_encoder_open(&p, &enc) == FAAC_OK);
    FT_REQUIRE(faac_encoder_get_info(enc, &info) == FAAC_OK);

    int16_t *in = (int16_t *)malloc((size_t)total * CH * sizeof(*in));
    FT_REQUIRE(in);
    for (uint32_t i = 0; i < total; i++) {
        double amp = i < (uint32_t)LOUD_S * RATE ? LOUD_AMP : QUIET_AMP;
        int16_t v = (int16_t)(amp * sin(2.0 * M_PI * 997.0 * i / RATE));
        in[(size_t)i * CH] = in[(size_t)i * CH + 1] = v;
    }

    size_t cap = (size_t)info.max_output_bytes * MAX_AU;
    s->data = (uint8_t *)malloc(cap);
    FT_REQUIRE(s->data);
    size_t used = 0;
    s->aus = 0;
    for (uint32_t done = 0, last = 0; !last;) {
        uint32_t got = 0, n = total - done < info.frame_samples ? total - done : info.frame_samples;
        if (n)
            FT_REQUIRE(faac_encoder_encode(enc, in + (size_t)done * CH, n * CH, s->data + used,
                                           (uint32_t)(cap - used), &got) == FAAC_OK);
        else {
            FT_REQUIRE(faac_encoder_encode(enc, NULL, 0, s->data + used, (uint32_t)(cap - used), &got) == FAAC_OK);
            last = !got;
        }
        done += n;
        if (got) {
            FT_REQUIRE(s->aus < MAX_AU);
            s->start[s->aus] = used;
            s->len[s->aus++] = got;
            used += got;
        }
    }
    FT_REQUIRE(faac_encoder_close(&enc) == FAAC_OK);
    free(in);
}

typedef struct {
    double energy[MAX_AU];     /* per access unit, float PCM */
    uint32_t drc_info_aus;     /* access units that carried dynamic_range_info */
    uint32_t ref_level_aus;    /* ... with a program reference level */
    uint32_t ref_level;        /* last reported, quarter-dB below full scale */
    uint32_t aus;
} decoded;

static void decode(const stream *s, unsigned scale, decoded *d)
{
    faad_config cfg;
    faad_decoder *dec = NULL;
    faad_stream_info si = { .struct_size = sizeof(si) };
    FT_REQUIRE(faad_config_init(&cfg, sizeof(cfg)) == FAAD_OK);
    cfg.output_format = FAAD_OUTPUT_FLOAT;
    cfg.drc_cut = (uint8_t)scale;
    cfg.drc_boost = (uint8_t)scale;
    FT_REQUIRE(faad_decoder_open(&cfg, NULL, 0, &dec) == FAAD_OK);
    FT_REQUIRE(faad_decoder_get_info(dec, &si) == FAAD_OK);
    float *pcm = (float *)malloc(si.max_output_bytes);
    FT_REQUIRE(pcm);
    memset(d, 0, sizeof(*d));
    for (uint32_t a = 0; a < s->aus; a++) {
        uint32_t used = 0, written = 0, flags = 0;
        FT_REQUIRE(faad_decode_frame(dec, s->data + s->start[a], (uint32_t)s->len[a], &used,
                                     pcm, si.max_output_bytes, &written, &flags) == FAAD_OK);
        FT_REQUIRE(faad_decoder_get_info(dec, &si) == FAAD_OK);
        for (uint32_t i = 0; i < written / sizeof(float); i++)
            d->energy[a] += (double)pcm[i] * pcm[i];
        d->drc_info_aus += (flags & FAAD_FRAME_DRC_INFO) != 0;
        if (si.flags & FAAD_STREAM_DRC_REF_LEVEL) {
            d->ref_level_aus++;
            d->ref_level = (uint32_t)lroundf(-4.0f * si.drc_ref_level);
        }
    }
    d->aus = s->aus;
    free(pcm);
    FT_REQUIRE(faad_decoder_close(&dec) == FAAD_OK);
}

/* Energy change in dB over access units [from, to). */
static double change_db(const decoded *base, const decoded *drc, uint32_t from, uint32_t to)
{
    double b = 0, g = 0;
    for (uint32_t a = from; a < to; a++) {
        b += base->energy[a];
        g += drc->energy[a];
    }
    FT_REQUIRE(b > 0 && g > 0);
    return 10.0 * log10(g / b);
}

int main(void)
{
    faac_library_info enc = { .struct_size = sizeof(enc) };
    faad_library_info dec = { .struct_size = sizeof(dec) };
    FT_REQUIRE(faac_get_library_info(&enc) == FAAC_OK);
    FT_REQUIRE(faad_get_library_info(&dec) == FAAD_OK);
    if (enc.max_channels < CH || dec.max_channels < CH) return 77;

    stream with = {0}, without = {0};
    decoded base, drc, off;
    const uint32_t onset = LOUD_S * RATE / FRAME;

    encode(&with, FAAC_DRC_FILM);
    decode(&with, 0, &base);
    decode(&with, 127, &drc);
    FT_REQUIRE(drc.aus > onset + 450);

    /* Every access unit but the encoder's priming carries the element and the
     * default -23 LUFS reference, written as 92 quarter-dB steps. */
    FT_CHECK(drc.drc_info_aus + 4 >= drc.aus);
    FT_CHECK(drc.ref_level_aus + 4 >= drc.aus);
    FT_CHECK_INT(drc.ref_level, 92);

    /* Film cuts a -6 dBFS tone by roughly 9 dB and boosts -40 dBFS by 5 dB; the
     * windows sit past the attack and release settling. */
    double loud = change_db(&base, &drc, onset - 120, onset - 20);
    double quiet = change_db(&base, &drc, onset + 400, onset + 440);
    fprintf(stderr, "drc roundtrip: loud %.2f dB, quiet %.2f dB\n", loud, quiet);
    FT_CHECK(loud < -4.0 && loud > -14.0);
    FT_CHECK(quiet > 2.0 && quiet < 8.0);

    encode(&without, FAAC_DRC_OFF);
    decode(&without, 127, &off);
    FT_CHECK_INT(off.drc_info_aus, 0);
    FT_CHECK_INT(off.ref_level_aus, 0);

    free(with.data);
    free(without.data);
    return ft_done();
}
