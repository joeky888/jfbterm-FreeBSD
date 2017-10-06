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

#if defined (__linux__) || defined (__NetBSD__)
#define LIBICONV_PLUG
#endif

#if defined (__GLIBC__)
#define __USE_STRING_INLINES
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <iconv.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "clipboard.h"
#include "cursor.h"
#include "font.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "mouse.h"
#include "screensaver.h"
#include "term.h"
#include "utilities.h"
#include "vterm.h"
#include "vtermlow.h"

static inline u_int coordToIndex(TVterm *p, u_int x, u_int y);
static inline bool isLeadChar(TVterm *p, u_int x, u_int y);
static inline bool isTailChar(TVterm *p, u_int x, u_int y);
static inline void adjustCoord(TVterm *p, u_int *x, u_int *y);
static inline u_int coordToIndexH(TVterm *p, u_int x, u_int y);
static inline bool isLeadCharH(TVterm *p, u_int x, u_int y);
static inline bool isTailCharH(TVterm *p, u_int x, u_int y);
static inline void adjustCoordH(TVterm *p, u_int *x, u_int *y);
static inline void brmove(void *dst, void *src, int n);
static inline void unclean(void *head, int n);
static inline void unclean4(void *head, int n);
static inline void vterm_move(TVterm *p, int dst, int src, int n);
static inline void vterm_brmove(TVterm *p, int dst, int src, int n);
static inline void vterm_clear(TVterm *p, int top, int n);
static void vterm_text_clean_band(TVterm *p, u_int top, u_int bottom);
static void vterm_add_history(TVterm *p, int line);
static void pollMouseCursor(TVterm *p);

static inline u_int coordToIndex(TVterm *p, u_int x, u_int y)
{
	return (x + (p->history + y) * p->cols4) % p->tsize;
}

static inline bool isLeadChar(TVterm *p, u_int x, u_int y)
{
	return p->flag[coordToIndex(p, x, y)] & VTERM_FLAG_2COLUMN_1;
}

static inline bool isTailChar(TVterm *p, u_int x, u_int y)
{
	return p->flag[coordToIndex(p, x, y)] & VTERM_FLAG_2COLUMN_2;
}

static inline void adjustCoord(TVterm *p, u_int *x, u_int *y)
{
	if (isTailChar(p, *x, *y))
		-- * x;
}

static inline u_int coordToIndexH(TVterm *p, u_int x, u_int y)
{
	return (x + (p->history - p->top + y) * p->cols4) % p->tsize;
}

static inline bool isLeadCharH(TVterm *p, u_int x, u_int y)
{
	return p->flag[coordToIndexH(p, x, y)] & VTERM_FLAG_2COLUMN_1;
}

static inline bool isTailCharH(TVterm *p, u_int x, u_int y)
{
	return p->flag[coordToIndexH(p, x, y)] & VTERM_FLAG_2COLUMN_2;
}

static inline void adjustCoordH(TVterm *p, u_int *x, u_int *y)
{
	if (isTailCharH(p, *x, *y))
		--*x;
}

void vterm_unclean(TVterm *p)
{
	u_int i, x, y;

	for (y = 0; y < p->rows; y++) {
		for (x = 0; x < p->cols; x++) {
			i = coordToIndexH(p, x, y);
			p->flag[i] &= ~VTERM_FLAG_CLEAN;
		}
	}
}

static inline void brmove(void *dst, void *src, int n)
{
	memmove(((uint8_t *)dst - n), ((uint8_t *)src - n), n);
}

static inline void unclean(void *head, int n)
{
	uint8_t *c, *e;

	c = (uint8_t *)head;
	e = (uint8_t *)head + n;
	for (; c < e; c++)
		*c &= ~VTERM_FLAG_CLEAN;
}

static inline void unclean4(void *head, int n)
{
	uint32_t *l, *e;

	l = (uint32_t *)head;
	e = (uint32_t *)head + (n >> 2);
	for (; l < e; l++)
		*l &= 0x7f7f7f7f; /* ~VTERM_FLAG_CLEAN */
}

static inline void vterm_move(TVterm *p, int dst, int src, int n)
{
	memmove(p->text + dst, p->text + src, n * sizeof(uint16_t));
	memmove(p->fontIndex + dst, p->fontIndex + src, n * sizeof(u_int));
	memmove(p->rawText + dst, p->rawText + src, n * sizeof(uint16_t));
	memmove(p->foreground + dst, p->foreground + src, n);
	memmove(p->background + dst, p->background + src, n);
	memmove(p->flag + dst, p->flag + src, n);
}

static inline void vterm_brmove(TVterm *p, int dst, int src, int n)
{
	brmove(p->text + dst, p->text + src, n * sizeof(uint16_t));
	brmove(p->fontIndex + dst, p->fontIndex + src, n * sizeof(u_int));
	brmove(p->rawText + dst, p->rawText + src, n * sizeof(uint16_t));
	brmove(p->foreground + dst, p->foreground + src, n);
	brmove(p->background + dst, p->background + src, n);
	brmove(p->flag + dst, p->flag + src, n);
}

static inline void vterm_clear(TVterm *p, int top, int n)
{
	bzero(p->text + top, n * sizeof(uint16_t));
	bzero(p->fontIndex + top, n * sizeof(u_int));
	bzero(p->rawText + top, n * sizeof(uint16_t));
	memset(p->foreground + top, p->pen.foreground, n);
	memset(p->background + top, p->pen.background, n);
	bzero(p->flag + top, n);
}

void vterm_delete_n_chars(TVterm *p, int n)
{
	u_int i, dx;

	i = coordToIndex(p, p->pen.x, p->pen.y);
	dx = p->cols - p->pen.x - n;

	vterm_move(p, i, i + n, dx);
	unclean(p->flag + i, dx);

	i = coordToIndex(p, p->cols - n, p->pen.y);
	vterm_clear(p, i, n);
}

void vterm_insert_n_chars(TVterm *p, int n)
{
	u_int i, dx;

	i = coordToIndex(p, p->cols - 1, p->pen.y);
	dx = p->cols - p->pen.x - n;
	vterm_brmove(p, i, i - n, dx);

	i = coordToIndex(p, p->pen.x, p->pen.y);
	unclean(p->flag + i + n, dx);
	vterm_clear(p, i, n);
}

void vterm_refresh(TVterm *p)
{
	TFont *font;
	u_int i, x, y;
	sigset_t set, oldset;
	bool underline, doubleColumn;

	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigprocmask(SIG_SETMASK, &set, &oldset);
	if (!p->active) {
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		return;
	}
	cursor_show(p, &cursor, false);
	cursor_show(p, &mouseCursor, false);
	if (p->textClear) {
		gFramebuffer.accessor.fill(&gFramebuffer,
					   0, 0,
					   gFramebuffer.width,
					   gFramebuffer.height,
					   0);
		p->textClear = false;
	}
	doubleColumn = false;
	for (y = 0; y < p->rows; y++) {
		for (x = 0; x < p->cols; x++) {
			i = coordToIndexH(p, x, y);
			if (p->flag[i] & VTERM_FLAG_CLEAN)
				continue; /* already clean */
			p->flag[i] |= VTERM_FLAG_CLEAN;
			if (p->flag[i] & VTERM_FLAG_1COLUMN) {
				font = &(gFonts[p->fontIndex[i]]);
				doubleColumn = false;
			} else if (p->flag[i] & VTERM_FLAG_2COLUMN_1) {
				font = &(gFonts[p->fontIndex[i]]);
				doubleColumn = true;
				p->flag[i + 1] |= VTERM_FLAG_CLEAN;
			} else {
				font = &(gFonts[0]);
				doubleColumn = false;
			}
			underline = (p->flag[i] & VTERM_FLAG_UNDERLINE);
			font_draw(font, p->text[i], p->foreground[i],
				  p->background[i], x, y,
				  underline, doubleColumn);
		}
	}
	/* XXX: pen position go out of screen by resize(1) for example */
	if (p->top == 0 && p->pen.x < p->cols && p->pen.y < p->rows) {
		cursor.wide = isLeadChar(p, p->pen.x, p->pen.y);
		cursor.x = p->pen.x;
		cursor.y = p->pen.y;
		cursor_show(p, &cursor, true);
	}
	sigprocmask(SIG_SETMASK, &oldset, NULL);
}

void vterm_sput(TVterm *p, u_int fontIndex, u_char c, u_char raw)
{
	u_int i;

	i = coordToIndex(p, p->pen.x, p->pen.y);
	p->foreground[i] = p->pen.foreground;
	p->background[i] = p->pen.background;
	p->text[i] = c;
	p->fontIndex[i] = fontIndex;
	p->rawText[i] = raw;
	p->flag[i] = VTERM_FLAG_1COLUMN | VTERM_FLAG_MULTIBYTE;
	if (p->pen.underline)
		p->flag[i] |= VTERM_FLAG_UNDERLINE;
}

void vterm_wput(TVterm *p, u_int fontIndex, u_char c1, u_char c2, u_char raw1, u_char raw2)
{
	u_int i;

	i = coordToIndex(p, p->pen.x, p->pen.y);
	p->foreground[i] = p->pen.foreground;
	p->background[i] = p->pen.background;
	p->text[i] = (c1 << 8) | c2;
	p->fontIndex[i] = fontIndex;
	p->rawText[i] = (raw1 << 8) | raw2;
	p->flag[i] = VTERM_FLAG_2COLUMN_1 | VTERM_FLAG_MULTIBYTE;
	if (p->pen.underline)
		p->flag[i] |= VTERM_FLAG_UNDERLINE;
	i++;
	p->flag[i] = VTERM_FLAG_2COLUMN_2 | VTERM_FLAG_MULTIBYTE;
}

#ifdef ENABLE_UTF8
void vterm_uput1(TVterm *p, u_int fontIndex, uint16_t ucs2, uint16_t raw)
{
	u_int i;

	i = coordToIndex(p, p->pen.x, p->pen.y);
	p->foreground[i] = p->pen.foreground;
	p->background[i] = p->pen.background;
	p->text[i] = ucs2;
	p->fontIndex[i] = fontIndex;
	p->rawText[i] = raw;
	p->flag[i] = VTERM_FLAG_1COLUMN | VTERM_FLAG_UNICODE;
	if (p->pen.underline)
		p->flag[i] |= VTERM_FLAG_UNDERLINE;
}

void vterm_uput2(TVterm *p, u_int fontIndex, uint16_t ucs2, uint16_t raw)
{
	u_int i;

	i = coordToIndex(p, p->pen.x, p->pen.y);
	p->foreground[i] = p->pen.foreground;
	p->background[i] = p->pen.background;
	p->text[i] = ucs2;
	p->fontIndex[i] = fontIndex;
	p->rawText[i] = raw;
	p->flag[i] = VTERM_FLAG_2COLUMN_1 | VTERM_FLAG_UNICODE;
	if (p->pen.underline)
		p->flag[i] |= VTERM_FLAG_UNDERLINE;
	i++;
	p->flag[i] = VTERM_FLAG_2COLUMN_2 | VTERM_FLAG_UNICODE;
}
#endif

void vterm_text_clear_all(TVterm *p)
{
	u_int i, y;

	vterm_add_history(p, p->ymax);
	for (y = 0; y < p->ymax; y++) {
		i = coordToIndex(p, 0, y);
		vterm_clear(p, i, p->cols4);
	}
	p->textClear = true;
}

void vterm_text_clear_eol(TVterm *p, int mode)
{
	u_int i, x, len;

	switch (mode) {
	case 1:
		x = 0;
		len = p->pen.x;
		break;
	case 2:
		x = 0;
		len = p->cols;
		break;
	default:
		x = p->pen.x;
		len = p->cols - p->pen.x;
		break;
	}
	i = coordToIndex(p, x, p->pen.y);
	vterm_clear(p, i, len);
}

void vterm_text_clear_eos(TVterm *p, int mode)
{
	u_int i, len;

	switch (mode) {
	case 1:
		vterm_text_clean_band(p, 0, p->pen.y);
		i = coordToIndex(p, 0, p->pen.y);
		vterm_clear(p, i, p->pen.x);
		break;
	case 2:
		vterm_text_clear_all(p);
		break;
	default:
		if (p->pen.x == 0 && p->pen.y == 0)
			vterm_add_history(p, p->ymax);
		vterm_text_clean_band(p, p->pen.y + 1, p->ymax);
		i = coordToIndex(p, p->pen.x, p->pen.y);
		len = p->cols - p->pen.x;
		vterm_clear(p, i, len);
		break;
	}
}

static void vterm_text_clean_band(TVterm *p, u_int top, u_int bottom)
{
	u_int i, y;

	for (y = top; y < bottom; y++) {
		i = coordToIndex(p, 0, y);
		vterm_clear(p, i, p->cols4);
	}
}

void vterm_text_scroll_down(TVterm *p, int line)
{
	vterm_text_move_down(p, p->ymin, p->ymax, line);
}

void vterm_text_move_down(TVterm *p, u_int top, u_int bottom, int line)
{
	u_int n, src, dst;

	if (bottom <= top + line)
		vterm_text_clean_band(p, top, bottom);
	else {
		for (n = bottom - 1; n >= top + line; n--) {
			dst = coordToIndex(p, 0, n);
			src = coordToIndex(p, 0, n - line);
			vterm_move(p, dst, src, p->cols4);
			unclean4(p->flag + dst, p->cols4);
		}
		vterm_text_clean_band(p, top, top + line);
	}
}

void vterm_text_scroll_up(TVterm *p, int line)
{
	vterm_text_move_up(p, p->ymin, p->ymax, line);
}

static void vterm_add_history(TVterm *p, int line)
{
	u_int o, src, dst, n;

	if (p->history == 0)
		return;
	if (line > p->rows)
		return;
	if (p->history == p->historyTop) {
		src = (line * p->cols4) % p->tsize;
		n = p->history;
		dst = 0;
	} else if (p->history - p->historyTop >= line) {
		src = ((p->history - p->historyTop) * p->cols4) % p->tsize;
		n = p->historyTop + line;
		p->historyTop += line;
		dst = ((p->history - p->historyTop) * p->cols4) % p->tsize;
	} else if (p->history - p->historyTop < line) {
		o = p->historyTop + line - p->history;
		src = ((p->history - p->historyTop + o) * p->cols4) % p->tsize;
		n = line - o;
		p->historyTop = p->history;
		dst = 0;
	}
	vterm_move(p, dst, src, n * p->cols4);
}

void vterm_text_move_up(TVterm *p, u_int top, u_int bottom, int line)
{
	u_int n, src, dst;

	if (bottom <= top + line)
		vterm_text_clean_band(p, top, bottom);
	else {
		if (top == p->ymin && bottom == p->ymax)
			vterm_add_history(p, line);
		for (n = top; n < bottom - line; n++) {
			dst = coordToIndex(p, 0, n);
			src = coordToIndex(p, 0, n + line);
			vterm_move(p, dst, src, p->cols4);
			unclean4(p->flag + dst, p->cols4);
		}
		vterm_text_clean_band(p, bottom - line, bottom);
	}
}

void vterm_scroll_reset(TVterm *p)
{
	if (p->top != 0) {
		p->top = 0;
		vterm_unclean(p);
		vterm_refresh(p);
	}
}

void vterm_scroll_forward_line(TVterm *p)
{
	u_int top;

	top = p->top;
	if (p->top > 0)
		p->top--;
	if (p->top != top) {
		vterm_unclean(p);
		vterm_refresh(p);
	}
}

void vterm_scroll_backward_line(TVterm *p)
{
	u_int top;

	top = p->top;
	if (p->historyTop > p->top)
		p->top++;
	if (p->top != top) {
		vterm_unclean(p);
		vterm_refresh(p);
	}
}

void vterm_scroll_forward_page(TVterm *p)
{
	u_int top;

	top = p->top;
	if (p->top > p->rows)
		p->top -= p->rows;
	else
		p->top = 0;
	if (p->top != top) {
		vterm_unclean(p);
		vterm_refresh(p);
	}
}

void vterm_scroll_backward_page(TVterm *p)
{
	u_int top;

	top = p->top;
	if (p->historyTop > p->top + p->rows)
		p->top += p->rows;
	else
		p->top = p->historyTop;
	if (p->top != top) {
		vterm_unclean(p);
		vterm_refresh(p);
	}
}

void vterm_reverseText(TVterm *p, u_int sx, u_int sy, u_int ex, u_int ey)
{
	u_int from, to, x, y, xx;
	uint8_t foreground, background;
	uint8_t foreground2, background2;

	adjustCoordH(p, &sx, &sy);
	adjustCoordH(p, &ex, &ey);
	for (xx = p->cols, y = sy; y <= ey; y++) {
		if (y == ey)
			xx = ex;
		from = coordToIndexH(p, sx, y);
		to = coordToIndexH(p, xx, y);
		if (p->flag[from] & VTERM_FLAG_2COLUMN_2)
			from--;
		for (x = from; x <= to; x++) {
			if (p->rawText[x] == 0)
				continue;
			foreground = p->foreground[x];
			background = p->background[x];
#ifdef ENABLE_256_COLOR
			if (foreground < 16 && background < 16) {
				foreground2 = (foreground & 8) |
					      (background & ~8);
				background2 = (background & 8) |
					      (foreground & ~8);
				p->foreground[x] = foreground2;
				p->background[x] = background2;
			} else {
				p->foreground[x] = background;
				p->background[x] = foreground;
			}
#else
			foreground2 = (foreground & 8) |
				      (background & ~8);
			background2 = (background & 8) |
				      (foreground & ~8);
			p->foreground[x] = foreground2;
			p->background[x] = background2;
#endif
			p->flag[x] &= ~VTERM_FLAG_CLEAN;
		}
		sx = 0;
	}
}

void vterm_copyText(TVterm *p, u_int sx, u_int sy, u_int ex, u_int ey)
{
	u_int from, to, x, y, xx;
	uint16_t rawText;
	size_t nbytes;
	iconv_t mbcd, uccd;
	char inbytes[MAX_MULTIBYTE_LEN], outbytes[MAX_MULTIBYTE_LEN];
#if defined (__GLIBC__)
	char *inbuf;
#else
	const char *inbuf;
#endif
	char *outbuf;
	size_t inbytesleft, outbytesleft;

	clipboard_clear();

	mbcd = (iconv_t)-1;
	uccd = (iconv_t)-1;
#ifdef ENABLE_OTHER_CODING_SYSTEM
	if (vterm_is_otherCS(p)) {
		if (strcasecmp(p->otherCS->fromcode, p->otherCS->tocode) != 0)
			mbcd = iconv_open(p->otherCS->fromcode,
					  p->otherCS->tocode);
		uccd = iconv_open(p->otherCS->fromcode, "UCS-2BE");
	}
#endif
#ifdef ENABLE_UTF8
	if (uccd == (iconv_t)-1 && vterm_is_UTF8(p))
		uccd = iconv_open("UTF-8", "UCS-2BE");
#endif

	adjustCoordH(p, &sx, &sy);
	adjustCoordH(p, &ex, &ey);
	for (xx = p->cols, y = sy; y <= ey; y++) {
		if (y == ey)
			xx = ex;
		from = coordToIndexH(p, sx, y);
		if (p->flag[from] & VTERM_FLAG_2COLUMN_2)
			from--;
		to = coordToIndexH(p, xx, y);
		for (x = to; x > from; x--)
			if (p->rawText[x] > 0)
				break;
		to = x;
		for (x = from; x <= to; x++) {
			rawText = p->rawText[x];
			outbytes[0] = 0x20;
			nbytes = 1;
			if (p->flag[x] & VTERM_FLAG_1COLUMN &&
			    p->flag[x] & VTERM_FLAG_MULTIBYTE) {
				outbytes[0] = rawText & 0xff;
				nbytes = 1;
			} else if (p->flag[x] & VTERM_FLAG_2COLUMN_1 &&
				   p->flag[x] & VTERM_FLAG_MULTIBYTE) {
				if (mbcd != (iconv_t)-1) {
					inbytes[0] = (rawText >> 8) & 0xff;
					inbytes[1] = rawText & 0xff;
					inbuf = inbytes;
					inbytesleft = 2;
					outbuf = outbytes;
					outbytesleft = sizeof(outbytes);
					if (iconv(mbcd, &inbuf, &inbytesleft,
						  &outbuf, &outbytesleft) != -1)
						nbytes = sizeof(outbytes) -
							 outbytesleft;
					else {
						outbytes[0] = 0x20;
						nbytes = 1;
					}
				} else {
					outbytes[0] = (rawText >> 8) & 0xff;
					outbytes[1] = rawText & 0xff;
					nbytes = 2;
				}
			} else if ((p->flag[x] & VTERM_FLAG_1COLUMN &&
				    p->flag[x] & VTERM_FLAG_UNICODE) ||
				   (p->flag[x] & VTERM_FLAG_2COLUMN_1 &&
				    p->flag[x] & VTERM_FLAG_UNICODE)) {
				if (uccd != (iconv_t)-1) {
					inbytes[0] = (rawText >> 8) & 0xff;
					inbytes[1] = rawText & 0xff;
					inbuf = inbytes;
					inbytesleft = 2;
					outbuf = outbytes;
					outbytesleft = sizeof(outbytes);
					if (iconv(uccd, &inbuf, &inbytesleft,
						  &outbuf, &outbytesleft) != -1)
						nbytes = sizeof(outbytes) -
							 outbytesleft;
					else {
						outbytes[0] = 0x20;
						nbytes = 1;
					}
				}
			} else if (p->flag[x] & VTERM_FLAG_2COLUMN_2)
				continue;
			if (nbytes == 1 &&
			    (outbytes[0] < 0x20 || outbytes[0] == 0x7f))
				outbytes[0] = 0x20;
			clipboard_appendText(outbytes, nbytes);
		}
		if (y < ey)
			clipboard_appendText("\n", 1);
		sx = 0;
	}
	if (mbcd != (iconv_t)-1)
		iconv_close(mbcd);
	if (uccd != (iconv_t)-1)
		iconv_close(uccd);
}

void vterm_pasteText(TVterm *p)
{
	if (clipboard_getLength() > 1)
		write_wrapper(p->term->masterPty,
		              clipboard_getText(), clipboard_getLength() - 1);
}

static void pollMouseCursor(TVterm *p)
{
	u_int x, y;
	bool wide;

	x = mouse_getX();
	y = mouse_getY();
	adjustCoordH(p, &x, &y);
	wide = isLeadCharH(p, x, y);
	if (mouseCursor.x != x || mouseCursor.y != y ||
	    mouseCursor.wide != wide) {
		cursor_show(p, &mouseCursor, false);
		mouseCursor.x = x;
		mouseCursor.y = y;
		mouseCursor.wide = wide;
		cursor_show(p, &mouseCursor, true);
	} else {
		if (mouse_isTimeout())
			cursor_show(p, &mouseCursor, false);
		else {
			if (mouseCursor.style == CURSOR_STYLE_BLINK &&
			    cursor_isTimeout(&mouseCursor)) {
				cursor_toggle(p, &mouseCursor);
				if (cursor.interval == mouseCursor.interval)
					cursor_resetTimer(&cursor);
			}
		}
	}
}

void vterm_pollCursor(TVterm *p, bool wakeup)
{
	static bool scrollLocked[2];

	if (!p->active)
		return;
	if (wakeup) {
		screensaver_execute(false);
		cursor_show(p, &cursor, true);
		pollMouseCursor(p);
		return;
	}
	/* Idle. */
	if (screensaver_isRunning())
		return;
	if (screensaver_isEnable() && screensaver_isTimeout()) {
		cursor_show(p, &cursor, false);
		cursor_show(p, &mouseCursor, false);
		screensaver_execute(true);
		return;
	}
	if (cursor.style == CURSOR_STYLE_NORMAL) {
		scrollLocked[0] = keyboard_isScrollLocked();
		if (scrollLocked[0] != scrollLocked[1]) {
			cursor_show(p, &cursor, !scrollLocked[0]);
			scrollLocked[1] = scrollLocked[0];
			return;
		}
	} else if (cursor.style == CURSOR_STYLE_BLINK)
		if (cursor_isTimeout(&cursor))
			cursor_toggle(p, &cursor);
	if (mouse_isEnable())
		pollMouseCursor(p);
}

