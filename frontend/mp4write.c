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

/* Large-file semantics for the fopen() below, not only for the seeks in cli_io.c: on 32-bit
   POSIX a plain fopen() fails with EFBIG once an M4A passes 2 GiB. */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define access _access
#define W_OK 2
#else
#include <unistd.h>
#endif

#include "faam.h"
#include "cli_io.h"
#include "mp4write.h"
#include "endian.h"
#include "cover_art.h"
#ifdef _WIN32
#include "charset.h"
#include "cli_io.h"
#endif

enum {
    MP4_TRACK_ID = 1, /* single-track file */

    MP4_IO_BUFSIZE = 65536, /* stdio buffer for the mdat write path, see mp4_open() */
};

/* The muxer is created on the first frame, not in mp4_open(): the sample rate,
   channel count and sample size arrive after mp4_open(), and it fixes them
   when it writes the file header. Everything that is only needed for the
   moov (decoder config, tags, gapless, creation time) is applied in
   mp4_finish(), because the encoder only reports its AudioSpecificConfig then. */
static struct {
    uint32_t samplerate;
    uint32_t channels;
    uint32_t bits;
    bool constant_rate;
    bool faststart;

    struct {
        const uint8_t *data;
        unsigned long size;
    } asc;

    FILE *fout;
    faam_muxer *mux;
    uint32_t num_metadata;

    uint32_t creation_time;
    const char *encoder;
    const char *language;
    const char *tags[MP4TAG_COUNT];
    bool compilation;
    uint16_t trackno;
    uint16_t ntracks;
    uint16_t discno;
    uint16_t ndiscs;

    struct {
        const uint8_t *data;
        uint32_t size;
    } cover;

    struct {
        bool present;
        uint32_t priming;
        uint32_t padding;
        uint64_t original_samples;
    } gapless;

    faam_metadata_entry *custom;
    faam_metadata_entry *entries;
    uint32_t customcnt;
    uint32_t customcap;
} g_mp4 = { 0 };

/* Resets per-output-file write state: the muxer and the open output handle.
   Tag config (named metadata, custom tags via mp4_add_custom_tag()) is set by
   the caller and may happen before *or* after mp4_open() depending on the
   frontend, so it must survive this reset -- only mp4_close() clears it,
   once the caller is actually done with this muxer session. */
static void reset_write_state(void) {
    faam_muxer_close(&g_mp4.mux);
    if (g_mp4.fout) {
        fclose(g_mp4.fout);
        g_mp4.fout = NULL;
    }
    memset(&g_mp4.gapless, 0, sizeof(g_mp4.gapless));
}

int mp4_open(const char *path, bool overwrite) {
    reset_write_state(); /* in case of a retry after a failed previous mp4_open() */

#ifdef _WIN32
    if (!overwrite && win32_access_utf8(path, 0) == 0) return 1;
    g_mp4.fout = win32_fopen_utf8(path, g_mp4.faststart ? "w+b" : "wb");
#else
    if (!overwrite && access(path, 0) == 0) return 1;
    g_mp4.fout = fopen(path, g_mp4.faststart ? "w+b" : "wb");
#endif
    if (!g_mp4.fout) return 1;
    setvbuf(g_mp4.fout, NULL, _IOFBF, MP4_IO_BUFSIZE);
    return 0;
}

static bool open_muxer(void) {
    if (g_mp4.mux) return true;
    if (!g_mp4.fout) return false;

    faam_muxer_config cfg;
    if (faam_muxer_config_init(&cfg, sizeof(cfg)) != FAAM_OK) return false;
    if (g_mp4.constant_rate) cfg.flags |= FAAM_MUXER_CONSTANT_RATE;
    if (g_mp4.faststart) cfg.flags |= FAAM_MUXER_FASTSTART;

    faam_track_config track = { .struct_size = sizeof(track) };
    track.track_type = FAAM_TRACK_AUDIO;
    track.codec_id = FAAM_CODEC_AAC;
    track.track_id = MP4_TRACK_ID;
    track.timescale = g_mp4.samplerate;
    track.sample_rate = g_mp4.samplerate;
    track.channels = g_mp4.channels;
    track.audio_sample_size = (uint16_t)g_mp4.bits;
    cfg.tracks = &track;
    cfg.num_tracks = 1;

    /* The muxer only writes, except for faststart's move; offsets are 64-bit so an M4A past 2 GiB
       still patches its mdat size and stco. */
    faam_io io = cli_faam_io(g_mp4.fout);
    if (!g_mp4.faststart) io.read = NULL;
    if (faam_muxer_open(&cfg, &io, &g_mp4.mux) != FAAM_OK) return false;

    return true;
}

/* ISO/IEC 14496-1 spells a constant-rate stream as maxBitrate == avgBitrate. */
void mp4_set_constant_rate(bool constant) { g_mp4.constant_rate = constant; }

/* Faststart reads back what it wrote to move it, hence the update mode in mp4_open(). */
void mp4_set_faststart(bool faststart) { g_mp4.faststart = faststart; }

void mp4_set_creation_time(uint32_t t) { g_mp4.creation_time = t; }

void mp4_set_format(uint32_t samplerate, uint32_t channels, uint32_t bits) {
    g_mp4.samplerate = samplerate;
    g_mp4.channels = channels;
    g_mp4.bits = bits;
}

void mp4_set_decoder_config(const uint8_t *asc, unsigned long size) {
    g_mp4.asc.data = asc;
    g_mp4.asc.size = size;
}

void mp4_set_encoder(const char *value) { g_mp4.encoder = value; }

void mp4_set_tag(mp4_tag_id_t id, const char *value) {
    if (id < MP4TAG_COUNT)
        g_mp4.tags[id] = value;
}


void mp4_set_language(const char *lang) { g_mp4.language = lang; }

void mp4_set_compilation(bool flag) { g_mp4.compilation = flag; }

void mp4_set_track(uint16_t num, uint16_t total) {
    g_mp4.trackno = num;
    g_mp4.ntracks = total;
}

void mp4_set_disc(uint16_t num, uint16_t total) {
    g_mp4.discno = num;
    g_mp4.ndiscs = total;
}

void mp4_set_cover(const uint8_t *data, uint32_t size) {
    g_mp4.cover.data = data;
    g_mp4.cover.size = size;
}

void mp4_set_gapless(uint32_t priming, uint32_t padding, uint64_t original_samples) {
    g_mp4.gapless.present = true;
    g_mp4.gapless.priming = priming;
    g_mp4.gapless.padding = padding;
    g_mp4.gapless.original_samples = original_samples;
}

int mp4_add_custom_tag(const char *name, const char *value) {
    if (g_mp4.customcnt >= g_mp4.customcap) {
        uint32_t new_cap = g_mp4.customcap ? g_mp4.customcap * 2 : 8;
        void *tmp = realloc(g_mp4.custom, (size_t)new_cap * sizeof(*g_mp4.custom));
        if (!tmp) return -1;
        g_mp4.custom = tmp;
        g_mp4.customcap = new_cap;
    }
    /* Callers free their strings once this returns, so keep our own copies. */
    char *name_copy = strdup(name);
    char *value_copy = strdup(value);
    if (!name_copy || !value_copy) {
        free(name_copy);
        free(value_copy);
        return -1;
    }
    g_mp4.custom[g_mp4.customcnt].struct_size = sizeof(g_mp4.custom[0]);
    g_mp4.custom[g_mp4.customcnt].mean = "faac"; /* master's namespace */
    g_mp4.custom[g_mp4.customcnt].name = name_copy;
    g_mp4.custom[g_mp4.customcnt].data = value_copy;
    g_mp4.custom[g_mp4.customcnt].key = FAAM_METADATA_KEY_FREEFORM;
    g_mp4.custom[g_mp4.customcnt].data_type = FAAM_METADATA_UTF8;
    g_mp4.custom[g_mp4.customcnt].data_bytes = (uint32_t)strlen(value_copy);
    g_mp4.custom[g_mp4.customcnt].locale = 0;
    g_mp4.custom[g_mp4.customcnt].flags = 0;
    g_mp4.customcnt++;
    return 0;
}

int mp4_write_frame(const uint8_t *data, uint32_t size, uint32_t samples) {
    if (!open_muxer()) return -1;
    return faam_muxer_write_frame(g_mp4.mux, MP4_TRACK_ID, data, size, samples, 0, FAAM_FRAME_KEYFRAME) == FAAM_OK ? 0 : -1;
}

/* Returns 0 on success, 1 on failure (mirroring mp4_open()'s convention). */
int mp4_finish(void) {
    if (!open_muxer()) return 1;

    g_mp4.num_metadata = 0;
    free(g_mp4.entries);
    g_mp4.entries = NULL;
    if (g_mp4.customcnt > UINT32_MAX - MP4TAG_COUNT - 8) return 1;
    g_mp4.entries = calloc((size_t)g_mp4.customcnt + MP4TAG_COUNT + 8, sizeof(*g_mp4.entries));
    if (!g_mp4.entries) return 1;
    static const uint32_t keys[MP4TAG_COUNT] = {
        FAAM_METADATA_KEY_ARTIST, FAAM_METADATA_KEY_ARTIST_SORT,
        FAAM_METADATA_KEY_COMPOSER, FAAM_METADATA_KEY_COMPOSER_SORT,
        FAAM_METADATA_KEY_TITLE, FAAM_METADATA_KEY_ALBUM,
        FAAM_METADATA_KEY_ALBUM_ARTIST, FAAM_METADATA_KEY_ALBUM_ARTIST_SORT,
        FAAM_METADATA_KEY_ALBUM_SORT, FAAM_METADATA_KEY_GENRE,
        FAAM_METADATA_KEY_YEAR, FAAM_METADATA_KEY_COMMENT
    };
    for (unsigned i = 0; i <= MP4TAG_COUNT; i++) {
        const char *text = i == MP4TAG_COUNT ? g_mp4.encoder : g_mp4.tags[i];
        if (!text) continue;
        g_mp4.entries[g_mp4.num_metadata++] = (faam_metadata_entry){ .struct_size = sizeof(faam_metadata_entry),
            .key = i == MP4TAG_COUNT ? FAAM_METADATA_KEY_ENCODER : keys[i],
            .data_type = FAAM_METADATA_UTF8, .data = text, .data_bytes = (uint32_t)strlen(text) };
    }
    uint8_t values[3][8] = {{0}};
    values[0][0] = 1;
    write_u16_be(values[1] + 2, g_mp4.trackno); write_u16_be(values[1] + 4, g_mp4.ntracks);
    write_u16_be(values[2] + 2, g_mp4.discno); write_u16_be(values[2] + 4, g_mp4.ndiscs);
    const uint32_t number_keys[] = { FAAM_METADATA_KEY_COMPILATION, FAAM_METADATA_KEY_TRACK, FAAM_METADATA_KEY_DISC };
    const bool present[] = { g_mp4.compilation, g_mp4.trackno != 0, g_mp4.discno != 0 };
    const uint32_t sizes[] = {1, 8, 8};
    for (unsigned i = 0; i < 3; i++) if (present[i])
        g_mp4.entries[g_mp4.num_metadata++] = (faam_metadata_entry){ .struct_size = sizeof(faam_metadata_entry),
            .key = number_keys[i], .data_type = i == 0 ? FAAM_METADATA_INTEGER : FAAM_METADATA_IMPLICIT,
            .data = values[i], .data_bytes = sizes[i] };
    if (g_mp4.cover.data && g_mp4.cover.size) {
        uint8_t type = faam_detect_cover_type(g_mp4.cover.data, g_mp4.cover.size);
        if (!type) return 1;
        g_mp4.entries[g_mp4.num_metadata++] = (faam_metadata_entry){ .struct_size = sizeof(faam_metadata_entry),
            .key = FAAM_METADATA_KEY_COVER_ART, .data_type = type,
            .data = g_mp4.cover.data, .data_bytes = g_mp4.cover.size };
    }
    for (uint32_t i = 0; i < g_mp4.customcnt; i++) g_mp4.entries[g_mp4.num_metadata++] = g_mp4.custom[i];

    faam_gapless_info gapless = {
        .struct_size = sizeof(gapless),
        .encoder_delay = g_mp4.gapless.priming,
        .end_padding = g_mp4.gapless.padding,
        .total_samples = g_mp4.gapless.original_samples,
    };
    faam_muxer_update_params update = { .struct_size = sizeof(update) };
    update.metadata = g_mp4.entries;
    update.num_metadata = g_mp4.num_metadata;
    update.flags = FAAM_UPDATE_METADATA | FAAM_UPDATE_CODEC_DATA | FAAM_UPDATE_LANGUAGE | FAAM_UPDATE_CREATION_TIME;
    update.track_id = MP4_TRACK_ID;
    update.codec_data = g_mp4.asc.data;
    update.codec_data_len = (uint32_t)g_mp4.asc.size;
    update.creation_time = g_mp4.creation_time;
    for (unsigned i = 0; g_mp4.language && i < 3 && g_mp4.language[i]; i++) update.language[i] = g_mp4.language[i];
    if (g_mp4.gapless.present) {
        update.flags |= FAAM_UPDATE_GAPLESS;
        update.gapless = &gapless;
    }
    if (faam_muxer_update(g_mp4.mux, &update) != FAAM_OK) return 1;
    return faam_muxer_finalize(g_mp4.mux) == FAAM_OK ? 0 : 1;
}

/* Custom tags (mp4_add_custom_tag()) are freed here, not in
   reset_write_state(), so mp4_open() doesn't wipe them if a caller sets
   them before the first open. A future multi-file/batch session reusing
   this muxer across encodes would need to re-add custom tags after each
   mp4_close() -- they don't survive it. */
int mp4_close(void) {
    reset_write_state();
    for (uint32_t i = 0; i < g_mp4.customcnt; i++) {
        free((void *)g_mp4.custom[i].name);
        free((void *)g_mp4.custom[i].data);
    }
    free(g_mp4.entries);
    g_mp4.entries = NULL;
    free(g_mp4.custom);
    g_mp4.custom = NULL;
    g_mp4.customcnt = 0;
    g_mp4.customcap = 0;
    return 0;
}

static bool track_info(faam_muxer_info *info) {
    info->struct_size = sizeof(*info);
    return g_mp4.mux && faam_muxer_get_info(g_mp4.mux, MP4_TRACK_ID, info) == FAAM_OK;
}

uint32_t mp4_frame_count(void) {
    faam_muxer_info info;
    return track_info(&info) ? info.frame_count : 0;
}

uint64_t mp4_sample_count(void) {
    faam_muxer_info info;
    return track_info(&info) ? info.duration_ticks : 0;
}

uint32_t mp4_max_bitrate(void) {
    faam_muxer_info info;
    return track_info(&info) ? info.max_bitrate : 0;
}

uint32_t mp4_avg_bitrate(void) {
    faam_muxer_info info;
    return track_info(&info) ? info.avg_bitrate : 0;
}

uint16_t mp4_max_frame_size(void) {
    faam_muxer_info info;
    return track_info(&info) ? (uint16_t)info.max_frame_size : 0;
}
