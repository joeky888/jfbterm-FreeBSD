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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#include "framebuffer.h"
#include "getcap.h"
#include "screensaver.h"

#if defined (__linux__)
#define UNBLANK (FB_BLANK_UNBLANK)
#elif defined (__FreeBSD__)
#define UNBLANK (V_DISPLAY_ON)
#elif defined (__NetBSD__) || defined (__OpenBSD__)
#define UNBLANK (WSDISPLAYIO_VIDEO_ON)
#elif
	#error not implement
#endif

#define DEFAULT_SCREENSAVER_TIMEOUT (5) /* 5 min. */

static struct {
	int mode;
	int timeout;
	struct timeval timer;
	bool status;
	bool enable;
} screensaver = {
	UNBLANK,
	DEFAULT_SCREENSAVER_TIMEOUT,
	{ 0, 0 },
	false,
	false
};

static bool initialized;

static void finalize(void);
static void finalizer(void);
static void configMode(const char *config);
static void configTimeout(const char *config);

void screensaver_initialize(void)
{
	assert(!initialized);

	atexit(finalizer);
	initialized = true;
	screensaver_resetTimer();
}

static void finalize(void)
{
	assert(initialized);

	if (screensaver_isRunning())
		screensaver_execute(false);
	screensaver.mode   = UNBLANK;
	screensaver.enable = false;
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

static void configMode(const char *config)
{
	bool found;
	int i;

	static const struct {
		const char *key;
		const int mode;
		const bool enable;
	} list[] = {
#if defined (__linux__)
		{ "Blank",        FB_BLANK_NORMAL,        true  },
		{ "VSyncSuspend", FB_BLANK_VSYNC_SUSPEND, true  },
		{ "HSyncSuspend", FB_BLANK_HSYNC_SUSPEND, true  },
		{ "Powerdown",    FB_BLANK_POWERDOWN,     true  },
#elif defined (__FreeBSD__)
		{ "Blank",        V_DISPLAY_BLANK,        true  },
		{ "StandBy",      V_DISPLAY_STAND_BY,     true  },
		{ "Suspend",      V_DISPLAY_SUSPEND,      true  },
#elif defined (__NetBSD__) || defined (__OpenBSD__)
		{ "Blank",        WSDISPLAYIO_VIDEO_OFF,  true  },
#endif
		{ "None",         UNBLANK,                false },
		{ NULL,           0,                      false }
	};

	assert(initialized);

	screensaver.mode   = UNBLANK;
	screensaver.enable = false;
	if (config != NULL) {
		found = false;
		for (i = 0; list[i].key != NULL; i++) {
			if (strcasecmp(list[i].key, config) == 0) {
				screensaver.mode   = list[i].mode;
				screensaver.enable = list[i].enable;
				found = true;
				break;
			}
		}
		if (!found)
			warnx("Invalid screensaver mode: %s", config);
	}
}

static void configTimeout(const char *config)
{
	int timeout;

	assert(initialized);

	if (config != NULL) {
		timeout = atoi(config);
		if (timeout > 0)
			screensaver.timeout = timeout;
		else
			warnx("Invalid screensaver timeout: %s", config);
	}
}

void screensaver_configure(TCaps *caps)
{
	const char *config;

	assert(initialized);
	assert(caps != NULL);

	config = caps_findFirst(caps, "screensaver.mode");
	configMode(config);
	config = caps_findFirst(caps, "screensaver.timeout");
	configTimeout(config);
}

bool screensaver_isEnable(void)
{
	assert(initialized);

	return screensaver.enable;
}

bool screensaver_isRunning(void)
{
	assert(initialized);

	return screensaver.status;
}

void screensaver_resetTimer(void)
{
	assert(initialized);

	gettimeofday(&screensaver.timer, NULL);
}

bool screensaver_isTimeout(void)
{
	struct timeval now, diff;
	u_int elapsedTime;

	assert(initialized);

	timerclear(&diff);
	gettimeofday(&now, NULL);
	timersub(&now, &screensaver.timer, &diff);
	elapsedTime = diff.tv_sec / 60; /* min. */
	return elapsedTime >= screensaver.timeout;
}

void screensaver_execute(const bool status)
{
	int mode;

	assert(initialized);

	if (screensaver.enable && screensaver.status != status) {
		mode = status ? screensaver.mode : UNBLANK;
		if (!framebuffer_setBlank(mode))
			warnx("Could not set blank.");
		screensaver.status = status;
	}
	screensaver_resetTimer();
}

