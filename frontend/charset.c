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

#include "git_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <locale.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <sys/types.h>
#elif defined(HAVE_ICONV)
#include <langinfo.h>
#include <iconv.h>
#include <strings.h>
#endif

#include "charset.h"

#ifdef _WIN32
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

static UINT s_orig_output_cp = 0;
static bool s_cp_saved = false;

static void win32_restore_console_cp(void)
{
    if (s_cp_saved && s_orig_output_cp != 0)
        SetConsoleOutputCP(s_orig_output_cp);
}

static bool win32_has_console_output(void)
{
    HANDLE handles[] = { GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE) };
    for (size_t i = 0; i < sizeof(handles) / sizeof(handles[0]); i++)
    {
        DWORD mode;
        if (handles[i] != NULL && handles[i] != INVALID_HANDLE_VALUE &&
            GetConsoleMode(handles[i], &mode))
            return true;
    }
    return false;
}
#endif

void cli_init_console(void)
{
#ifdef _WIN32
    if (!s_cp_saved && win32_has_console_output())
    {
        s_orig_output_cp = GetConsoleOutputCP();
        if (s_orig_output_cp != 0 && SetConsoleOutputCP(CP_UTF8))
        {
            s_cp_saved = true;
            atexit(win32_restore_console_cp);
        }
    }
    setlocale(LC_CTYPE, ".UTF-8");
#else
    setlocale(LC_CTYPE, "");
#endif
}

/* ID3v1 genres 0-147, then the Winamp extension to 191. */
static const char *const id3_genres[] = {
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz",
    "Metal", "New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno",
    "Industrial", "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno",
    "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical",
    "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
    "Alternative Rock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop",
    "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial", "Electronic",
    "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta Rap", "Top 40",
    "Christian Rap", "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave", "Psychedelic",
    "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka",
    "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk", "Folk-Rock", "National Folk",
    "Swing", "Fast Fusion", "Bebop", "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde",
    "Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock",
    "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour", "Speech", "Chanson", "Opera",
    "Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire",
    "Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad",
    "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo", "A Cappella", "Euro-House",
    "Dance Hall", "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie",
    "BritPop", "Afro-Punk", "Polsk Punk", "Beat", "Christian Gangsta Rap", "Heavy Metal",
    "Black Metal", "Crossover", "Contemporary Christian", "Christian Rock", "Merengue",
    "Salsa", "Thrash Metal", "Anime", "JPop", "Synthpop", "Abstract", "Art Rock", "Baroque",
    "Bhangra", "Big Beat", "Breakbeat", "Chillout", "Downtempo", "Dub", "EBM", "Eclectic",
    "Electro", "Electroclash", "Emo", "Experimental", "Garage", "Global", "IDM", "Illbient",
    "Industro-Goth", "Jam Band", "Krautrock", "Leftfield", "Lounge", "Math Rock",
    "New Romantic", "Nu-Breakz", "Post-Punk", "Post-Rock", "Psytrance", "Shoegaze",
    "Space Rock", "Trop Rock", "World Music", "Neoclassical", "Audiobook", "Audio Theatre",
    "Neue Deutsche Welle", "Podcast", "Indie Rock", "G-Funk", "Dubstep", "Garage Rock",
    "Psybient"
};

/* Smart-quote substitution (Notes, Pages, word processors) turns straight
   quotes into curly ones before a pasted --tag/--genre argument ever
   reaches us, so both forms need stripping. */
char *trim_quotes_and_spaces(char *s)
{
    if (!s) return s;

    while (*s)
    {
        if (*s == ' ' || *s == '\t' || *s == '"' || *s == '\'')
        {
            s++;
        }
        else if ((unsigned char)s[0] == 0xE2 && (unsigned char)s[1] == 0x80 &&
                 ((unsigned char)s[2] == 0x9C || (unsigned char)s[2] == 0x9D || (unsigned char)s[2] == 0x98 || (unsigned char)s[2] == 0x99))
        {
            s += 3;
        }
        else
        {
            break;
        }
    }

    size_t len = strlen(s);
    while (len > 0)
    {
        if (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '"' || s[len - 1] == '\'')
        {
            s[--len] = '\0';
        }
        else if (len >= 3 && (unsigned char)s[len - 3] == 0xE2 && (unsigned char)s[len - 2] == 0x80 &&
                 ((unsigned char)s[len - 1] == 0x9C || (unsigned char)s[len - 1] == 0x9D || (unsigned char)s[len - 1] == 0x98 || (unsigned char)s[len - 1] == 0x99))
        {
            len -= 3;
            s[len] = '\0';
        }
        else
        {
            break;
        }
    }

    return s;
}

const char *parse_tag_arg(char *arg, char **name, char **value)
{
    char *eq = strchr(arg, '=');
    char *comma = strchr(arg, ',');
    char *sep;

    if (eq && comma)
        sep = (eq < comma) ? eq : comma;
    else
        sep = eq ? eq : comma;
    if (!sep)
        return "Missing tag value.\n";

    *sep++ = '\0';
    *name = trim_quotes_and_spaces(arg);
    *value = trim_quotes_and_spaces(sep);
    if (**name == '\0')
        return "Tag name cannot be empty.\n";
    if (**value == '\0')
        return "Tag value cannot be empty.\n";
    return NULL;
}

bool parse_index_arg(const char *arg, uint16_t *n, uint16_t *total)
{
    return sscanf(arg, "%hu/%hu", n, total) >= 1;
}

/* Spellings other taggers use for a genre, matched like the table names. */
static const struct { const char *name; unsigned index; } id3_genre_aliases[] = {
    { "Alt. Rock", 40 }, { "AlternRock", 40 }, { "Gangsta", 59 }, { "Native US", 64 },
    { "Psychadelic", 67 }, { "Bebob", 85 }, { "Acapella", 123 }
};

/* Lower-cased letters and digits only, with '&' read as "and", so "Hip Hop", "hip-hop" and
 * "HIPHOP" are one genre and so are "R&B" and "R and B". */
static bool genre_key(const char *s, char *key, size_t size)
{
    size_t n = 0;
    for (; *s; s++)
    {
        const char *add = NULL;
        char one[2] = { 0, 0 };
        if (*s == '&')
            add = "and";
        else if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9'))
        {
            one[0] = (*s >= 'A' && *s <= 'Z') ? (char)(*s - 'A' + 'a') : *s;
            add = one;
        }
        if (!add)
            continue;
        size_t len = strlen(add);
        if (n + len >= size)
            return false;
        memcpy(key + n, add, len);
        n += len;
    }
    key[n] = 0;
    return n > 0;
}

const char *id3_genre_name(unsigned index)
{
    return index < sizeof(id3_genres) / sizeof(id3_genres[0]) ? id3_genres[index] : NULL;
}

bool parse_genre(const char *arg, const char **genre_name)
{
    if (!arg || !*arg)
        return false;

    bool is_num = true;
    for (const char *p = arg; *p; p++)
    {
        if (*p < '0' || *p > '9')
        {
            is_num = false;
            break;
        }
    }

    if (is_num)
    {
        unsigned index = 0;
        for (const char *p = arg; *p; p++)
        {
            index = index * 10 + (unsigned)(*p - '0');
            if (index >= sizeof(id3_genres) / sizeof(id3_genres[0]))
                return false;
        }
        const char *name = id3_genre_name(index);
        if (genre_name) *genre_name = name;
        return true;
    }

    char want[64], have[64];
    if (genre_key(arg, want, sizeof(want)))
    {
        for (unsigned i = 0; i < sizeof(id3_genres) / sizeof(id3_genres[0]); i++)
        {
            if (genre_key(id3_genres[i], have, sizeof(have)) && strcmp(want, have) == 0)
            {
                if (genre_name) *genre_name = id3_genres[i];
                return true;
            }
        }
        for (unsigned i = 0; i < sizeof(id3_genre_aliases) / sizeof(id3_genre_aliases[0]); i++)
        {
            if (genre_key(id3_genre_aliases[i].name, have, sizeof(have)) && strcmp(want, have) == 0)
            {
                if (genre_name) *genre_name = id3_genres[id3_genre_aliases[i].index];
                return true;
            }
        }
    }

    /* Anything else is stored as the user typed it. */
    if (genre_name) *genre_name = arg;
    return true;
}

/* Structural check only (continuation-byte pattern, NUL-termination): does
   not reject overlong encodings, surrogates, or code points past U+10FFFF.
   A plausibility check, not a strict validator -- good enough to decide
   "does this need repair_to_utf8()", not for security-sensitive callers. */
static bool utf8_is_valid(const char *str)
{
    if (!str)
        return true;

    const unsigned char *bytes = (const unsigned char *)str;
    while (*bytes)
    {
        if (bytes[0] <= 0x7F)
        {
            bytes += 1;
        }
        else if ((bytes[0] & 0xE0) == 0xC0)
        {
            if (bytes[1] == '\0' || (bytes[1] & 0xC0) != 0x80) return false;
            bytes += 2;
        }
        else if ((bytes[0] & 0xF0) == 0xE0)
        {
            if (bytes[1] == '\0' || (bytes[1] & 0xC0) != 0x80 ||
                bytes[2] == '\0' || (bytes[2] & 0xC0) != 0x80) return false;
            bytes += 3;
        }
        else if ((bytes[0] & 0xF8) == 0xF0)
        {
            if (bytes[1] == '\0' || (bytes[1] & 0xC0) != 0x80 ||
                bytes[2] == '\0' || (bytes[2] & 0xC0) != 0x80 ||
                bytes[3] == '\0' || (bytes[3] & 0xC0) != 0x80) return false;
            bytes += 4;
        }
        else
        {
            return false;
        }
    }
    return true;
}

#ifdef _WIN32
char *win32_utf16_to_utf8(const wchar_t *wstr)
{
    if (!wstr)
        return NULL;

    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (len <= 0)
        return NULL;

    char *str = malloc((size_t)len);
    if (str)
    {
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
    }
    return str;
}

/* Attempts to repair str, assumed to be in the current Windows ANSI code
   page, into UTF-8. Returns NULL if it can't. */
static char *repair_to_utf8(const char *str)
{
    int wn = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    if (wn <= 0)
        return NULL;

    wchar_t *ws = malloc((size_t)wn * sizeof(wchar_t));
    if (!ws)
        return NULL;

    MultiByteToWideChar(CP_ACP, 0, str, -1, ws, wn);
    char *utf8 = win32_utf16_to_utf8(ws);
    free(ws);
    return utf8;
}

wchar_t *win32_utf8_to_utf16(const char *utf8_str)
{
    if (!utf8_str)
        return NULL;

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (wlen <= 0)
        return NULL;

    wchar_t *wstr = malloc((size_t)wlen * sizeof(wchar_t));
    if (wstr)
        MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, wlen);

    return wstr;
}

#else /* POSIX */
#ifdef HAVE_ICONV
/* Mirrors the Windows path (assume the current code page, convert to UTF-8)
   using POSIX's equivalent: the locale's codeset name plus iconv(). Returns
   NULL if it can't repair str. Requires setlocale(LC_CTYPE, "") to have
   been called at startup -- otherwise nl_langinfo() reports the "C"
   locale's codeset (ASCII) regardless of the environment. */
static char *repair_to_utf8(const char *str)
{
    const char *codeset = nl_langinfo(CODESET);
    if (!codeset || !strcasecmp(codeset, "UTF-8") || !strcasecmp(codeset, "UTF8"))
        return NULL; /* locale already claims UTF-8 (or is unknown): this is
                        corrupt input, not a different encoding */

    iconv_t cd = iconv_open("UTF-8", codeset);
    if (cd == (iconv_t)-1)
        return NULL;

    size_t inbytes = strlen(str);
    size_t outcap = inbytes * 4 + 16; /* worst-case UTF-8 expansion + headroom */
    char *outbuf = malloc(outcap);
    if (!outbuf)
    {
        iconv_close(cd);
        return NULL;
    }

    char *inp = (char *)str;
    char *outp = outbuf;
    size_t inleft = inbytes;
    size_t outleft = outcap - 1; /* leave room for the NUL below */

    size_t rc = iconv(cd, &inp, &inleft, &outp, &outleft);
    /* Flush to the initial shift state: a no-op for simple codesets like
       ISO-8859-*, but stateful ones (e.g. ISO-2022-JP) need a trailing
       escape sequence emitted here, or the converted tag is truncated. */
    if (rc != (size_t)-1 && inleft == 0)
        iconv(cd, NULL, NULL, &outp, &outleft);
    iconv_close(cd);

    if (rc == (size_t)-1 || inleft != 0)
    {
        free(outbuf);
        return NULL;
    }

    *outp = '\0';
    return outbuf;
}
#else
/* No iconv available on this platform: can't attempt repair. */
static char *repair_to_utf8(const char *str)
{
    (void)str;
    return NULL;
}
#endif
#endif

char *utf8_ensure(const char *str)
{
    if (!str)
        return NULL;

    if (utf8_is_valid(str))
        return strdup(str);

    char *fixed = repair_to_utf8(str);
    if (fixed)
        return fixed;

    fprintf(stderr, "warning: tag value is not valid UTF-8, writing as-is\n");
    return strdup(str);
}

const char *cli_version_string(char *buf, size_t buf_size, const char *version) {
    if (FAAC_GIT_VERSION[0])
        snprintf(buf, buf_size, "%s (%s)", version, FAAC_GIT_VERSION);
    else
        snprintf(buf, buf_size, "%s", version);
    return buf;
}

void cli_print_patent_notice(FILE *stream) {
    fputs("\nPlease note that the use of this software may require the payment of patent\n"
          "royalties. You need to consider this issue before you start building derivative\n"
          "works. We are not warranting or indemnifying you in any way for patent\n"
          "royalities! YOU ARE SOLELY RESPONSIBLE FOR YOUR OWN ACTIONS!\n\n", stream);
}

void cli_print_lgpl_notice(FILE *stream, const char *subject) {
    fprintf(stream, "This %s is free software; you can redistribute it and/or\n", subject);
    fprintf(stream,
                    "modify it under the terms of the GNU Lesser General Public\n"
                    "License as published by the Free Software Foundation; either\n"
                    "version 2.1 of the License, or (at your option) any later version.\n"
                    "\n"
                    "This %s is distributed in the hope that it will be useful,\n"
                    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU\n"
                    "Lesser General Public License for more details.\n\n", subject);
}
