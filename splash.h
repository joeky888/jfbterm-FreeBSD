/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
 *
 * JFBTERM Unofficial eXtension Patch
 * Copyright (C) 2006 Young Chol Song <nskystars at yahoo dot com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY YUSUKE BABA ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL YUSUKE BABA BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef INCLUDE_SPLASH_H
#define INCLUDE_SPLASH_H
#ifdef ENABLE_SPLASH_SCREEN

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#include "getcap.h"

#define SPLASH_INRANGE(p, x, y) \
	(((x) >= 0) && ((x) < p.width) && ((y) >= 0) && ((y) < p.height))

#define SPLASH_GET_PIXEL_15(p, x, y) \
	(*(((uint16_t *)p.bitmap) + (p.width) * (y) + (x)))

#define SPLASH_GET_PIXEL_16(p, x, y) \
	(*(((uint16_t *)p.bitmap) + (p.width) * (y) + (x)))

#define SPLASH_GET_PIXEL_24(p, x, y, c) \
	(*(((uint8_t *)p.bitmap) + (p.width) * 3 * (y) + (x) * 3 + (c)))

#define SPLASH_GET_PIXEL_32(p, x, y) \
	(*(((uint32_t *)p.bitmap) + (p.width) * (y) + (x)))

typedef struct TSplash_Raw {
	u_char *bitmap;
	u_int width;
	u_int height;
	bool enable;
} TSplash;

extern TSplash gSplash;

void splash_initialize(void);
void splash_configure(TCaps *caps);
void splash_load(void);

#endif  /* ENABLE_SPLASH_SCREEN */
#endif  /* INCLUDE_SPLASH_H */

