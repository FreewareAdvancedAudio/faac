/*
 * FAAR - Freeware Advanced Audio Resampler
 * Copyright (C) 2026 Nils Schimmelmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */
#ifndef FAAR_TEST_ALLOC_HOOKS_H
#define FAAR_TEST_ALLOC_HOOKS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
void *faar_alloc_malloc(size_t size);
void faar_alloc_free(void *ptr);
void faar_alloc_reset_counters(void);
void faar_alloc_fail_nth(size_t nth);
void faar_alloc_fail_all(int enabled);
size_t faar_alloc_calls(void);
size_t faar_alloc_frees(void);
size_t faar_alloc_live(void);
size_t faar_alloc_live_bytes(void);
size_t faar_alloc_peak_bytes(void);
#ifdef __cplusplus
}
#endif

/* The library allocates only through these hooks. */
#define AllocMemory(size) faar_alloc_malloc(size)
#define FreeMemory(block) faar_alloc_free(block)

#endif
