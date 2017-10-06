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

#ifndef INCLUDE_PALETTE_H
#define INCLUDE_PALETTE_H

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
void palette_initialize(const struct fb_var_screeninfo *fb_var_screeninfo,
			const struct fb_fix_screeninfo *fb_fix_screeninfo);
#elif defined (__FreeBSD__)
void palette_initialize(const video_info_t *video_info,
			const video_adapter_info_t *video_adapter_info);
#elif defined (__NetBSD__) || defined (__OpenBSD__)
void palette_initialize(const struct wsdisplay_fbinfo *wsdisplay_fbinfo,
			const int wsdisplay_type);
#endif
void palette_configure(TCaps *caps);
bool palette_hasColorMap(void);
void palette_reset(void);
void palette_restore(void);
static inline uint8_t palette_ansiToVGA(const int ansiColor);
static inline uint16_t palette_getTrueColor15(const uint8_t color);
static inline uint16_t palette_getTrueColor16(const uint8_t color);
static inline void palette_getTrueColor24(const uint8_t color,
				   uint8_t *c0, uint8_t *c1, uint8_t *c2);
static inline uint32_t palette_getTrueColor32(const uint8_t color);
int palette_getRLength(void);
int palette_getGLength(void);
int palette_getBLength(void);
int palette_getROffset(void);
int palette_getGOffset(void);
int palette_getBOffset(void);
int palette_getRIndex(void);
int palette_getGIndex(void);
int palette_getBIndex(void);
void palette_reverse(bool status);
bool palette_update(const int ansiColor, const char *value);

#endif /* INCLUDE_PALETTE_H */

