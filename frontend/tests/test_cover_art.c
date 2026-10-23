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

/* Unit tests for cover art format detection. */
#include "cover_art.h"
#undef NDEBUG
#include <assert.h>

static const uint8_t png[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
static const uint8_t jpg[] = {0xff, 0xd8, 0xff, 0xe0};
static const uint8_t gif87[] = {'G', 'I', 'F', '8', '7', 'a'};
static const uint8_t gif89[] = {'G', 'I', 'F', '8', '9', 'a'};
static const uint8_t bmp[58] = {
    [0] = 'B', [1] = 'M', [2] = 58, [10] = 54, [14] = 40,
    [18] = 1, [22] = 1, [26] = 1, [28] = 24, [34] = 4
};
static const uint8_t bmp_core[30] = {
    [0] = 'B', [1] = 'M', [2] = 30, [10] = 26, [14] = 12,
    [18] = 1, [20] = 1, [22] = 1, [24] = 24
};

static void test_formats(void)
{
    static const uint8_t junk[] = {1, 2, 3, 4, 5, 6, 7, 8};
    assert(faam_detect_cover_type(png, sizeof(png)) == FAAM_METADATA_PNG);
    assert(faam_detect_cover_type(jpg, sizeof(jpg)) == FAAM_METADATA_JPEG);
    assert(faam_detect_cover_type(gif87, sizeof(gif87)) == FAAM_METADATA_GIF);
    assert(faam_detect_cover_type(gif89, sizeof(gif89)) == FAAM_METADATA_GIF);
    assert(faam_detect_cover_type(bmp, sizeof(bmp)) == FAAM_METADATA_BMP);
    assert(faam_detect_cover_type(bmp_core, sizeof(bmp_core)) == FAAM_METADATA_BMP);
    assert(faam_detect_cover_type(junk, sizeof(junk)) == 0);
    assert(faam_detect_cover_type(NULL, 0) == 0);
}

/* Truncated signatures and malformed BMP fields must not be accepted. */
static void test_rejected(void)
{
    assert(!faam_detect_cover_type(jpg, 2));
    assert(!faam_detect_cover_type(png, 7));
    assert(!faam_detect_cover_type(gif89, 5));
    static const uint32_t short_sizes[] = {2, 25, 53, 57}; /* 57: declared size exceeds data */
    for (unsigned i = 0; i < sizeof(short_sizes) / sizeof(short_sizes[0]); i++)
        assert(!faam_detect_cover_type(bmp, short_sizes[i]));

    static const struct { unsigned at; uint8_t value; } edits[] = {
        {6, 1},   /* reserved field */
        {10, 20}, /* pixels overlap headers */
        {10, 59}, /* pixels outside file */
        {14, 16}, /* unsupported DIB */
        {18, 0},  /* zero width */
        {22, 0},  /* zero height */
        {26, 2},  /* planes */
        {28, 3},  /* bit depth */
    };
    for (unsigned i = 0; i < sizeof(edits) / sizeof(edits[0]); i++) {
        uint8_t artwork[sizeof(bmp)];
        memcpy(artwork, bmp, sizeof(artwork));
        artwork[edits[i].at] = edits[i].value;
        assert(!faam_detect_cover_type(artwork, sizeof(artwork)));
    }
    uint8_t oversized[sizeof(bmp)];
    memcpy(oversized, bmp, sizeof(oversized));
    memset(oversized + 14, 255, 4);
    assert(!faam_detect_cover_type(oversized, sizeof(oversized)));
}

int main(void)
{
    test_formats();
    test_rejected();
    return 0;
}
