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

/* Unit tests for frontend character and tag argument handling. */
#include "../charset.h"
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void test_utf8_ensure(void)
{
    const char input[] = "caf\xc3\xa9 \xe2\x98\x83";
    char *output = utf8_ensure(input);
    assert(output != NULL);
    assert(output != input);
    assert(strcmp(output, input) == 0);
    free(output);
    assert(utf8_ensure(NULL) == NULL);
}

static void test_tag_whitespace_and_quotes(void)
{
    char arg[] = "  \xe2\x80\x9c" "Album" "\xe2\x80\x9d = "
                 "\xe2\x80\x98" "caf\xc3\xa9" "\xe2\x80\x99  ";
    char *name;
    char *value;
    assert(parse_tag_arg(arg, &name, &value) == NULL);
    assert(strcmp(name, "Album") == 0);
    assert(strcmp(value, "caf\xc3\xa9") == 0);

    char empty[] = "Artist= \xe2\x80\x9c" "\xe2\x80\x9d";
    assert(parse_tag_arg(empty, &name, &value) != NULL);
}

static void test_genre_and_indices(void)
{
    uint16_t genre_id;
    const char *genre_name;
    assert(parse_genre("jAzZ", &genre_id, &genre_name));
    assert(genre_id == 9 && strcmp(genre_name, "Jazz") == 0);
    assert(parse_genre("17", &genre_id, &genre_name));
    assert(genre_id == 18 && strcmp(genre_name, "Rock") == 0);
    assert(!parse_genre("", &genre_id, &genre_name));

    uint16_t track = 0;
    uint16_t total = 0;
    assert(parse_index_arg("3/12", &track, &total));
    assert(track == 3 && total == 12);
}

#ifdef _WIN32
static void test_windows_utf16_conversion(void)
{
    const char input[] = "caf\xc3\xa9";
    wchar_t *wide = win32_utf8_to_utf16(input);
    assert(wide != NULL);
    char *output = win32_utf16_to_utf8(wide);
    assert(output != NULL && strcmp(output, input) == 0);
    free(output);
    free(wide);
}
#endif

int main(void)
{
    test_utf8_ensure();
    test_tag_whitespace_and_quotes();
    test_genre_and_indices();
#ifdef _WIN32
    test_windows_utf16_conversion();
#endif
    return 0;
}
