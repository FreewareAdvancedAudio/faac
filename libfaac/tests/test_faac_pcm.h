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

/* Deterministic 16-bit PCM for the encoder tests: two tones, seeded noise bursts and
 * full-scale clicks in near-silence, so block switching and rate control both engage. */
#ifndef TEST_FAAC_PCM_H
#define TEST_FAAC_PCM_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "../../tests/ftest.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline int16_t *pcm_make(uint32_t rate, uint32_t channels, uint32_t frames)
{
    int16_t *pcm = (int16_t *)malloc((size_t)frames * channels * sizeof(*pcm));
    uint32_t rng = 0x2545F491u;
    FT_REQUIRE(pcm);
    for (uint32_t i = 0; i < frames; i++) {
        double t = (double)i / rate;
        double v = 3000.0 * sin(2 * M_PI * 440.0 * t) + 1500.0 * sin(2 * M_PI * 3100.0 * t);
        if ((i / (rate / 4)) % 3 == 1)
            v += (double)(int32_t)(ft_rand(&rng) % 8001) - 4000.0;
        if (i % (rate / 3) < 48)
            v = (i & 1) ? 30000.0 : -30000.0;
        for (uint32_t c = 0; c < channels; c++)
            pcm[(size_t)i * channels + c] = (int16_t)(v * (c ? 0.7 : 1.0));
    }
    return pcm;
}

/* Full-scale white noise: the worst case for frame size. */
static inline int16_t *pcm_make_noise(uint32_t channels, uint32_t frames)
{
    int16_t *pcm = (int16_t *)malloc((size_t)frames * channels * sizeof(*pcm));
    uint32_t rng = 7;
    FT_REQUIRE(pcm);
    for (size_t i = 0; i < (size_t)frames * channels; i++)
        pcm[i] = (int16_t)((int)(ft_rand(&rng) % 60001) - 30000);
    return pcm;
}

#endif
