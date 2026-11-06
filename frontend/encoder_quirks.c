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

/* priming is the fallback for encoders that never wrote gapless metadata; a
 * wrong guess trims real audio, so encoders that do write it keep it at 0.
 * priming_includes_decoder_delay marks encoders whose container priming
 * already counts the SBR decoder delay, which decoders otherwise add again. */
struct encoder_quirk {
    const char *match;      /* substring of the Encoder/Tool value */
    uint32_t priming;       /* samples per channel at the track rate */
    bool priming_includes_decoder_delay;
};

static const struct encoder_quirk encoder_quirks[] = {
    /* FAAC 1.x predates SBR and always primed 1024 samples; later versions
     * write iTunSMPB/elst. */
    { "FAAC 1.", 1024, false },
    { "libfaac", 1024, false },
    { "Nero AAC codec", 0, true },
};

static bool contains(const uint8_t *haystack, size_t haystack_len, const char *needle)
{
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || haystack_len < needle_len) return false;
    for (size_t i = 0; i + needle_len <= haystack_len; i++)
        if (!memcmp(haystack + i, needle, needle_len)) return true;
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

/* First table entry matched by a UTF-8 Encoder/Tool tag of an AAC track. */
static const struct encoder_quirk *find_quirk(const faam_demuxer *d, uint32_t track_id)
{
    if (!d || !is_aac_audio_track(d, track_id)) return NULL;

    uint32_t count = 0;
    if (faam_demuxer_get_num_metadata(d, &count) != FAAM_OK) return NULL;
    for (uint32_t i = 0; i < count; i++) {
        faam_metadata_entry e = { .struct_size = sizeof(e) };
        if (faam_demuxer_get_metadata_entry(d, i, &e) != FAAM_OK) continue;
        bool is_encoder = e.key == FAAM_METADATA_KEY_ENCODER ||
                          (e.name && (!strcmp(e.name, "Encoder") || !strcmp(e.name, "Tool")));
        if (!is_encoder || e.data_type != FAAM_METADATA_UTF8 || !e.data || !e.data_bytes) continue;
        for (size_t q = 0; q < sizeof(encoder_quirks) / sizeof(encoder_quirks[0]); q++)
            if (contains(e.data, e.data_bytes, encoder_quirks[q].match)) return &encoder_quirks[q];
    }
    return NULL;
}

uint32_t encoder_quirk_resolve_delay(const faam_demuxer *demuxer, uint32_t track_id,
                                     uint32_t explicit_delay, bool *inferred)
{
    if (inferred) *inferred = false;
    if (explicit_delay || !demuxer) return explicit_delay;

    const struct encoder_quirk *q = find_quirk(demuxer, track_id);
    if (!q || !q->priming) return 0;
    if (inferred) *inferred = true;
    return q->priming;
}

bool encoder_quirk_priming_includes_decoder_delay(const faam_demuxer *demuxer, uint32_t track_id)
{
    const struct encoder_quirk *q = find_quirk(demuxer, track_id);
    return q && q->priming_includes_decoder_delay;
}
