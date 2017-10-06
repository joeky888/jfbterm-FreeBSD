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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined (__linux__)
#include <sys/kd.h>
#elif defined (__FreeBSD__)
#include <osreldate.h>
#if __FreeBSD_version >= 410000
#include <sys/kbio.h>
#else
#include <machine/console.h>
#endif
#elif defined (__NetBSD__) || defined (__OpenBSD__)
#ifndef _DEV_WSCONS_WSDISPLAY_USL_IO_H_
#include <dev/wscons/wsdisplay_usl_io.h>
#define _DEV_WSCONS_WSDISPLAY_USL_IO_H_
#endif
#endif

#include "bell.h"
#include "framebuffer.h"
#include "getcap.h"
#include "palette.h"

typedef enum {
	BELL_STYLE_OFF,
	BELL_STYLE_ON,
	BELL_STYLE_VISIBLE
} BELL_STYLE;

#define DEFAULT_BELL_DURATION (125)     /* 125ms */
#define DEFAULT_BELL_FREQ     (750)     /* 750hz */
#define DEFAULT_BELL_COLOR    (15)      /* Light White */

#define BELL_DURATION_MIN     (0)       /* 0ms */
#define BELL_DURATION_MAX     (2000)    /* 2000ms */

#define BELL_FREQ_MIN         (21)      /* 21hz */
#define BELL_FREQ_MAX         (32766)   /* 32766hz */

#define BELL_HZ_TO_CT(x) ((x) ? (1193180 / (x)) : 0)

static struct {
	BELL_STYLE style;
	int duration;
	int freq;
	uint8_t color;
} bell = {
	BELL_STYLE_OFF,
	DEFAULT_BELL_DURATION,
	DEFAULT_BELL_FREQ,
	DEFAULT_BELL_COLOR
};

static bool initialized;

static void finalize(void);
static void finalizer(void);
static void configStyle(const char *config);
static void configDuration(const char *config);
static void configFreq(const char *config);
static void configColor(const char *config);

void bell_initialize(void)
{
	assert(!initialized);

	atexit(finalizer);
	initialized = true;
}

static void finalize(void)
{
	assert(initialized);

	bell.style = BELL_STYLE_OFF;
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

static void configStyle(const char *config)
{
	bool found;
	int i;

	static const struct {
		const char *key;
		const BELL_STYLE style;
	} list[] = {
		{ "On",      BELL_STYLE_ON      },
		{ "Off",     BELL_STYLE_OFF     },
		{ "Visible", BELL_STYLE_VISIBLE },
		{ NULL,      BELL_STYLE_OFF     }
	};

	assert(initialized);

	bell.style = BELL_STYLE_OFF;
	if (config != NULL) {
		found = false;
		for (i = 0; list[i].key != NULL; i++) {
			if (strcasecmp(list[i].key, config) == 0) {
				bell.style = list[i].style;
				found = true;
				break;
			}
		}
		if (!found)
			warnx("Invalid bell style: %s", config);
	}
}

static void configDuration(const char *config)
{
	int duration;

	assert(initialized);

	if (config != NULL) {
		duration = atoi(config);
		if (duration >= BELL_DURATION_MIN &&
		    duration <= BELL_DURATION_MAX)
			bell.duration = duration;
		else
			warnx("Invalid bell duration: %s", config);
	}
}

static void configFreq(const char *config)
{
	int freq;

	assert(initialized);

	if (config != NULL) {
		freq = atoi(config);
		if (freq >= BELL_FREQ_MIN && freq <= BELL_FREQ_MAX)
			bell.freq = freq;
		else
			warnx("Invalid bell frequency: %s", config);
	}
}

static void configColor(const char *config)
{
	int ansiColor;

	assert(initialized);

	if (config != NULL) {
		ansiColor = atoi(config);
		if (ansiColor >= 0 && ansiColor <= 15)
			bell.color = palette_ansiToVGA(ansiColor);
		else
			warnx("Invalid bell color: %s", config);
	}
}

void bell_configure(TCaps *caps)
{
	const char *config;

	assert(initialized);
	assert(caps != NULL);

	config = caps_findFirst(caps, "bell.style");
	configStyle(config);
	config = caps_findFirst(caps, "bell.duration");
	configDuration(config);
	config = caps_findFirst(caps, "bell.freq");
	configFreq(config);
	config = caps_findFirst(caps, "bell.color");
	configColor(config);
}

void bell_setDuration(const int value)
{
	assert(initialized);

	if (value >= BELL_DURATION_MIN && value <= BELL_DURATION_MAX)
		bell.duration = value;
}

void bell_setFreq(const int value)
{
	assert(initialized);

	if (value >= BELL_FREQ_MIN && value <= BELL_FREQ_MAX)
		bell.freq = value;
}

void bell_execute(TVterm *p)
{
	sigset_t set, oldset;

	assert(initialized);
	assert(p != NULL);

	switch (bell.style) {
	case BELL_STYLE_OFF:
		break;
	case BELL_STYLE_ON:
		if (p->active) {
			if (ioctl(STDIN_FILENO, KDMKTONE,
			          (bell.duration << 16) |
				   BELL_HZ_TO_CT(bell.freq)) == -1)
				warn("ioctl(KDMKTONE)");
		}
		break;
	case BELL_STYLE_VISIBLE:
		sigemptyset(&set);
		sigaddset(&set, SIGUSR1);
		sigprocmask(SIG_SETMASK, &set, &oldset);
		if (p->active) {
			gFramebuffer.accessor.reverse(&gFramebuffer,
						      0, 0,
						      gFramebuffer.width,
						      gFramebuffer.height,
						      bell.color);
			gFramebuffer.accessor.reverse(&gFramebuffer,
						      0, 0,
						      gFramebuffer.width,
						      gFramebuffer.height,
						      bell.color);
		}
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		break;
	default:
		break;
	}
}

