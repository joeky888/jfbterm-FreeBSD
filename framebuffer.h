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

#ifndef INCLUDE_FRAMEBUFFER_H
#define INCLUDE_FRAMEBUFFER_H

#include <sys/param.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#if defined (__linux__)
#include <linux/fb.h>
#elif defined (__FreeBSD__)
#include <osreldate.h>
#if __FreeBSD_version >= 410000
#include <sys/fbio.h>
#else
#include <machine/console.h>
#endif
#elif defined (__NetBSD__) || defined (__OpenBSD__)
#include <time.h>
#include <dev/wscons/wsconsio.h>
#endif

#include "getcap.h"

#if defined (__linux__)
#define PORT_OUTBYTE(reg, val) outb(val, reg)
#define PORT_INBYTE(reg) inb(reg)
#elif defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__)
#define PORT_OUTBYTE(reg, val) outb(reg, val)
#define PORT_INBYTE(reg) inb(reg)
#endif

struct Raw_TFrameBuffer;

typedef struct Raw_TFrameBufferAccessor {
	u_int bitsPerPixel;
#if defined (__linux__)
	u_int type;
	u_int visual;
#elif defined (__FreeBSD__)
	u_int memoryModel;
#endif
	void (*fill)(struct Raw_TFrameBuffer *p, u_int sx, u_int sy,
		     u_int ex, u_int ey, uint8_t color);
	void (*overlay)(struct Raw_TFrameBuffer *p, const u_char *bitmap,
			u_int bytesPerWidth, u_int sx, u_int sy,
			u_int ex, u_int ey, uint8_t color);
	void (*reverse)(struct Raw_TFrameBuffer *p, u_int sx, u_int sy,
			u_int ex, u_int ey, uint8_t color);
} TFrameBufferAccessor;

typedef struct Raw_TFrameBuffer {
	char device[MAXPATHLEN];      /* framebuffer device name */
	int fd;                       /* framebuffer file descriptor */
	u_int height;                 /* height */
	u_int width;                  /* width */
	u_int bytesPerLine;           /* bytes per line */
	/* memory */
	u_long offset;
	u_long length;
	u_char *memory;
	/* function hooks */
	TFrameBufferAccessor accessor;
} TFrameBuffer;

extern TFrameBuffer gFramebuffer;

void framebuffer_initialize(void);
void framebuffer_configure(TCaps *caps);
#if defined (__linux__)
void framebuffer_getColorMap(struct fb_cmap *cmap);
#elif defined (__FreeBSD__)
void framebuffer_getColorMap(video_color_palette_t *cmap);
#elif defined (__NetBSD__) || defined (__OpenBSD__)
void framebuffer_getColorMap(struct wsdisplay_cmap *cmap);
#endif
#if defined (__linux__)
void framebuffer_setColorMap(struct fb_cmap *cmap);
#elif defined (__FreeBSD__)
void framebuffer_setColorMap(video_color_palette_t *cmap);
#elif defined (__NetBSD__) || defined (__OpenBSD__)
void framebuffer_setColorMap(struct wsdisplay_cmap *cmap);
#endif
bool framebuffer_setBlank(const int blank);
void framebuffer_open(void);
void framebuffer_reset(void);

#endif /* INCLUDE_FRAMEBUFFER_H */

