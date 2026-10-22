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

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_io.h"

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#define get_process_id _getpid
#else
#include <sys/types.h>
#include <unistd.h>
#define get_process_id getpid
#endif

static void make_path(char *out, size_t out_size, const char *dir, const char *name)
{
    int n = snprintf(out, out_size, "%s/%s", dir, name);
    assert(n > 0 && (size_t)n < out_size);
}

static bool make_hard_link(const char *from, const char *to)
{
#ifdef _WIN32
    return CreateHardLinkA(to, from, NULL) != 0;
#else
    return link(from, to) == 0;
#endif
}

static void write_file(const char *path, const char *contents)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    int write_result = fputs(contents, f);
    int close_result = fclose(f);
    assert(write_result >= 0);
    assert(close_result == 0);
}

static void test_same_file(const char *dir)
{
    char source[1024], spelling_alias[1024], other[1024];
    char hard_alias[1024], soft_alias[1024], missing[1024];
    char name[96];
    snprintf(name, sizeof(name), "same-file-%lu", (unsigned long)get_process_id());
    make_path(source, sizeof(source), dir, name);
    make_path(other, sizeof(other), dir, "same-file-other.tmp");
    make_path(hard_alias, sizeof(hard_alias), dir, "same-file-hardlink.tmp");
    make_path(soft_alias, sizeof(soft_alias), dir, "same-file-symlink.tmp");
    make_path(missing, sizeof(missing), dir, "same-file-missing.tmp");
    int n = snprintf(spelling_alias, sizeof(spelling_alias), "%s/./%s", dir, name);
    assert(n > 0 && (size_t)n < sizeof(spelling_alias));

    remove(source);
    remove(other);
    remove(hard_alias);
    remove(soft_alias);
    remove(missing);
    write_file(source, "source");
    write_file(other, "other");

    assert(cli_same_file(source, source));
    assert(cli_same_file(source, spelling_alias));
    assert(!cli_same_file(source, other));
    assert(!cli_same_file(source, missing));

    assert(make_hard_link(source, hard_alias));
    assert(cli_same_file(source, hard_alias));
    remove(hard_alias);
#ifndef _WIN32
    /* Symlink creation may be unavailable in restricted or unusual CI filesystems. */
    if (symlink(source, soft_alias) == 0) {
        assert(cli_same_file(source, soft_alias));
        remove(soft_alias);
    }
#endif

    remove(source);
    remove(other);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    test_same_file(argv[1]);
    return 0;
}
