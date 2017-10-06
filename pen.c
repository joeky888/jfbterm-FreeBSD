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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "palette.h"
#include "pen.h"

void pen_initialize(TPen *p)
{
	assert(p != NULL);

	p->prev = NULL;
	p->x = 0;
	p->y = 0;
	p->foreground = 7; /* White */
	p->background = 0; /* Black */
	p->underline = false;
	p->reverse   = false;
	p->highlight = false;
}

void pen_finalize(TPen *p)
{
	TPen *q;

	assert(p != NULL);

	q = p->prev;
	p->prev = NULL;
	if (q != NULL) {
		pen_finalize(q);
		free(q);
	}
}

void pen_copy(TPen *p, TPen *q)
{
	assert(p != NULL);

	p->prev = NULL;
	p->x = q->x;
	p->y = q->y;
	p->foreground = q->foreground;
	p->background = q->background;
	p->underline = q->underline;
	p->reverse   = q->reverse;
	p->highlight = q->highlight;
}

inline void pen_resetAttribute(TPen *p)
{
	assert(p != NULL);

	p->foreground = 7; /* White */
	p->background = 0; /* Black */
	p->underline = false;
	p->reverse   = false;
	p->highlight = false;
}

void pen_setHighlight(TPen *p, const bool status)
{
	assert(p != NULL);

	if (status) {
		p->highlight = true;
#ifdef ENABLE_256_COLOR
		if (p->foreground < 16) p->foreground |= 8;
#else
		if (p->foreground != 0) p->foreground |= 8;
#endif
	} else {
		p->highlight = false;
#ifdef ENABLE_256_COLOR
		if (p->foreground < 16) p->foreground &= ~8;
#else
		p->foreground &= ~8;
#endif
	}
}

void pen_setUnderline(TPen *p, const bool status)
{
	assert(p != NULL);

	p->underline = status;
}

void pen_setReverse(TPen *p, const bool status)
{
	uint8_t color;

	assert(p != NULL);

	if (status) {
		if (!p->reverse) {
			p->reverse = true;
#ifdef ENABLE_256_COLOR
			if (p->foreground < 16)
				color = p->foreground & ~8;
			else
				color = p->foreground;
			if (p->background < 16) {
				p->foreground = p->background & ~8;
				if (p->highlight && p->foreground != 0)
					p->foreground |= 8;
			} else
				p->foreground = p->background;
			p->background = color;
#else
			color = p->foreground & ~8;
			p->foreground = p->background & ~8;
			if (p->highlight && p->foreground != 0)
				p->foreground |= 8;
			p->background = color;
#endif
		}
	} else {
		if (p->reverse) {
			p->reverse = false;
#ifdef ENABLE_256_COLOR
			if (p->foreground < 16)
				color = p->foreground & ~8;
			else
				color = p->foreground;
			if (p->background < 16) {
				p->foreground = p->background & ~8;
				if (p->highlight && p->foreground != 0)
					p->foreground |= 8;
			} else
				p->foreground = p->background;
			p->background = color;
#else
			color = p->foreground & ~8;
			p->foreground = p->background & ~8;
			if (p->highlight && p->foreground != 0)
				p->foreground |= 8;
			p->background = color;
#endif
		}
	}
}

void pen_setColor(TPen *p, const int ansiColor)
{
	uint8_t color;

	assert(p != NULL);

	if (ansiColor >= 30 && ansiColor <= 37) {
		color = palette_ansiToVGA(ansiColor - 30);
		if (p->reverse)
			p->background = color;
		else {
			if (p->highlight) color |= 8;
			p->foreground = color;
		}
	} else if (ansiColor == 39) {
		color = 7; /* White */
		if (p->reverse)
			p->background = color;
		else {
			if (p->highlight) color |= 8;
			p->foreground = color;
		}
	} else if (ansiColor >= 40 && ansiColor <= 47) {
		color = palette_ansiToVGA(ansiColor - 40);
		if (p->reverse) {
			if (p->highlight) color |= 8;
			p->foreground = color;
		} else
			p->background = color;
	} else if (ansiColor == 49) {
		color = 0; /* Black */
		if (p->reverse) {
			if (p->highlight) color |= 8;
			p->foreground = color;
		} else
			p->background = color;
	}
}

#ifdef ENABLE_256_COLOR
void pen_set256Color(TPen *p, const int mode, const int ansiColor)
{
	uint8_t color;

	assert(p != NULL);

	if (ansiColor < 0 || ansiColor > 255)
		return;
	color = palette_ansiToVGA(ansiColor);
	if (mode == 38) {
		if (p->reverse) {
			p->background = color;
		} else {
			if (p->highlight && color < 16) color |= 8;
			p->foreground = color;
		}
	} else if (mode == 48) {
		if (p->reverse) {
			if (p->highlight && color < 16) color |= 8;
			p->foreground = color;
		} else {
			p->background = color;
		}
	}
}
#endif

