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
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#if defined (__linux__)
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>
#elif defined (__FreeBSD__)
#include <osreldate.h>
#if __FreeBSD_version >= 410000
#include <sys/consio.h>
#else
#include <machine/console.h>
#endif
#elif defined (__NetBSD__) || defined (__OpenBSD__)
#include <time.h>
#include <dev/wscons/wsconsio.h>
#ifndef _DEV_WSCONS_WSDISPLAY_USL_IO_H_
#include <dev/wscons/wsdisplay_usl_io.h>
#define _DEV_WSCONS_WSDISPLAY_USL_IO_H_
#endif
#endif

#include "console.h"
#include "privilege.h"

static bool initialized;

static void finalize(void);
static void finalizer(void);
static void acquire(int signum);
static void release(int signum);

static struct {
	int active;
	int mode;
#if defined (__FreeBSD__)
	bool SC_PIXEL_MODE; /* syscons SC_PIXEL_MODE */
#endif
	struct termios *termios;
	struct vt_mode *vt_mode;
	void (*acquireHandler)(int signum);
	void (*releaseHandler)(int signum);
} console;

void console_initialize(void)
{
	struct sigaction act_sigusr1, act_sigusr2;
	struct vt_mode vt_mode;

	assert(!initialized);

	atexit(finalizer);
	initialized = true;

	console.active         = -1;
	console.mode           = -1;
#if defined (__FreeBSD__)
	console.SC_PIXEL_MODE  = false;
#endif
	console.termios        = NULL;
	console.vt_mode        = NULL;
	console.acquireHandler = NULL;
	console.releaseHandler = NULL;

	console.active = console_getActive();
	if (console.active < 1)
		errx(1, "This program must be run from the console.");

	console.mode = console_getMode();

	console.termios = malloc(sizeof(struct termios));
	if (console.termios == NULL)
		err(1, "malloc()");
	if (tcgetattr(STDIN_FILENO, console.termios) == -1) {
		free(console.termios);
		console.termios = NULL;
		err(1, "tcgetattr()");
	}

	console.vt_mode = malloc(sizeof(struct vt_mode));
	if (console.vt_mode == NULL)
		err(1, "malloc()");
	if (ioctl(STDIN_FILENO, VT_GETMODE, console.vt_mode) == -1) {
		free(console.vt_mode);
		console.vt_mode = NULL;
		err(1, "ioctl(VT_GETMODE)");
	}

	ioctl(STDIN_FILENO, VT_RELDISP, VT_ACKACQ);

	bzero(&act_sigusr1, sizeof(act_sigusr1));
	act_sigusr1.sa_handler = release;
	act_sigusr1.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &act_sigusr1, NULL);

	bzero(&act_sigusr2, sizeof(act_sigusr2));
	act_sigusr2.sa_handler = acquire;
	act_sigusr2.sa_flags = SA_RESTART;
	sigaction(SIGUSR2, &act_sigusr2, NULL);

	vt_mode.mode   = VT_PROCESS;
	vt_mode.waitv  = 0;
	vt_mode.relsig = SIGUSR1;
	vt_mode.acqsig = SIGUSR2;
	vt_mode.frsig  = SIGUSR1;
	if (ioctl(STDIN_FILENO, VT_SETMODE, &vt_mode) == -1)
		warn("ioctl(VT_SETMODE)");
#if defined (__linux__)
	puts("\033[?25l");
#endif
}

static void finalize(void)
{
	struct sigaction act_sigusr1, act_sigusr2;

	assert(initialized);

	bzero(&act_sigusr1, sizeof(act_sigusr1));
	act_sigusr1.sa_handler = SIG_DFL;
	sigaction(SIGUSR1, &act_sigusr1, NULL);

	bzero(&act_sigusr2, sizeof(act_sigusr2));
	act_sigusr2.sa_handler = SIG_DFL;
	sigaction(SIGUSR2, &act_sigusr2, NULL);

	ioctl(STDIN_FILENO, VT_RELDISP, 1); /* VT_TRUE */

	if (console.termios != NULL) {
		if (tcsetattr(STDIN_FILENO, TCSANOW, console.termios) == -1)
			warn("tcsetattr()");
		free(console.termios);
		console.termios = NULL;
	}

	if (console.vt_mode != NULL) {
		if (ioctl(STDIN_FILENO, VT_SETMODE, console.vt_mode) == -1)
			warn("ioctl(VT_SETMODE)");
		free(console.vt_mode);
		console.vt_mode = NULL;
	}

#if defined (__linux__) || defined (__NetBSD__) || defined (__OpenBSD__)
	if (console.mode != -1) {
		console_setMode(console.mode);
		console.mode = -1;
	}
#elif defined (__FreeBSD__)
	if (console.mode != -1) {
		if (console.mode == KD_TEXT && console.SC_PIXEL_MODE)
			console_setMode(KD_PIXEL);
		else
			console_setMode(console.mode);
		console.mode = -1;
	}
#else
	#error not implement
#endif
#if defined (__linux__)
	puts("\033[?25h");
#endif
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

int console_getActive(void)
{
#if defined (__linux__)
	struct vt_stat vt_stat;
	int result;

	assert(initialized);

	result = 0;
	if (ioctl(STDIN_FILENO, VT_GETSTATE, &vt_stat) != -1)
		result = vt_stat.v_active;
	return result;
#elif defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__)
	int result, active;

	assert(initialized);

	result = 0;
	if (ioctl(STDIN_FILENO, VT_GETACTIVE, &active) != -1)
		result = active;
	return result;
#else
	#error not implement
#endif
}

bool console_isActive(void)
{
	int active;

	assert(initialized);

	active = console_getActive();
	if (active < 1)
		errx(1, "Could not get active console.");
	return console.active == active;
}

int console_getMode(void)
{
	int mode;

	assert(initialized);

#if defined (__linux__) || defined (__FreeBSD__)
	if (ioctl(STDIN_FILENO, KDGETMODE, &mode) == -1)
		errx(1, "Could not get console mode.");
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	if (ioctl(STDIN_FILENO, WSDISPLAYIO_GMODE, &mode) == -1)
		errx(1, "Could not get console mode.");
#else
	#error not implement
#endif
	return mode;
}

void console_setMode(int mode)
{
#if defined (__linux__) || defined (__FreeBSD__)
	assert(initialized);

	if (ioctl(STDIN_FILENO, KDSETMODE, mode) == -1)
		errx(1, "Could not set console mode.");
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	int value;

	assert(initialized);

	value = mode;
	if (ioctl(STDIN_FILENO, WSDISPLAYIO_SMODE, &value) == -1)
		errx(1, "Could not set console mode.");
#else
	#error not implement
#endif
}

#if defined (__FreeBSD__)
void console_setSC_PIXEL_MODE(bool SC_PIXEL_MODE)
{
	console.SC_PIXEL_MODE = SC_PIXEL_MODE;
}
#endif

void console_setAcquireHandler(void (*acquireHandler)(int signum))
{
	assert(initialized);

	console.acquireHandler = acquireHandler;
}

void console_setReleaseHandler(void (*releaseHandler)(int signum))
{
	assert(initialized);

	console.releaseHandler = releaseHandler;
}

static void acquire(int signum)
{
	int errsv;

	assert(initialized);

	errsv = errno;
	ioctl(STDIN_FILENO, VT_RELDISP, VT_ACKACQ);
	ioctl(STDIN_FILENO, VT_WAITACTIVE, console.active);
	if (console.acquireHandler != NULL)
		console.acquireHandler(signum);
	errno = errsv;
}

static void release(int signum)
{
	int errsv;

	assert(initialized);

	errsv = errno;
	if (console.releaseHandler != NULL)
		console.releaseHandler(signum);
	ioctl(STDIN_FILENO, VT_RELDISP, 1); /* 1 == VT_TRUE */
	errno = errsv;
}

