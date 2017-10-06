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

#ifndef INCLUDE_accessor_H
#define INCLUDE_accessor_H

#include <sys/types.h>
#include <stdint.h>

#include "framebuffer.h"

#ifdef ENABLE_8BPP
void accessor_fill_8bpp(TFrameBuffer *p, u_int sx, u_int sy,
			u_int ex, u_int ey, uint8_t color);
void accessor_overlay_8bpp(TFrameBuffer *p, const u_char *bitmap,
			   u_int bytesPerWidth, u_int sx, u_int sy,
			   u_int ex, u_int ey, uint8_t color);
void accessor_reverse_8bpp(TFrameBuffer *p, u_int sx, u_int sy,
			   u_int ex, u_int ey, uint8_t color);
#endif /* ENABLE_8BPP */

#ifdef ENABLE_15BPP
void accessor_fill_15bpp(TFrameBuffer *p, u_int sx, u_int sy,
			 u_int ex, u_int ey, uint8_t color);
void accessor_overlay_15bpp(TFrameBuffer *p, const u_char *bitmap,
			    u_int bytesPerWidth, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color);
void accessor_reverse_15bpp(TFrameBuffer *p, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color);
#endif /* ENABLE_15BPP */

#ifdef ENABLE_16BPP
void accessor_fill_16bpp(TFrameBuffer *p, u_int sx, u_int sy,
			 u_int ex, u_int ey, uint8_t color);
void accessor_overlay_16bpp(TFrameBuffer *p, const u_char *bitmap,
			    u_int bytesPerWidth, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color);
void accessor_reverse_16bpp(TFrameBuffer *p, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color);
#endif /* ENABLE_16BPP */

#ifdef ENABLE_24BPP
void accessor_fill_24bpp(TFrameBuffer *p, u_int sx, u_int sy,
			 u_int ex, u_int ey, uint8_t color);
void accessor_overlay_24bpp(TFrameBuffer *p, const u_char *bitmap,
			    u_int bytesPerWidth, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color);
void accessor_reverse_24bpp(TFrameBuffer *p, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color);
#endif /* ENABLE_24BPP */

#ifdef ENABLE_32BPP
void accessor_fill_32bpp(TFrameBuffer *p, u_int sx, u_int sy,
			 u_int ex, u_int ey, uint8_t color);
void accessor_overlay_32bpp(TFrameBuffer *p, const u_char *bitmap,
			    u_int bytesPerWidth, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color);
void accessor_reverse_32bpp(TFrameBuffer *p, u_int sx, u_int sy,
			    u_int ex, u_int ey, uint8_t color);
#endif  /* ENABLE_32BPP */

#ifdef ENABLE_VGA16FB
void accessor_fill_vga16fb(TFrameBuffer *p, u_int sx, u_int sy,
			   u_int ex, u_int ey, uint8_t color);
void accessor_overlay_vga16fb(TFrameBuffer *p, const u_char *bitmap,
			      u_int bytesPerWidth, u_int sx, u_int sy,
			      u_int ex, u_int ey, uint8_t color);
void accessor_reverse_vga16fb(TFrameBuffer *p, u_int sx, u_int sy,
			      u_int ex, u_int ey, uint8_t color);
#endif /* ENABLE_VGA16FB */

#endif  /* INCLUDE_accessor_H */

