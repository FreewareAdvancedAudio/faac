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
    const char *genre_name;
    assert(parse_genre("jAzZ", &genre_name) && strcmp(genre_name, "Jazz") == 0);
    assert(parse_genre("17", &genre_name) && strcmp(genre_name, "Rock") == 0);
    assert(parse_genre("0017", &genre_name) && strcmp(genre_name, "Rock") == 0);
    assert(!parse_genre("", &genre_name) && !parse_genre(NULL, &genre_name));

    /* The table runs 0 to 191 with no gaps and no repeats, and every name parses back to itself. */
    assert(strcmp(id3_genre_name(0), "Blues") == 0 && strcmp(id3_genre_name(147), "Synthpop") == 0);
    assert(strcmp(id3_genre_name(148), "Abstract") == 0 && strcmp(id3_genre_name(191), "Psybient") == 0);
    assert(strcmp(id3_genre_name(67), "Psychedelic") == 0 && strcmp(id3_genre_name(85), "Bebop") == 0);
    assert(strcmp(id3_genre_name(123), "A Cappella") == 0 && strcmp(id3_genre_name(133), "Afro-Punk") == 0);
    assert(id3_genre_name(192) == NULL);
    for (unsigned i = 0; i < 192; i++)
    {
        const char *name = id3_genre_name(i);
        assert(name && *name);
        char number[8];
        snprintf(number, sizeof(number), "%u", i);
        assert(parse_genre(number, &genre_name) && genre_name == name);
        assert(parse_genre(name, &genre_name) && genre_name == name);
        for (unsigned j = 0; j < i; j++)
            assert(strcmp(id3_genre_name(j), name) != 0);
    }

    /* Spelling variants find the same genre. */
    static const struct { const char *typed; unsigned index; } same[] = {
        { "hip hop", 7 }, { "HIPHOP", 7 }, { "  hip-hop ", 7 }, { "R and B", 14 }, { "r&b", 14 },
        { "Rock and Roll", 78 }, { "rock & roll", 78 }, { "Jazz Funk", 29 }, { "club house", 128 },
        { "Alt. Rock", 40 }, { "AlternRock", 40 }, { "alternative rock", 40 }, { "Gangsta", 59 },
        { "Native US", 64 }, { "Psychadelic", 67 }, { "Bebob", 85 }, { "Acapella", 123 },
        { "a cappella", 123 }, { "afro punk", 133 }, { "Fast-Fusion", 84 }
    };
    for (unsigned i = 0; i < sizeof(same) / sizeof(same[0]); i++)
        assert(parse_genre(same[i].typed, &genre_name) && genre_name == id3_genre_name(same[i].index));
    assert(parse_genre("Rock-ish", &genre_name) && strcmp(genre_name, "Rock-ish") == 0);
    assert(parse_genre("&", &genre_name) && strcmp(genre_name, "&") == 0);

    /* Numbers past the table are refused, other text passes through as typed. */
    assert(!parse_genre("192", &genre_name) && !parse_genre("255", &genre_name));
    assert(!parse_genre("256", &genre_name) && !parse_genre("99999999999999999999", &genre_name));
    assert(parse_genre("NotAGenre", &genre_name) && strcmp(genre_name, "NotAGenre") == 0);
    assert(parse_genre("-1", &genre_name) && strcmp(genre_name, "-1") == 0);

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
