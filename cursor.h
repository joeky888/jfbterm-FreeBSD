/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
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

#ifndef INCLUDE_CURSOR_H
#define INCLUDE_CURSOR_H

#include <sys/time.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#include "getcap.h"
#include "vterm.h"

#define DEFAULT_CURSOR_INTERVAL (4)     /* 0.4 sec. */
#define DEFAULT_CURSOR_COLOR    (15)    /* Light White */

typedef enum {
	CURSOR_STYLE_NORMAL,
	CURSOR_STYLE_BLINK
} CURSOR_STYLE;

typedef struct Raw_TCursor {
	CURSOR_STYLE style;
	u_int x;
	u_int y;
	bool on;
	bool toggle;
	bool mouse;
	bool shown;
	bool wide;
	u_short width;
	u_short height;
	u_int interval;
	uint8_t color;
	struct timeval timer;
} TCursor;

extern TCursor cursor;
extern TCursor mouseCursor;

void cursor_initialize(void);
void cursor_configure(TCaps *caps);
void cursor_resetTimer(TCursor *c);
bool cursor_isTimeout(TCursor *c);
void cursor_toggle(TVterm *p, TCursor *c);
void cursor_show(TVterm *p, TCursor *c, const bool show);

#endif /* INCLUDE_CURSOR_H */

