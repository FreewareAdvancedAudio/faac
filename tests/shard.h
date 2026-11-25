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

/* Splits one long test into several Meson tests that each run a slice of it, so a
 * slow target can time out one slice alone and the slices can run in parallel.
 *
 * The test takes two optional trailing arguments, "<shard> <shards>", and runs the
 * items whose index i satisfies i % shards == shard. With no arguments it runs
 * everything, so running the program by hand behaves as it always did. Every item is
 * covered by exactly one shard whatever the shard count, so adding items, or changing
 * how many shards Meson registers, never leaves anything unrun.
 *
 * Usage:
 *     test_shard s;
 *     if (test_shard_init(&s, argc, argv, 1)) return 2;   // arguments start at argv[1]
 *     for (i = 0; i < n; i++) {
 *         if (!test_shard_take(&s, i)) continue;
 *         ...run item i...
 *     }
 *     return test_shard_finish(&s, n);
 */
#ifndef FAAC_TEST_SHARD_H
#define FAAC_TEST_SHARD_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    unsigned long shard, shards, taken;
} test_shard;

/* Returns nonzero, after printing why, if the arguments are not "<shard> <shards>"
 * with 0 <= shard < shards. A shard that silently ran nothing because of a typo
 * would pass without testing anything, so bad input is an error. */
static inline int test_shard_init(test_shard *s, int argc, char **argv, int first)
{
    s->shard = 0;
    s->shards = 1;
    s->taken = 0;
    if (argc <= first)
        return 0;
    char *end_a = NULL, *end_b = NULL;
    if (argc != first + 2) {
        fprintf(stderr, "usage: %s [<shard> <shards>]\n", argv[0]);
        return 1;
    }
    errno = 0;
    unsigned long shard = strtoul(argv[first], &end_a, 10);
    unsigned long shards = strtoul(argv[first + 1], &end_b, 10);
    if (errno || end_a == argv[first] || *end_a || end_b == argv[first + 1] || *end_b ||
        shards == 0 || shard >= shards) {
        fprintf(stderr, "%s: shard '%s' of '%s' is not 0 <= shard < shards\n", argv[0],
                argv[first], argv[first + 1]);
        return 1;
    }
    s->shard = shard;
    s->shards = shards;
    return 0;
}

/* True if this shard runs item i. */
static inline int test_shard_take(test_shard *s, unsigned long i)
{
    if (i % s->shards != s->shard)
        return 0;
    s->taken++;
    return 1;
}

/* Exit status once the items are done: 0, or 77 (meson: skipped) when this shard has
 * nothing to do because there are fewer than `total` >= `shards` items, as in a build
 * with fewer channels. A shard that finds nothing while enough items exist is a bug
 * in the test and fails. */
static inline int test_shard_finish(const test_shard *s, unsigned long total)
{
    if (s->taken == 0 && total < s->shards) {
        fprintf(stderr, "shard %lu of %lu: only %lu item(s), nothing to run\n", s->shard,
                s->shards, total);
        return 77;
    }
    if (s->taken == 0) {
        fprintf(stderr, "shard %lu of %lu ran nothing although there are %lu items\n",
                s->shard, s->shards, total);
        return 1;
    }
    return 0;
}

#endif
