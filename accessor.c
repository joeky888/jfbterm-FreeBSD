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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if defined (__linux__)
#include <endian.h>
#elif (defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__))
#include <machine/endian.h>
#endif

#ifdef ENABLE_VGA16FB
#if defined (__linux__)
#include <sys/io.h>
#elif defined (__FreeBSD__)
#include <machine/sysarch.h>
#include <machine/bus.h>
#endif
#endif

#include "accessor.h"
#include "palette.h"
#ifdef ENABLE_SPLASH_SCREEN
#include "splash.h"
#endif

#ifndef min
#define min(a, b) (((a) > (b)) ? (b) : (a))
#endif

#ifdef ENABLE_8BPP
void accessor_fill_8bpp(TFrameBuffer *p, u_int sx, u_int sy,
			u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, w, h;
	uint8_t *d;

	w = min(sx + ex, p->width);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = (uint8_t *)(p->memory + (y * p->bytesPerLine + sx));
		for (x = sx; x < w; x++)
			*d++ = color;
	}
}

void accessor_overlay_8bpp(TFrameBuffer *p, const u_char *bitmap,
			   u_int bytesPerWidth, u_int sx, u_int sy,
			   u_int ex, u_int ey, uint8_t color)
{
	u_int i, y, h;
	const u_char *cp;
	u_char c;
	uint8_t *d;

	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		cp = bitmap;
		d = (uint8_t *)(p->memory + (y * p->bytesPerLine + sx));
		for (i = ex; i >= 8; i -= 8) {
			c = *cp++;
			if (c & 0x80) d[0] = color;
			if (c & 0x40) d[1] = color;
			if (c & 0x20) d[2] = color;
			if (c & 0x10) d[3] = color;
			if (c & 0x08) d[4] = color;
			if (c & 0x04) d[5] = color;
			if (c & 0x02) d[6] = color;
			if (c & 0x01) d[7] = color;
			d += 8;
		}
		if (i != 0) {
			c = *cp++;
			switch (i) {
			case 7: if (c & 0x02) d[6] = color;
				/* FALLTHROUGH */
			case 6: if (c & 0x04) d[5] = color;
				/* FALLTHROUGH */
			case 5: if (c & 0x08) d[4] = color;
				/* FALLTHROUGH */
			case 4: if (c & 0x10) d[3] = color;
				/* FALLTHROUGH */
			case 3: if (c & 0x20) d[2] = color;
				/* FALLTHROUGH */
			case 2: if (c & 0x40) d[1] = color;
				/* FALLTHROUGH */
			case 1: if (c & 0x80) d[0] = color;
				/* FALLTHROUGH */
			default: break;
			}
		}
		bitmap += bytesPerWidth;
	}
}

void accessor_reverse_8bpp(TFrameBuffer *p, u_int sx, u_int sy,
			   u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, w, h;
	uint8_t *d;

	w = min(sx + ex, p->width);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = (uint8_t *)(p->memory + (y * p->bytesPerLine + sx));
		for (x = sx; x < w; x++)
			*d++ ^= color;
	}
}
#endif /* ENABLE_8BPP */

#ifdef ENABLE_15BPP
void accessor_fill_15bpp(TFrameBuffer *p, u_int sx, u_int sy,
			 u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, w, h;
	uint16_t *d, trueColor;
#ifdef ENABLE_SPLASH_SCREEN
	bool black;
#endif

	trueColor = palette_getTrueColor15(color);
#ifdef ENABLE_SPLASH_SCREEN
	black = (gSplash.enable && trueColor == 0);
#endif
	w = min(sx + ex, p->width);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = (uint16_t *)(p->memory + (y * p->bytesPerLine + sx * 2));
		for (x = sx; x < w; x++) {
#ifdef ENABLE_SPLASH_SCREEN
			if (black && SPLASH_INRANGE(gSplash, x, y))
				*d++ = SPLASH_GET_PIXEL_15(gSplash, x, y);
			else
				*d++ = trueColor;
#else
			*d++ = trueColor;
#endif
		}
	}
}

void accessor_overlay_15bpp(TFrameBuffer *p, const u_char *bitmap,
			    u_int bytesPerWidth, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color)
{
	u_int i, y, h;
	const u_char *cp;
	u_char c;
	uint16_t *d, trueColor;

	trueColor = palette_getTrueColor15(color);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		cp = bitmap;
		d = (uint16_t *)(p->memory + (y * p->bytesPerLine + sx * 2));
		for (i = ex; i >= 8; i -= 8) {
			c = *cp++;
			if (c & 0x80) d[0] = trueColor;
			if (c & 0x40) d[1] = trueColor;
			if (c & 0x20) d[2] = trueColor;
			if (c & 0x10) d[3] = trueColor;
			if (c & 0x08) d[4] = trueColor;
			if (c & 0x04) d[5] = trueColor;
			if (c & 0x02) d[6] = trueColor;
			if (c & 0x01) d[7] = trueColor;
			d += 8;
		}
		if (i != 0) {
			c = *cp++;
			switch (i) {
			case 7: if (c & 0x02) d[6] = trueColor;
				/* FALLTHROUGH */
			case 6: if (c & 0x04) d[5] = trueColor;
				/* FALLTHROUGH */
			case 5: if (c & 0x08) d[4] = trueColor;
				/* FALLTHROUGH */
			case 4: if (c & 0x10) d[3] = trueColor;
				/* FALLTHROUGH */
			case 3: if (c & 0x20) d[2] = trueColor;
				/* FALLTHROUGH */
			case 2: if (c & 0x40) d[1] = trueColor;
				/* FALLTHROUGH */
			case 1: if (c & 0x80) d[0] = trueColor;
				/* FALLTHROUGH */
			default: break;
			}
		}
		bitmap += bytesPerWidth;
	}
}

void accessor_reverse_15bpp(TFrameBuffer *p, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, w, h;
	uint16_t *d, trueColor;

	trueColor = palette_getTrueColor15(color);
	w = min(sx + ex, p->width);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = (uint16_t *)(p->memory + (y * p->bytesPerLine + sx * 2));
		for (x = sx; x < w; x++)
			*d++ ^= trueColor;
	}
}
#endif /* ENABLE_15BPP */

#ifdef ENABLE_16BPP
void accessor_fill_16bpp(TFrameBuffer *p, u_int sx, u_int sy,
			 u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, w, h;
	uint16_t *d, trueColor;
#ifdef ENABLE_SPLASH_SCREEN
	bool black;
#endif

	trueColor = palette_getTrueColor16(color);
#ifdef ENABLE_SPLASH_SCREEN
	black = (gSplash.enable && trueColor == 0);
#endif
	w = min(sx + ex, p->width);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = (uint16_t *)(p->memory + (y * p->bytesPerLine + sx * 2));
		for (x = sx; x < w; x++) {
#ifdef ENABLE_SPLASH_SCREEN
			if (black && SPLASH_INRANGE(gSplash, x, y))
				*d++ = SPLASH_GET_PIXEL_16(gSplash, x, y);
			else
				*d++ = trueColor;
#else
			*d++ = trueColor;
#endif
		}
	}
}

void accessor_overlay_16bpp(TFrameBuffer *p, const u_char *bitmap,
			    u_int bytesPerWidth, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color)
{
	u_int i, y, h;
	const u_char *cp;
	u_char c;
	uint16_t *d, trueColor;

	trueColor = palette_getTrueColor16(color);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		cp = bitmap;
		d = (uint16_t *)(p->memory + (y * p->bytesPerLine + sx * 2));
		for (i = ex; i >= 8; i -= 8) {
			c = *cp++;
			if (c & 0x80) d[0] = trueColor;
			if (c & 0x40) d[1] = trueColor;
			if (c & 0x20) d[2] = trueColor;
			if (c & 0x10) d[3] = trueColor;
			if (c & 0x08) d[4] = trueColor;
			if (c & 0x04) d[5] = trueColor;
			if (c & 0x02) d[6] = trueColor;
			if (c & 0x01) d[7] = trueColor;
			d += 8;
		}
		if (i != 0) {
			c = *cp++;
			switch (i) {
			case 7: if (c & 0x02) d[6] = trueColor;
				/* FALLTHROUGH */
			case 6: if (c & 0x04) d[5] = trueColor;
				/* FALLTHROUGH */
			case 5: if (c & 0x08) d[4] = trueColor;
				/* FALLTHROUGH */
			case 4: if (c & 0x10) d[3] = trueColor;
				/* FALLTHROUGH */
			case 3: if (c & 0x20) d[2] = trueColor;
				/* FALLTHROUGH */
			case 2: if (c & 0x40) d[1] = trueColor;
				/* FALLTHROUGH */
			case 1: if (c & 0x80) d[0] = trueColor;
				/* FALLTHROUGH */
			default: break;
			}
		}
		bitmap += bytesPerWidth;
	}
}

void accessor_reverse_16bpp(TFrameBuffer *p, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, w, h;
	uint16_t *d, trueColor;

	trueColor = palette_getTrueColor16(color);
	w = min(sx + ex, p->width);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = (uint16_t *)(p->memory + (y * p->bytesPerLine + sx * 2));
		for (x = sx; x < w; x++)
			*d++ ^= trueColor;
	}
}
#endif /* ENABLE_16BPP */

#ifdef ENABLE_24BPP
void accessor_fill_24bpp(TFrameBuffer *p, u_int sx, u_int sy,
			 u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, w, h;
	uint8_t *d, t, m, b;
#ifdef ENABLE_SPLASH_SCREEN
	bool black;
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
	palette_getTrueColor24(color, &b, &m, &t);
#elif __BYTE_ORDER == __BIG_ENDIAN
	palette_getTrueColor24(color, &t, &m, &b);
#else
	#error unknown byte order
#endif
#ifdef ENABLE_SPLASH_SCREEN
	black = (gSplash.enable && t == 0 && m == 0 && b == 0);
#endif
	w = min(sx + ex, p->width);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = (uint8_t *)(p->memory + (y * p->bytesPerLine + sx * 3));
		for (x = sx; x < w; x++) {
#ifdef ENABLE_SPLASH_SCREEN
			if (black && SPLASH_INRANGE(gSplash, x, y)) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
				*d++ = SPLASH_GET_PIXEL_24(gSplash, x, y, 2);
				*d++ = SPLASH_GET_PIXEL_24(gSplash, x, y, 1);
				*d++ = SPLASH_GET_PIXEL_24(gSplash, x, y, 0);
#elif __BYTE_ORDER == __BIG_ENDIAN
				*d++ = SPLASH_GET_PIXEL_24(gSplash, x, y, 0);
				*d++ = SPLASH_GET_PIXEL_24(gSplash, x, y, 1);
				*d++ = SPLASH_GET_PIXEL_24(gSplash, x, y, 2);
#else
	#error unknown byte order
#endif
			} else {
				*d++ = t;
				*d++ = m;
				*d++ = b;
			}
#else
			*d++ = t;
			*d++ = m;
			*d++ = b;
#endif
		}
	}
}

void accessor_overlay_24bpp(TFrameBuffer *p, const u_char *bitmap,
			    u_int bytesPerWidth, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color)
{
	u_int i, y, h;
	const u_char *cp;
	u_char c;
	uint8_t *d, t, m, b;

#if __BYTE_ORDER == __LITTLE_ENDIAN
	palette_getTrueColor24(color, &b, &m, &t);
#elif __BYTE_ORDER == __BIG_ENDIAN
	palette_getTrueColor24(color, &t, &m, &b);
#else
	#error unknown byte order
#endif
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		cp = bitmap;
		d = (uint8_t *)(p->memory + (y * p->bytesPerLine + sx * 3));
		for (i = ex; i >= 8; i -= 8) {
			c = *cp++;
			if (c & 0x80) d[0x00] = t, d[0x01] = m, d[0x02] = b;
			if (c & 0x40) d[0x03] = t, d[0x04] = m, d[0x05] = b;
			if (c & 0x20) d[0x06] = t, d[0x07] = m, d[0x08] = b;
			if (c & 0x10) d[0x09] = t, d[0x0a] = m, d[0x0b] = b;
			if (c & 0x08) d[0x0c] = t, d[0x0d] = m, d[0x0e] = b;
			if (c & 0x04) d[0x0f] = t, d[0x10] = m, d[0x11] = b;
			if (c & 0x02) d[0x12] = t, d[0x13] = m, d[0x14] = b;
			if (c & 0x01) d[0x15] = t, d[0x16] = m, d[0x17] = b;
			d += 24;
		}
		if (i != 0) {
			c = *cp++;
			switch (i) {
			case 7: if (c & 0x02)
					d[0x12] = t, d[0x13] = m, d[0x14] = b;
				/* FALLTHROUGH */
			case 6: if (c & 0x04)
					d[0x0f] = t, d[0x10] = m, d[0x11] = b;
				/* FALLTHROUGH */
			case 5: if (c & 0x08)
					d[0x0c] = t, d[0x0d] = m, d[0x0e] = b;
				/* FALLTHROUGH */
			case 4: if (c & 0x10)
					d[0x09] = t, d[0x0a] = m, d[0x0b] = b;
				/* FALLTHROUGH */
			case 3: if (c & 0x20)
					d[0x06] = t, d[0x07] = m, d[0x08] = b;
				/* FALLTHROUGH */
			case 2: if (c & 0x40)
					d[0x03] = t, d[0x04] = m, d[0x05] = b;
				/* FALLTHROUGH */
			case 1: if (c & 0x80)
					d[0x00] = t, d[0x01] = m, d[0x02] = b;
				/* FALLTHROUGH */
			default: break;
			}
		}
		bitmap += bytesPerWidth;
	}
}

void accessor_reverse_24bpp(TFrameBuffer *p, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, w, h;
	uint8_t *d, t, m, b;

#if __BYTE_ORDER == __LITTLE_ENDIAN
	palette_getTrueColor24(color, &b, &m, &t);
#elif __BYTE_ORDER == __BIG_ENDIAN
	palette_getTrueColor24(color, &t, &m, &b);
#else
	#error unknown byte order
#endif
	w = min(sx + ex, p->width);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = (uint8_t *)(p->memory + (y * p->bytesPerLine + sx * 3));
		for (x = sx; x < w; x++) {
			*d++ ^= t;
			*d++ ^= m;
			*d++ ^= b;
		}
	}
}
#endif /* ENABLE_24BPP */

#ifdef ENABLE_32BPP
void accessor_fill_32bpp(TFrameBuffer *p, u_int sx, u_int sy,
			 u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, w, h;
	uint32_t *d, trueColor;
#ifdef ENABLE_SPLASH_SCREEN
	bool black;
#endif

	trueColor = palette_getTrueColor32(color);
#ifdef ENABLE_SPLASH_SCREEN
	black = (gSplash.enable && trueColor == 0);
#endif
	w = min(sx + ex, p->width);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = (uint32_t *)(p->memory + (y * p->bytesPerLine + sx * 4));
		for (x = sx; x < w; x++) {
#ifdef ENABLE_SPLASH_SCREEN
			if (black && SPLASH_INRANGE(gSplash, x, y))
				*d++ = SPLASH_GET_PIXEL_32(gSplash, x, y);
			else
				*d++ = trueColor;
#else
			*d++ = trueColor;
#endif
		}
	}
}

void accessor_overlay_32bpp(TFrameBuffer *p, const u_char *bitmap,
			    u_int bytesPerWidth, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color)
{
	u_int i, y, h;
	const u_char *cp;
	u_char c;
	uint32_t *d, trueColor;

	trueColor = palette_getTrueColor32(color);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		cp = bitmap;
		d = (uint32_t *)(p->memory + (y * p->bytesPerLine + sx * 4));
		for (i = ex; i >= 8; i -= 8) {
			c = *cp++;
			if (c & 0x80) d[0] = trueColor;
			if (c & 0x40) d[1] = trueColor;
			if (c & 0x20) d[2] = trueColor;
			if (c & 0x10) d[3] = trueColor;
			if (c & 0x08) d[4] = trueColor;
			if (c & 0x04) d[5] = trueColor;
			if (c & 0x02) d[6] = trueColor;
			if (c & 0x01) d[7] = trueColor;
			d += 8;
		}
		if (i != 0) {
			c = *cp++;
			switch (i) {
			case 7: if (c & 0x02) d[6] = trueColor;
				/* FALLTHROUGH */
			case 6: if (c & 0x04) d[5] = trueColor;
				/* FALLTHROUGH */
			case 5: if (c & 0x08) d[4] = trueColor;
				/* FALLTHROUGH */
			case 4: if (c & 0x10) d[3] = trueColor;
				/* FALLTHROUGH */
			case 3: if (c & 0x20) d[2] = trueColor;
				/* FALLTHROUGH */
			case 2: if (c & 0x40) d[1] = trueColor;
				/* FALLTHROUGH */
			case 1: if (c & 0x80) d[0] = trueColor;
				/* FALLTHROUGH */
			default: break;
			}
		}
		bitmap += bytesPerWidth;
	}
}

void accessor_reverse_32bpp(TFrameBuffer *p, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, w, h;
	uint32_t *d, trueColor;

	trueColor = palette_getTrueColor32(color);
	w = min(sx + ex, p->width);
	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = (uint32_t *)(p->memory + (y * p->bytesPerLine + sx * 4));
		for (x = sx; x < w; x++)
			*d++ ^= trueColor;
	}
}
#endif /* ENABLE_32BPP */

#ifdef ENABLE_VGA16FB
void accessor_fill_vga16fb(TFrameBuffer *p, u_int sx, u_int sy,
			   u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, h;
	u_char *d;
	u_int sofs, eofs;
	unsigned char mask, emask;

	/* operation */
	PORT_OUTBYTE(0x3ce, 0x03);
	PORT_OUTBYTE(0x3cf, 0x00);
	/* color */
	PORT_OUTBYTE(0x3ce, 0x00);
	PORT_OUTBYTE(0x3cf, color & 0x0f);
	/* bit mask */
	PORT_OUTBYTE(0x3ce, 0x08);
	PORT_OUTBYTE(0x3cf, 0xff);

	sofs = sx % 8;
	eofs = (sx + ex) % 8;
	emask = 0xff00 >> eofs;

	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = p->memory + y * p->bytesPerLine + sx / 8;
		mask = 0x00ff >> sofs;
		for (x = ex + sofs; x >= 8; x -= 8) {
			PORT_OUTBYTE(0x3cf, mask);
			*d |= 1;
			d++;
			mask = 0x00ff;
		}
		if (x != 0) {
			PORT_OUTBYTE(0x3cf, mask & emask);
			*d |= 1;
		}
	}
}

void accessor_overlay_vga16fb(TFrameBuffer *p, const u_char *bitmap,
			      u_int bytesPerWidth, u_int sx, u_int sy,
			      u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, h;
	const u_char *cp;
	u_char *d;
	u_int sofs, eofs, sxs8, sxe8;
	unsigned char mask, emask, smask;

	/* operation */
	PORT_OUTBYTE(0x3ce, 0x03);
	PORT_OUTBYTE(0x3cf, 0x00);
	/* color */
	PORT_OUTBYTE(0x3ce, 0x00);
	PORT_OUTBYTE(0x3cf, color & 0x0f);
	/* bit mask */
	PORT_OUTBYTE(0x3ce, 0x08);

	sofs = sx % 8;
	eofs = (sx + ex) % 8;
	sxs8 = sx / 8;
	sxe8 = (sx + ex + 7) / 8;
	eofs = eofs ? eofs : 8;
	smask = 0x00ff << sofs;
	emask = 0xff00 >> eofs;

	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		cp = bitmap;
		d = p->memory + y * p->bytesPerLine + sxs8;
		if (sxs8 + 1 == sxe8) {
			mask = ((*cp >> sofs) & emask);
			PORT_OUTBYTE(0x3cf, mask);
			*d |= 1;
		} else {
			mask = (*cp++ >> sofs);
			PORT_OUTBYTE(0x3cf, mask);
			*d |= 1;
			d++;
			for (x = sxs8 + 1; x < sxe8 - 1; x++) {
				mask = (*cp >> sofs) |
				       (*(cp - 1) << (8 - sofs));
				cp++;
				PORT_OUTBYTE(0x3cf, mask);
				*d |= 1;
				d++;
			}
			if (eofs <= sofs) {
				if (sofs == 0)
					mask = *cp;
				else
					mask = (*(cp - 1) << (8 - sofs)) &
					       emask;
			} else
				mask = ((*cp >> sofs) |
					(*(cp - 1) << (8 - sofs))) &
				       emask;
			PORT_OUTBYTE(0x3cf, mask);
			*d |= 1;
		}
		bitmap += bytesPerWidth;
	}
}

void accessor_reverse_vga16fb(TFrameBuffer *p, u_int sx, u_int sy,
			      u_int ex, u_int ey, uint8_t color)
{
	u_int x, y, h;
	u_char *d;
	u_int sofs, eofs;
	unsigned char mask, emask;

	/* operation */
	PORT_OUTBYTE(0x3ce, 0x03);
	PORT_OUTBYTE(0x3cf, 0x18);
	/* color */
	PORT_OUTBYTE(0x3ce, 0x00);
	PORT_OUTBYTE(0x3cf, color & 0x0f);
	/* bit mask */
	PORT_OUTBYTE(0x3ce, 0x08);

	sofs = sx % 8;
	eofs = (sx + ex) % 8;
	emask = 0xff00 >> eofs;

	h = min(sy + ey, p->height);
	for (y = sy; y < h; y++) {
		d = p->memory + y * p->bytesPerLine + sx / 8;
		mask = 0x00ff >> sofs;
		for (x = ex + sofs; x >= 8; x -= 8) {
			PORT_OUTBYTE(0x3cf, mask);
			*d |= 1;
			mask = 0x00ff;
			d++;
		}
		if (x != 0) {
			PORT_OUTBYTE(0x3cf, emask);
			*d |= 1;
		}
	}
}
#endif /* ENABLE_VGA16FB */

