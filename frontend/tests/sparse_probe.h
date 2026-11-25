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

#ifndef FAAC_TEST_SPARSE_PROBE_H
#define FAAC_TEST_SPARSE_PROBE_H

/* Tests that write fixtures past 4 GiB rely on the filesystem keeping the hole
 * unallocated. Some systems (GNU/Hurd, tmpfs-less chroots, FAT) either refuse
 * the seek or allocate every byte, which turns a unit test into a multi-GiB
 * write that times out. Probe first so those tests can skip instead. */
#ifndef _WIN32
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

static double sparse_probe_now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

/* Write one byte after a hole of `hole` bytes and report whether the file
 * stayed sparse. A hole that takes more than a second to create is not a
 * hole, whatever st_blocks says. */
static inline bool sparse_hole_ok(const char *path, off_t hole)
{
    bool ok = false;
    double start = sparse_probe_now();
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    if (fseeko(f, hole, SEEK_SET) == 0 && fputc(0, f) != EOF && fflush(f) == 0) {
        struct stat st;
        /* st_blocks counts 512-byte units; a real hole allocates almost nothing. */
        ok = fstat(fileno(f), &st) == 0 && st.st_size > hole &&
             (long long)st.st_blocks * 512 < (1LL << 20);
    }
    fclose(f);
    remove(path);
    return ok && sparse_probe_now() - start < 1.0;
}

/* A small hole first: on a filesystem that fills holes, the probe itself must
 * not write gigabytes. Only then try the real >4 GiB distance. */
static inline bool sparse_files_supported(const char *path)
{
    if (sizeof(off_t) < 8) return false;
#if defined(__GNU__) || defined(__gnu_hurd__)
    /* GNU/Hurd: the seek past 4 GiB succeeds but the fixture write then times
     * out, and st_blocks cannot be trusted to say why. Skip outright. */
    (void)path;
    return false;
#endif
    return sparse_hole_ok(path, (off_t)64 << 20) &&
           sparse_hole_ok(path, ((off_t)1 << 32) + 8191);
}
#endif

#endif
