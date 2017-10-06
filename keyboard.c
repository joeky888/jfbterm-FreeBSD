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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#if defined (__linux__)
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#elif defined (__FreeBSD__)
#include <osreldate.h>
#if __FreeBSD_version >= 410000
#include <sys/kbio.h>
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

#include "getcap.h"
#include "keyboard.h"
#include "privilege.h"

#define KEYBOARD_DELAY_MIN  (250)  /* 250ms */
#define KEYBOARD_DELAY_MAX  (1000) /* 1000ms */

#define KEYBOARD_REPEAT_MIN (30)   /* 30ms (30hz) */
#define KEYBOARD_REPEAT_MAX (500)  /* 500ms (2hz) */

static struct {
	int delay;
	int repeat;
} keyboard = {
	0,
	0
};

static bool repeatDataSaved;
static bool initialized;

static void finalize(void);
static void finalizer(void);
static void configDelay(const char *config);
static void configRepeat(const char *config);
static void setKeyRepeat(const int delay, const int repeat);

#if defined (__linux__)
static struct kbd_repeat old_kbd_repeat;
#elif defined (__FreeBSD__)
static keyboard_repeat_t old_keyboard_repeat;
#elif defined (__NetBSD__) || defined (__OpenBSD__)
static struct wskbd_keyrepeat_data old_wskbd_keyrepeat_data;
#endif

void keyboard_initialize(void)
{
	assert(!initialized);

	atexit(finalizer);
	initialized = true;
#if defined (__linux__)
	old_kbd_repeat.delay  = -1;
	old_kbd_repeat.period = -1;
	privilege_on();
	if (ioctl(STDIN_FILENO, KDKBDREP, &old_kbd_repeat) != -1) {
		keyboard.delay  = old_kbd_repeat.delay;
		keyboard.repeat = old_kbd_repeat.period;
		repeatDataSaved = true;
	} else
		warnx("Could not get keyrepeat data.");
	privilege_off();
#elif defined (__FreeBSD__)
	if (ioctl(STDIN_FILENO, KDGETREPEAT, &old_keyboard_repeat) != -1) {
		keyboard.delay  = old_keyboard_repeat.kb_repeat[0];
		keyboard.repeat = old_keyboard_repeat.kb_repeat[1];
		repeatDataSaved = true;
	} else
		warnx("Could not get keyrepeat data.");
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	old_wskbd_keyrepeat_data.which = WSKBD_KEYREPEAT_DOALL;
	if (ioctl(STDIN_FILENO, WSKBDIO_GETKEYREPEAT,
		  &old_wskbd_keyrepeat_data) != -1) {
		keyboard.delay  = old_wskbd_keyrepeat_data.del1;
		keyboard.repeat = old_wskbd_keyrepeat_data.delN;
		repeatDataSaved = true;
	} else
		warnx("Could not get keyrepeat data.");
#else
	#error not implement
#endif
}

static void finalize(void)
{
#if defined (__linux__)
	struct kbentry kbentry;
#endif

	assert(initialized);

#if defined (__linux__)
	privilege_on();
	if (repeatDataSaved) {
		if (ioctl(STDIN_FILENO, KDKBDREP, &old_kbd_repeat) == -1)
			warn("ioctl(KDKBDREP)");
	}
	kbentry.kb_table = (1 << KG_SHIFT);
	kbentry.kb_index = KEY_PAGEUP;
	kbentry.kb_value = K_SCROLLBACK;
	if (ioctl(STDIN_FILENO, KDSKBENT, &kbentry) == -1)
		warn("ioctl(KDSKBENT)");
	kbentry.kb_table = (1 << KG_SHIFT);
	kbentry.kb_index = KEY_PAGEDOWN;
	kbentry.kb_value = K_SCROLLFORW;
	if (ioctl(STDIN_FILENO, KDSKBENT, &kbentry) == -1)
		warn("ioctl(KDSKBENT)");
	privilege_off();
#elif defined (__FreeBSD__)
	if (repeatDataSaved) {
		if (ioctl(STDIN_FILENO, KDSETREPEAT,
			  &old_keyboard_repeat) == -1)
			warn("ioctl(KDSETREPEAT)");
	}
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	if (repeatDataSaved) {
		if (ioctl(STDIN_FILENO, WSKBDIO_SETKEYREPEAT,
			  &old_wskbd_keyrepeat_data) == -1)
			warn("ioctl(WSKBDIO_SETKEYREPEAT)");
	}
#else
	#error not implement
#endif
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

static void configDelay(const char *config)
{
	int delay;

	assert(initialized);

	if (config != NULL) {
		delay = atoi(config);
		if (delay >= KEYBOARD_DELAY_MIN &&
		    delay <= KEYBOARD_DELAY_MAX)
			keyboard.delay = delay;
		else
			warnx("Invalid keyboard delay: %s", config);
	}
}

static void configRepeat(const char *config)
{
	int repeat;

	assert(initialized);

	if (config != NULL) {
		repeat = atoi(config);
		if (repeat >= KEYBOARD_REPEAT_MIN &&
		    repeat <= KEYBOARD_REPEAT_MAX)
			keyboard.repeat = repeat;
		else
			warnx("Invalid keyboard repeat: %s", config);
	}
}

void keyboard_configure(TCaps *caps)
{
	const char *config;

	assert(initialized);
	assert(caps != NULL);

	config = caps_findFirst(caps, "keyboard.delay");
	configDelay(config);
	config = caps_findFirst(caps, "keyboard.repeat");
	configRepeat(config);
	setKeyRepeat(keyboard.delay, keyboard.repeat);
}

#if defined (__linux__)
void keyboard_enableScrollBack(bool enable)
{
	struct kbentry kbentry;

	privilege_on();
	kbentry.kb_table = (1 << KG_SHIFT);
	kbentry.kb_index = KEY_PAGEUP;
	kbentry.kb_value = enable ? K_SCROLLBACK : K_PGUP;
	if (ioctl(STDIN_FILENO, KDSKBENT, &kbentry) == -1)
		warn("ioctl(KDSKBENT)");
	kbentry.kb_table = (1 << KG_SHIFT);
	kbentry.kb_index = KEY_PAGEDOWN;
	kbentry.kb_value = enable ? K_SCROLLFORW : K_PGDN;
	if (ioctl(STDIN_FILENO, KDSKBENT, &kbentry) == -1)
		warn("ioctl(KDSKBENT)");
	privilege_off();
}
#endif

static void setKeyRepeat(const int delay, const int repeat)
{
#if defined (__linux__)
	struct kbd_repeat kbd_repeat;

	assert(initialized);

	if (!repeatDataSaved)
		return;
	kbd_repeat.delay  = delay;
	kbd_repeat.period = repeat;
	privilege_on();
	if (ioctl(STDIN_FILENO, KDKBDREP, &kbd_repeat) == -1)
		warn("ioctl(KDKBDREP)");
	privilege_off();
#elif defined (__FreeBSD__)
	keyboard_repeat_t keyboard_repeat;

	assert(initialized);

	if (!repeatDataSaved)
		return;
	keyboard_repeat.kb_repeat[0] = delay;
	keyboard_repeat.kb_repeat[1] = repeat;
	if (ioctl(STDIN_FILENO, KDSETREPEAT, &keyboard_repeat) == -1)
		warn("ioctl(KDSETREPEAT)");
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	struct wskbd_keyrepeat_data wskbd_keyrepeat_data;

	assert(initialized);

	if (!repeatDataSaved)
		return;
	wskbd_keyrepeat_data.which = WSKBD_KEYREPEAT_DOALL;
	wskbd_keyrepeat_data.del1  = delay;
	wskbd_keyrepeat_data.delN  = repeat;
	if (ioctl(STDIN_FILENO, WSKBDIO_SETKEYREPEAT,
		  &wskbd_keyrepeat_data) == -1)
		warn("ioctl(WSKBDIO_SETKEYREPEAT)");
#else
	#error not implement
#endif
}

bool keyboard_isScrollLocked(void)
{
#if defined (__linux__)
	bool result;
	int kbled;

	assert(initialized);

	result = false;
	if (ioctl(STDIN_FILENO, KDGKBLED, &kbled) != -1)
		result = (kbled & K_SCROLLLOCK);
	return result;
#elif defined (__FreeBSD__)
	bool result;
	int kbstate;

	assert(initialized);

	result = false;
	if (ioctl(STDIN_FILENO, KDGKBSTATE, &kbstate) != -1)
		result = (kbstate & SLKED);
	return result;
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	bool result;
	int led;

	assert(initialized);

	result = false;
	if (ioctl(STDIN_FILENO, WSKBDIO_GETLEDS, &led) != -1)
		result = (led & WSKBD_LED_SCROLL);
	return result;
#else
	#error not implement
#endif
}

bool keyboard_isShiftPressed(void)
{
#if defined (__linux__)
	char shift_state;

	shift_state = 6;
	if (ioctl(STDIN_FILENO, TIOCLINUX, &shift_state) == -1)
		return false;
	return (shift_state & (1 << KG_SHIFT));
#else
	return false;
#endif
}

bool keyboard_isAltPressed(void)
{
#if defined (__linux__)
	char shift_state;

	shift_state = 6;
	if (ioctl(STDIN_FILENO, TIOCLINUX, &shift_state) == -1)
		return false;
	return (shift_state & (1 << KG_ALT));
#else
	return false;
#endif
}

bool keyboard_isCtrlPressed(void)
{
#if defined (__linux__)
	char shift_state;

	shift_state = 6;
	if (ioctl(STDIN_FILENO, TIOCLINUX, &shift_state) == -1)
		return false;
	return (shift_state & (1 << KG_CTRL));
#else
	return false;
#endif
}

