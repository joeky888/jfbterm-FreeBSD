/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
 *
 * JFBTERM/ME - J framebuffer terminal/Multilingual Enhancement -
 * Copyright (C) 2003 Fumitoshi UKAI <ukai@debian.or.jp>
 * Copyright (C) 1999 Noritoshi MASUICHI <nmasu@ma3.justnet.ne.jp>
 *
 * KON2 - Kanji ON Console -
 * Copyright (C) 1992,1993 Atusi MAEDA <mad@math.keio.ac.jp>
 * Copyright (C) 1992-1996 Takashi MANABE <manabe@papilio.tutics.tut.ac.jp>
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

#ifndef INCLUDE_VTERM_H
#define INCLUDE_VTERM_H

#include <sys/time.h>
#include <sys/types.h>
#ifdef ENABLE_OTHER_CODING_SYSTEM
#include <iconv.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "font.h"
#include "pen.h"

typedef struct Raw_TFontSpec {
	u_int invokedGn;        /* 呼び出さされている Gn : n = 0..3 */
	u_int idx;              /* 文字集合のgFont[]での位置 */
	u_int type;             /* 文字集合の区分 */
	FONT_HALF half;         /* 文字集合のG0,G1 のどちらを使っているか */
} TFontSpec;

#define MAX_MULTIBYTE_LEN (6)

#ifdef ENABLE_OTHER_CODING_SYSTEM
typedef struct Raw_TCodingSystem {
	/* iconv state */
	iconv_t cd;
	char *fromcode;
	char *tocode;
	char inbuf[MAX_MULTIBYTE_LEN];
	size_t inbuflen;
	char outbuf[MAX_MULTIBYTE_LEN];
	/* saved state */
	u_int gSavedL;
	u_int gSavedR;
	u_int gSavedIdx[4];
	u_int utf8SavedIdx;
} TCodingSystem;
#endif

#define VTERM_FLAG_MULTIBYTE    (0x01) /* multibyte */
#define VTERM_FLAG_UNICODE      (0x02) /* unicode */
#define VTERM_FLAG_UNDERLINE    (0x04) /* underline */
#define VTERM_FLAG_1COLUMN      (0x10) /* 1 column */
#define VTERM_FLAG_2COLUMN_1    (0x20) /* 2 column 1st */
#define VTERM_FLAG_2COLUMN_2    (0x40) /* 2 column 2nd */
#define VTERM_FLAG_CLEAN        (0x80) /* skip draw */

typedef struct Raw_TVterm {
	struct Raw_TTerm *term;
	TCaps *caps;
	u_int history;
	u_int historyTop;
	u_int top;
	u_int tab;
	u_int cols;
	u_int cols4;
	u_int rows;
	u_int tsize;
	u_int xmax;
	u_int ymin;
	u_int ymax;
	int scroll;
	TPen pen;
	TPen *savedPen;
	TPen *savedPenSL;
	enum {
		VTERM_STATUS_LINE_NONE,
		VTERM_STATUS_LINE_ENTER,
		VTERM_STATUS_LINE_LEAVE
	} statusLine;
	enum {
		VTERM_MOUSE_TRACKING_NONE,
		VTERM_MOUSE_TRACKING_X10,
		VTERM_MOUSE_TRACKING_VT200,
		VTERM_MOUSE_TRACKING_BTN_EVENT,
		VTERM_MOUSE_TRACKING_ANY_EVENT
	} mouseTracking;
	/* ISO-2022 */
	u_int gDefaultL;
	u_int gDefaultR;
	u_int gDefaultIdx[4];
	u_int escSignature;
	u_int escGn;
	u_int gIdx[4];  /* Gnに指示されている文字集合のgFonts[]での位置 */
	TFontSpec gl;   /* GLに呼び出されている文字集合 */
	TFontSpec gr;   /* GRに呼び出されている文字集合 */
	TFontSpec tgl;  /* 次の文字がGLのときに使う文字集合 */
	TFontSpec tgr;  /* 次の文字がGRのときに使う文字集合 */
#ifdef ENABLE_UTF8
	u_int utf8DefaultIdx;
	u_int utf8Idx;
	u_int utf8remain;
	uint16_t ucs2;
	int ambiguous;
#endif
#ifdef ENABLE_OTHER_CODING_SYSTEM
	TCodingSystem *otherCS;
#endif
	u_char dbcsLeadByte;
	FONT_HALF dbcsHalf;
	u_int dbcsIdx;
	bool altCs;
	bool wrap;
	bool insert;
	bool cursor;
	bool active;
	bool textClear;
	void (*esc)(struct Raw_TVterm *p, u_char c);
	uint16_t *text;
	u_int *fontIndex;
	uint16_t *rawText;
	uint8_t *foreground;
	uint8_t *background;
	uint8_t *flag;
} TVterm;

void vterm_initialize(TVterm *p, struct Raw_TTerm *term, TCaps *caps,
		      u_int history, u_int cols, u_int rows,
		      const char *encoding, int ambiguous);
void vterm_finalize(TVterm *p);
#ifdef ENABLE_UTF8
bool vterm_is_UTF8(TVterm *p);
#endif
#ifdef ENABLE_OTHER_CODING_SYSTEM
bool vterm_is_otherCS(TVterm *p);
char *vterm_format_otherCS(const char *encoding);
#endif
void vterm_emulate(TVterm *p, const u_char *buf, size_t nchars);
void vterm_show_sequence(FILE *stream, const char *encoding);

#endif /* INCLUDE_VTERM_H */

