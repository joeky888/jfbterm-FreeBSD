/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
 *
 * JFBTERM/ME - J framebuffer terminal/Multilingual Enhancement -
 * Copyright (C) 2003 Fumitoshi UKAI <ukai@debian.or.jp>
 * Copyright (C) 1999 Noritoshi MASUICHI <nmasu@ma3.justnet.ne.jp>
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

#ifndef INCLUDE_FONT_H
#define INCLUDE_FONT_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "getcap.h"

#define FONT_SIGNATURE_SINGLE  (0x00000000)           /* 1 byte character set */
#define FONT_SIGNATURE_DOUBLE  (0x01000000)           /* 2 byte character set */
#define FONT_SIGNATURE_94CHAR  (0x00000000)           /* 94 or 94^n */
#define FONT_SIGNATURE_96CHAR  (0x02000000)           /* 96 or 96^n */
#define FONT_SIGNATURE_OTHER   (0x10000000)           /* other coding system */

typedef enum {
	FONT_HALF_LEFT,
	FONT_HALF_RIGHT,
	FONT_HALF_UNI
} FONT_HALF;

typedef struct Raw_TFont {
	const u_char *(*getGlyph)(struct Raw_TFont *p, uint16_t code,
				  u_short *width);
	const char *name;
	u_short width;
	u_short height;
	u_int signature;
	FONT_HALF half;
	bool alias;
	u_char **glyphs;
	u_short *glyphWidths;
	u_char *defaultGlyph;
	u_char *bitmap;
	u_int colf;
	u_int coll;
	u_int rowf;
	u_int rowl;
	u_int colspan;
	u_short bytesPerWidth;
	u_short bytesPerChar;
} TFont;

extern TFont gFonts[];
extern u_short gFontsWidth;
extern u_short gFontsHeight;

void font_initialize(void);
void font_configure(TCaps *caps);
void font_showList(FILE *stream);
int font_getIndexBySignature(const u_int signature);
int font_getIndexByName(const char *name);
bool font_isLoaded(TFont *p);
void font_unifontGlyph(TFont *p, int ambiguousWidth);
void font_draw(TFont *p, uint16_t code, uint8_t foregroundColor,
	       uint8_t backgroundColor, u_int x, u_int y,
	       bool underline, bool doubleColumn);

#endif /* INCLUDE_FONT_H */

