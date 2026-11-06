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


#ifndef FAAC_FRONTEND_ENCODER_QUIRKS_H
#define FAAC_FRONTEND_ENCODER_QUIRKS_H

#include <stdbool.h>
#include <stdint.h>

#include "faam.h"

/* Explicit container delay wins; a zero delay on an AAC track falls back to
 * the quirk table via the Encoder/Tool tag. track_id 0 is the first audio
 * track. *inferred (optional) is set when the tag supplied the value. */
uint32_t encoder_quirk_resolve_delay(const faam_demuxer *demuxer, uint32_t track_id,
                                     uint32_t explicit_delay, bool *inferred);

/* True when the Encoder/Tool tag names an encoder whose container priming
 * already counts the decoder delay. Unlike the priming fallback it applies
 * even when the container has a delay. */
bool encoder_quirk_priming_includes_decoder_delay(const faam_demuxer *demuxer, uint32_t track_id);

#endif
