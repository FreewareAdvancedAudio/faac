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

/* Unit tests for the PS payload parser and hybrid stereo processing. */
#include "../faad_internal.h"
#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void put_bits(uint8_t *data, uint32_t *position, uint32_t value, unsigned count)
{
    for (unsigned i = 0; i < count; i++) {
        unsigned shift = count - i - 1;
        if ((value >> shift) & 1u)
            data[*position >> 3] |= (uint8_t)(1u << (7 - (*position & 7)));
        (*position)++;
    }
}

static void test_layout(unsigned mode, bool expected_is34)
{
    struct faad_decoder *dec = calloc(1, sizeof(*dec));
    PSState *ps = calloc(1, sizeof(*ps));
    assert(dec != NULL && ps != NULL);
    dec->ps = ps;

    uint8_t payload[4] = { 0 };
    uint32_t bits = 0;
    put_bits(payload, &bits, 1, 1);       /* PS header present */
    put_bits(payload, &bits, 1, 1);       /* IID enabled */
    put_bits(payload, &bits, mode, 3);    /* 20- or 34-band layout */
    put_bits(payload, &bits, 1, 1);       /* ICC enabled */
    put_bits(payload, &bits, mode, 3);    /* matching ICC parameter layout */
    put_bits(payload, &bits, 1, 1);       /* extensions enabled */
    put_bits(payload, &bits, 0, 1);       /* fixed frame class */
    put_bits(payload, &bits, 0, 2);       /* zero coded envelopes: reuse defaults */
    put_bits(payload, &bits, 1, 4);      /* eight extension bits */
    for (int i = 0; i < 4; i++)
        put_bits(payload, &bits, 1, 2);   /* unknown extension: skip its payload */

    BitReader br;
    bits_init(&br, payload, sizeof(payload));
    ps_read_data(dec, &br, bits);
    assert(dec->ps_present);
    assert(ps->start && ps->num_env == 1 && ps->border[1] == 31);
    assert(ps->is34 == expected_is34);
    for (int b = 0; b < ps->nr_icc_par; b++)
        ps->icc_par[0][b] = 5; /* decorrelated ICC mode */

    init_ps_tables();
    ps_frame_begin(dec, 64);
    float stereo_delta = 0.0f;
    for (int n = 0; n < PS_QMF_SLOTS; n++) {
        float X[64][2], L[64][2], R[64][2];
        for (int k = 0; k < 64; k++) {
            X[k][0] = sinf((float)(n * 64 + k) * 0.037f);
            X[k][1] = cosf((float)(n * 64 + k) * 0.053f);
        }
        ps_frame_input(dec, n, X);
        ps_slot(dec, n, X, L, R);
        for (int k = 0; k < 64; k++) {
            assert(isfinite(L[k][0]) && isfinite(L[k][1]));
            assert(isfinite(R[k][0]) && isfinite(R[k][1]));
            stereo_delta += fabsf(L[k][0] - R[k][0]) + fabsf(L[k][1] - R[k][1]);
        }
    }
    assert(stereo_delta > 0.1f);
    free(ps);
    free(dec);
}

static void test_invalid_headers_reset_ps_state(void)
{
    struct faad_decoder *dec = calloc(1, sizeof(*dec));
    PSState *ps = calloc(1, sizeof(*ps));
    assert(dec != NULL && ps != NULL);
    dec->ps = ps;
    ps->start = true;

    uint8_t payload[3] = { 0 };
    uint32_t bits = 0;
    put_bits(payload, &bits, 1, 1);       /* PS header present */
    put_bits(payload, &bits, 1, 1);       /* IID enabled */
    put_bits(payload, &bits, 7, 3);      /* invalid IID mode */
    put_bits(payload, &bits, 0, 1);       /* ICC disabled */
    put_bits(payload, &bits, 0, 1);       /* extensions disabled */
    put_bits(payload, &bits, 0, 1);       /* fixed frame class */
    put_bits(payload, &bits, 0, 2);      /* no envelopes */
    BitReader br;
    bits_init(&br, payload, sizeof(payload));
    ps_read_data(dec, &br, bits);
    assert(!ps->start && !dec->ps_present);

    memset(payload, 0, sizeof(payload));
    bits = 0;
    put_bits(payload, &bits, 1, 1);       /* PS header present */
    put_bits(payload, &bits, 0, 1);       /* IID disabled */
    put_bits(payload, &bits, 0, 1);       /* ICC disabled */
    put_bits(payload, &bits, 0, 1);       /* extensions disabled */
    put_bits(payload, &bits, 1, 1);       /* variable frame class */
    put_bits(payload, &bits, 1, 2);       /* two envelopes */
    put_bits(payload, &bits, 20, 5);
    put_bits(payload, &bits, 10, 5);      /* descending borders are invalid */
    bits_init(&br, payload, sizeof(payload));
    ps_read_data(dec, &br, bits);
    assert(!ps->start && !dec->ps_present);
    for (size_t i = 0; i < sizeof(ps->iid_par); i++)
        assert(((const uint8_t *)ps->iid_par)[i] == 0);

    free(ps);
    free(dec);
}

static void test_parameter_deltas_and_previous_envelope(void)
{
    struct faad_decoder *dec = calloc(1, sizeof(*dec));
    PSState *ps = calloc(1, sizeof(*ps));
    assert(dec != NULL && ps != NULL);
    dec->ps = ps;

    uint8_t payload[64] = { 0 };
    uint32_t bits = 0;
    put_bits(payload, &bits, 1, 1); /* PS header present */
    put_bits(payload, &bits, 1, 1); /* IID enabled */
    put_bits(payload, &bits, 0, 3); /* 10-band IID */
    put_bits(payload, &bits, 0, 1); /* ICC disabled */
    put_bits(payload, &bits, 0, 1); /* extensions disabled */
    put_bits(payload, &bits, 1, 1); /* variable frame class */
    put_bits(payload, &bits, 1, 2); /* two envelopes */
    put_bits(payload, &bits, 12, 5);
    put_bits(payload, &bits, 20, 5); /* last envelope ends before slot 31 */

    const SBRHuffEntry *df_zero = &ps_huff_iid_df[PS_HUFF_IID_DF_OFFSET];
    const SBRHuffEntry *dt_zero = &ps_huff_iid_dt[PS_HUFF_IID_DT_OFFSET];
    put_bits(payload, &bits, 0, 1); /* frequency differential */
    for (int i = 0; i < 10; i++) put_bits(payload, &bits, df_zero->code, df_zero->len);
    put_bits(payload, &bits, 1, 1); /* time differential from envelope 0 */
    for (int i = 0; i < 10; i++) put_bits(payload, &bits, dt_zero->code, dt_zero->len);

    BitReader br;
    bits_init(&br, payload, sizeof(payload));
    ps_read_data(dec, &br, bits);
    assert(dec->ps_present && ps->num_env == 3);
    assert(ps->border[1] == 12 && ps->border[2] == 20 && ps->border[3] == 31);
    for (int e = 0; e < 3; e++)
        for (int b = 0; b < ps->nr_iid_par; b++)
            assert(ps->iid_par[e][b] == 0);

    /* A zero-envelope frame must hold the previous frame's last parameters. */
    memset(payload, 0, sizeof(payload));
    bits = 0;
    put_bits(payload, &bits, 1, 1); /* PS header present */
    put_bits(payload, &bits, 1, 1); /* IID remains enabled */
    put_bits(payload, &bits, 0, 3);
    put_bits(payload, &bits, 0, 1); /* ICC disabled */
    put_bits(payload, &bits, 0, 1); /* extensions disabled */
    put_bits(payload, &bits, 0, 1); /* fixed frame class */
    put_bits(payload, &bits, 0, 2); /* no envelopes */
    ps->num_env = 2;
    for (int b = 0; b < PS_NR_PAR; b++) ps->iid_par[1][b] = (int8_t)(b - 10);
    bits_init(&br, payload, sizeof(payload));
    ps_read_data(dec, &br, bits);
    assert(dec->ps_present && ps->num_env == 1 && ps->border[1] == 31);
    for (int b = 0; b < PS_NR_PAR; b++)
        assert(ps->iid_par[0][b] == (int8_t)(b - 10));

    free(ps);
    free(dec);
}

int main(void)
{
    test_layout(1, false);
    test_layout(2, true);
    test_invalid_headers_reset_ps_state();
    test_parameter_deltas_and_previous_envelope();
    return 0;
}
