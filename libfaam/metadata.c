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

/* Shared ilst serialization keeps frontend and retrofit tag layouts aligned. */
#include <stdio.h>
#include "endian.h"
#include "libfaam_internal.h"

#define FREEFORM_KEY FAAM_FOURCC('-', '-', '-', '-')

static const char *entry_mean(const char *mean) {
    return mean && *mean ? mean : "com.apple.iTunes";
}

static bool faam_metadata_key_matches(const faam_metadata_entry *entry, uint32_t key,
                               const char *mean, const char *name) {
    if (entry->key != key) return false;
    if (key != FREEFORM_KEY) return true;
    return entry->name && name && !strcmp(entry->name, name) &&
           !strcmp(entry_mean(entry->mean), entry_mean(mean));
}

static bool faam_metadata_has_key(const faam_entry_list *meta, uint32_t key, const char *mean, const char *name) {
    if (!meta) return false;
    for (uint32_t i = 0; i < meta->count; i++)
        if (faam_metadata_key_matches(faam_metadata_entry_at(meta->entries, i), key, mean, name)) return true;
    return false;
}

faam_status faam_metadata_entries_validate(const faam_metadata_entry *entries, uint32_t count) {
    if (!count) return FAAM_OK;
    if (!entries || entries->struct_size < FAAM_METADATA_ENTRY_BASELINE ||
        count > SIZE_MAX / entries->struct_size) return FAAM_ERR_INVALID_ARGUMENT;
    for (uint32_t i = 0; i < count; i++) {
        const faam_metadata_entry *e = faam_metadata_entry_at(entries, i);
        if (e->struct_size != entries->struct_size || !e->key || e->data_type > 0xffffff ||
            (e->flags & ~(uint32_t)FAAM_METADATA_REMOVE) || (e->data_bytes && !e->data) ||
            (e->flags && (e->data || e->data_bytes)) ||
            (e->key == FREEFORM_KEY ? (!e->name || !*e->name) : (e->name || e->mean)))
            return FAAM_ERR_INVALID_ARGUMENT;
        for (uint32_t j = 0; j < i; j++) {
            const faam_metadata_entry *prev = faam_metadata_entry_at(entries, j);
            if (faam_metadata_key_matches(prev, e->key, e->mean, e->name) && prev->flags != e->flags)
                return FAAM_ERR_INVALID_ARGUMENT;
        }
    }
    return FAAM_OK;
}

bool faam_metadata_atom_key(const uint8_t *atom, uint32_t size, uint32_t *key,
                             const uint8_t **mean, uint32_t *mean_len,
                             const uint8_t **name, uint32_t *name_len) {
    if (size < 8) return false;
    *key = read_u32_be(atom + 4);
    *mean = *name = NULL; *mean_len = *name_len = 0;
    if (*key != FREEFORM_KEY) return true;
    for (uint32_t off = 8; off + 8 <= size;) {
        uint32_t sub = read_u32_be(atom + off);
        if (sub < 8 || sub > size - off) return false;
        if (sub >= 12 && !memcmp(atom + off + 4, "mean", 4)) {
            *mean = atom + off + 12; *mean_len = sub - 12;
        } else if (sub >= 12 && !memcmp(atom + off + 4, "name", 4)) {
            *name = atom + off + 12; *name_len = sub - 12;
        }
        off += sub;
    }
    return *name_len && !memchr(*name, 0, *name_len) &&
           (!*mean_len || !memchr(*mean, 0, *mean_len));
}

bool faam_metadata_matches_atom(const faam_entry_list *meta, const uint8_t *atom, uint32_t size) {
    uint32_t key, mean_len, name_len;
    const uint8_t *mean, *name;
    if (!faam_metadata_atom_key(atom, size, &key, &mean, &mean_len, &name, &name_len)) return false;
    for (uint32_t i = 0; i < meta->count; i++) {
        const faam_metadata_entry *e = faam_metadata_entry_at(meta->entries, i);
        if (e->key != key) continue;
        if (key != FREEFORM_KEY) return true;
        const char *m = entry_mean(e->mean);
        if (strlen(e->name) == name_len && !memcmp(e->name, name, name_len) &&
            (mean_len ? (strlen(m) == mean_len && !memcmp(m, mean, mean_len)) :
                        !strcmp(m, "com.apple.iTunes"))) return true;
    }
    return false;
}

typedef struct {
    faam_bytes_writer write;
    void *user;
    uint64_t size;
    faam_status error;
} tag_writer;

static void bytes(tag_writer *w, const void *data, size_t length) {
    if (w->error) return;
    if (length > UINT32_MAX || w->size + length > UINT32_MAX) {
        w->error = FAAM_ERR_UNSUPPORTED;
        return;
    }
    w->size += length;
    if (w->write && length) w->error = w->write(w->user, data, (uint32_t)length);
}
static void number(tag_writer *w, uint32_t value) {
    uint8_t data[4]; write_u32_be(data, value); bytes(w, data, sizeof(data));
}
static void header(tag_writer *w, size_t size, const char *type) {
    if (size > UINT32_MAX) { w->error = FAAM_ERR_UNSUPPORTED; return; }
    number(w, (uint32_t)size); bytes(w, type, 4);
}
static void entry(tag_writer *w, const faam_metadata_entry *e) {
    if (e->flags & FAAM_METADATA_REMOVE) return;
    bool freeform = e->key == FREEFORM_KEY;
    const char *mean = entry_mean(e->mean);
    size_t m = freeform ? strlen(mean) : 0;
    size_t n = freeform ? strlen(e->name) : 0;
    uint64_t total = (freeform ? 48 : 24) + (uint64_t)m + n + e->data_bytes;
    if (total > UINT32_MAX) { w->error = FAAM_ERR_UNSUPPORTED; return; }
    number(w, (uint32_t)total); number(w, e->key);
    if (freeform) {
        header(w, 12 + m, "mean"); number(w, 0); bytes(w, mean, m);
        header(w, 12 + n, "name"); number(w, 0); bytes(w, e->name, n);
    }
    header(w, 16 + (uint64_t)e->data_bytes, "data");
    number(w, e->data_type); number(w, e->locale); bytes(w, e->data, e->data_bytes);
}

static void emit_ilst(tag_writer *w, uint32_t size, const faam_entry_list *m,
                      const faam_gapless_info *gapless, const uint8_t *extra, uint32_t extra_bytes) {
    static const faam_entry_list empty = {0};
    if (!m) m = &empty;
    header(w, size, "ilst");
    for (uint32_t i = 0; i < m->count && !w->error; i++)
        entry(w, faam_metadata_entry_at(m->entries, i));
    if (gapless && !faam_metadata_has_key(m, FREEFORM_KEY, "com.apple.iTunes", "iTunSMPB")) {
        char value[128];
        snprintf(value, sizeof(value),
            " 00000000 %08X %08X %08X%08X 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000",
            gapless->encoder_delay, gapless->end_padding,
            (uint32_t)(gapless->total_samples >> 32), (uint32_t)gapless->total_samples);
        faam_metadata_entry e = { .struct_size = sizeof(e), .key = FREEFORM_KEY,
            .data_type = FAAM_METADATA_UTF8, .name = "iTunSMPB", .data = value,
            .data_bytes = (uint32_t)strlen(value) };
        entry(w, &e);
    }
    if (extra_bytes) bytes(w, extra, extra_bytes);
}

faam_status faam_write_ilst(const faam_entry_list *metadata, const faam_gapless_info *gapless,
                            const uint8_t *extra, uint32_t extra_bytes,
                            faam_bytes_writer write, void *user, uint32_t *out_size) {
    if (!out_size || (extra_bytes && !extra)) return FAAM_ERR_INVALID_ARGUMENT;
    faam_status valid = faam_metadata_entries_validate(metadata ? metadata->entries : NULL,
                                                       metadata ? metadata->count : 0);
    if (valid != FAAM_OK) return valid;
    tag_writer count = {0};
    emit_ilst(&count, 0, metadata, gapless, extra, extra_bytes);
    if (count.error) return count.error;
    *out_size = (uint32_t)count.size;
    if (!write) return FAAM_OK;
    tag_writer output = {0}; output.write = write; output.user = user;
    emit_ilst(&output, *out_size, metadata, gapless, extra, extra_bytes);
    return output.error;
}
