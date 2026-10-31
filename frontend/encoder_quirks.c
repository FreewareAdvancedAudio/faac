/*
 * FAAC - Freeware Advanced Audio Decoder
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

#include <string.h>

#include "encoder_quirks.h"

/* Only encoders that never wrote gapless metadata belong here; priming is
 * encoder-specific and a wrong guess trims real audio. */
static const struct {
    const char *match;      /* substring of the Encoder/Tool value */
    uint32_t priming;       /* samples per channel at the track rate */
} encoder_priming_quirks[] = {
    /* FAAC 1.x predates SBR and always primed 1024 samples; later versions
     * write iTunSMPB/elst. */
    { "FAAC 1.", 1024 },
    { "libfaac", 1024 },
};

static bool contains(const uint8_t *haystack, size_t haystack_len, const char *needle)
{
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || haystack_len < needle_len) return false;
    for (size_t i = 0; i + needle_len <= haystack_len; i++)
        if (!memcmp(haystack + i, needle, needle_len)) return true;
    return false;
}

bool encoder_quirk_priming_bytes(const uint8_t *data, uint32_t len, uint32_t *priming)
{
    if (!data || !len || !priming) return false;
    for (size_t i = 0; i < sizeof(encoder_priming_quirks) / sizeof(encoder_priming_quirks[0]); i++) {
        if (contains(data, len, encoder_priming_quirks[i].match)) {
            *priming = encoder_priming_quirks[i].priming;
            return true;
        }
    }
    return false;
}

/* Encoder tags are file-level, so the AAC constant must not leak onto other
 * codecs; track_id 0 means the first audio track. */
static bool is_aac_audio_track(const faam_demuxer *d, uint32_t track_id)
{
    uint32_t num_tracks = 0, total_tracks = 0;
    if (faam_demuxer_get_num_tracks(d, &num_tracks, &total_tracks) != FAAM_OK) return false;
    (void)total_tracks;

    if (track_id == 0) {
        for (uint32_t t = 0; t < num_tracks; t++) {
            faam_track_info info = { .struct_size = sizeof(info) };
            if (faam_demuxer_get_track_info(d, t, &info) != FAAM_OK) continue;
            if (info.track_type == FAAM_TRACK_AUDIO) { track_id = info.track_id; break; }
        }
    }
    if (track_id == 0) return false;

    for (uint32_t t = 0; t < num_tracks; t++) {
        faam_track_info info = { .struct_size = sizeof(info) };
        if (faam_demuxer_get_track_info(d, t, &info) != FAAM_OK) continue;
        if (info.track_id == track_id)
            return info.track_type == FAAM_TRACK_AUDIO && info.codec_id == FAAM_CODEC_AAC;
    }
    return false;
}

uint32_t encoder_quirk_resolve_delay(const faam_demuxer *demuxer, uint32_t track_id,
                                     uint32_t explicit_delay, bool *inferred)
{
    if (inferred) *inferred = false;
    if (explicit_delay || !demuxer) return explicit_delay;
    if (!is_aac_audio_track(demuxer, track_id)) return 0;

    uint32_t count = 0;
    if (faam_demuxer_get_num_metadata(demuxer, &count) != FAAM_OK) return 0;
    for (uint32_t i = 0; i < count; i++) {
        faam_metadata_entry e = { .struct_size = sizeof(e) };
        if (faam_demuxer_get_metadata_entry(demuxer, i, &e) != FAAM_OK) continue;
        bool is_encoder = e.key == FAAM_METADATA_KEY_ENCODER ||
                          (e.name && (!strcmp(e.name, "Encoder") || !strcmp(e.name, "Tool")));
        if (!is_encoder || e.data_type != FAAM_METADATA_UTF8) continue;
        uint32_t priming;
        if (encoder_quirk_priming_bytes(e.data, e.data_bytes, &priming)) {
            if (inferred) *inferred = true;
            return priming;
        }
    }
    return 0;
}
