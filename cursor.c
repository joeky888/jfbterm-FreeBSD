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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>
#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cursor.h"
#include "font.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "palette.h"
#include "vterm.h"

TCursor cursor;
TCursor mouseCursor;

static bool initialized;

static void finalize(void);
static void finalizer(void);
static void configCursorType(TCursor *c, const char *config);
static void configCursorInterval(TCursor *c, const char *config);
static void configCursorColor(TCursor *c, const char *config);
static void configCursorHeight(TCursor *c, const char *config);
static void draw(TVterm *p, TCursor *c);

void cursor_initialize(void)
{
	assert(!initialized);

	atexit(finalizer);
	initialized = true;
	/* Cursor */
	cursor.style = CURSOR_STYLE_NORMAL;
	cursor.x = 0;
	cursor.y = 0;
	cursor.on = true;
	cursor.toggle = true;
	cursor.mouse = false;
	cursor.shown = false;
	cursor.wide = false;
	cursor.width = gFontsWidth;
	cursor.height = gFontsHeight;
	cursor.interval = DEFAULT_CURSOR_INTERVAL;
	cursor.color = palette_ansiToVGA(DEFAULT_CURSOR_COLOR);
	cursor_resetTimer(&cursor);
	/* Mouse cursor */
	mouseCursor.style = CURSOR_STYLE_NORMAL;
	mouseCursor.x = 0;
	mouseCursor.y = 0;
	mouseCursor.on = true;
	mouseCursor.toggle = true;
	mouseCursor.mouse = true;
	mouseCursor.shown = false;
	mouseCursor.wide = false;
	mouseCursor.width = gFontsWidth;
	mouseCursor.height = gFontsHeight;
	mouseCursor.interval = DEFAULT_CURSOR_INTERVAL;
	mouseCursor.color = palette_ansiToVGA(DEFAULT_CURSOR_COLOR);
	cursor_resetTimer(&mouseCursor);
}

static void finalize(void)
{
	assert(initialized);

	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

static void configCursorType(TCursor *c, const char *config)
{
	bool found;
	int i;

	static const struct {
		const char *key;
		const CURSOR_STYLE style;
	} list[] = {
		{ "Normal", CURSOR_STYLE_NORMAL },
		{ "Blink",  CURSOR_STYLE_BLINK  },
		{ NULL,     CURSOR_STYLE_NORMAL }
	};

	assert(initialized);
	assert(c != NULL);

	c->style = CURSOR_STYLE_NORMAL;
	if (config != NULL) {
		found = false;
		for (i = 0; list[i].key != NULL; i++) {
			if (strcasecmp(list[i].key, config) == 0) {
				c->style = list[i].style;
				found = true;
				break;
			}
		}
		if (!found)
			warnx("Invalid cursor type: %s", config);
	}
}

static void configCursorInterval(TCursor *c, const char *config)
{
	int interval;

	assert(initialized);
	assert(c != NULL);

	if (config != NULL) {
		interval = atoi(config);
		if (interval > 0)
			c->interval = atoi(config);
		else
			warnx("Invalid cursor interval: %s", config);
	}
}

static void configCursorColor(TCursor *c, const char *config)
{
	int ansiColor;

	assert(initialized);
	assert(c != NULL);

	if (config != NULL) {
		ansiColor = atoi(config);
		if (ansiColor >= 0 && ansiColor <= 15)
			c->color = palette_ansiToVGA(ansiColor);
		else
			warnx("Invalid cursor color: %s", config);
	}
}

static void configCursorHeight(TCursor *c, const char *config)
{
	int height;

	assert(initialized);
	assert(c != NULL);

	if (config != NULL) {
		height = atoi(config);
		if (height >= 1 && height <= 100) {
			height = gFontsHeight / (100 / height);
			if (height < 2)
				height = 2;
			c->height = height;
		 } else
			warnx("Invalid cursor height: %s", config);
	}
}

void cursor_configure(TCaps *caps)
{
	const char *config;

	assert(initialized);
	assert(caps != NULL);

	/* Cursor */
	config = caps_findFirst(caps, "cursor.type");
	configCursorType(&cursor, config);
	config = caps_findFirst(caps, "cursor.interval");
	configCursorInterval(&cursor, config);
	config = caps_findFirst(caps, "cursor.color");
	configCursorColor(&cursor, config);
	config = caps_findFirst(caps, "cursor.height");
	configCursorHeight(&cursor, config);
	/* Mouse cursor */
	config = caps_findFirst(caps, "mouse.cursor.type");
	configCursorType(&mouseCursor, config);
	config = caps_findFirst(caps, "mouse.cursor.interval");
	configCursorInterval(&mouseCursor, config);
	config = caps_findFirst(caps, "mouse.cursor.color");
	configCursorColor(&mouseCursor, config);
	config = caps_findFirst(caps, "mouse.cursor.height");
	configCursorHeight(&mouseCursor, config);
}

void cursor_resetTimer(TCursor *c)
{
	assert(initialized);
	assert(c != NULL);

	gettimeofday(&(c->timer), NULL);
}

bool cursor_isTimeout(TCursor *c)
{
	struct timeval now, diff;
	u_int elapsedTime;

	assert(initialized);
	assert(c != NULL);

	timerclear(&diff);
	gettimeofday(&now, NULL);
	timersub(&now, &(c->timer), &diff);
	elapsedTime = diff.tv_sec * 10 + diff.tv_usec / 100000; /* 0.1 sec. */
	return elapsedTime >= c->interval;
}

void cursor_toggle(TVterm *p, TCursor *c)
{
	assert(initialized);
	assert(p != NULL);
	assert(c != NULL);

	cursor_show(p, c, c->toggle);
	c->toggle = !c->toggle;
	cursor_resetTimer(c);
}

static void draw(TVterm *p, TCursor *c)
{
	sigset_t set, oldset;
	int x, y, height, width;

	assert(initialized);
	assert(p != NULL);
	assert(c != NULL);

	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigprocmask(SIG_SETMASK, &set, &oldset);
	if (p->active) {
		x = gFontsWidth * c->x;
		y = gFontsHeight * c->y + (gFontsHeight - c->height);
		width = c->width + (c->wide ? c->width : 0);
		height = c->height;
		gFramebuffer.accessor.reverse(&gFramebuffer,
					      x, y,
					      width, height,
					      c->color);
	}
	sigprocmask(SIG_SETMASK, &oldset, NULL);
}

void cursor_show(TVterm *p, TCursor *c, const bool show)
{
	bool shown;

	assert(initialized);
	assert(p != NULL);
	assert(c != NULL);

	shown = show;
	if (!p->active)
		return;
	if (!c->on)
		shown = false;
	if (!c->mouse && (keyboard_isScrollLocked() || p->top != 0))
		shown = false;
	if (c->shown != shown) {
		draw(p, c);
		c->shown = shown;
	}
}

