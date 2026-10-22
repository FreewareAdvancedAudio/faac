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

/* Unit tests for frontend output filename and container helpers. */
#include "../output.h"
#include "../cli_io.h"
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* output.c delegates file operations to cli_io; these cases exercise only its
   filename/container decisions, so keep the filesystem boundary deterministic. */
FILE *cli_fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return NULL;
}

bool cli_fsize(FILE *stream, uint64_t *size)
{
    (void)stream;
    (void)size;
    return false;
}

static void test_default_names(void)
{
    char *name = get_output_filename("recording.wav", false);
    assert(name != NULL && strcmp(name, "recording.aac") == 0);
    free(name);

    name = get_output_filename("archive.tar.WAV", true);
    assert(name != NULL && strcmp(name, "archive.tar.m4a") == 0);
    free(name);

    name = get_output_filename("folder.with.dot\\track", false);
    assert(name != NULL && strcmp(name, "folder.with.dot\\track.aac") == 0);
    free(name);

    name = get_output_filename(".profile", true);
    assert(name != NULL && strcmp(name, ".m4a") == 0);
    free(name);
    assert(get_output_filename(NULL, false) == NULL);
}

static void test_extension_detection(void)
{
    assert(is_adts_filename("track.AAC"));
    assert(is_adts_filename("dir.with.dot\\track.adts"));
    assert(!is_adts_filename("track.mp4"));
    assert(!is_adts_filename("dir.aac\\track.wav"));
    assert(!is_adts_filename(NULL));

    assert(is_mp4_filename("track.M4A"));
    assert(is_mp4_filename("track.mp4"));
    assert(is_mp4_filename("track.m4b"));
    assert(!is_mp4_filename("track.aac"));
    assert(!is_mp4_filename(NULL));

    assert(!detect_container_mp4("-"));
    assert(!detect_container_mp4("track.aac"));
    assert(detect_container_mp4("track.m4b"));
    assert(detect_container_mp4("unknown.ext"));
    assert(detect_container_mp4(NULL));
}

static void test_file_size_failure_cases(void)
{
    assert(get_file_size("-") == -1);
    assert(get_file_size("missing-output-test-file.aac") == -1);
}

int main(void)
{
    test_default_names();
    test_extension_detection();
    test_file_size_failure_cases();
    return 0;
}
