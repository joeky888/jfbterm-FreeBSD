/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
 *
 * KON2 - Kanji ON Console -
 * Copyright (C) 1992,1993 Atusi MAEDA <mad@math.keio.ac.jp>
 * Copyright (C) 1992-1996 Takashi MANABE <manabe@papilio.tutics.tut.ac.jp>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#if defined (__FreeBSD__)
#include <sys/mouse.h>
#elif defined (__NetBSD__) || defined (__OpenBSD__)
#include <time.h>
#include <dev/wscons/wsconsio.h>
#endif

#include "clipboard.h"
#include "font.h"
#include "keyboard.h"
#include "mouse.h"
#include "privilege.h"
#include "utilities.h"
#include "term.h"
#include "vterm.h"
#include "vtermlow.h"

#define MAX_PACKET_SIZE     (8)
#define BUF_SIZE	    (MAX_PACKET_SIZE * 100)

#define MOUSE_LIFETIME      (55) /* 5.5 sec. */

#define MOUSE_BUTTON_NONE   (0x00)
#define MOUSE_BUTTON_MB1    (0x01)
#define MOUSE_BUTTON_MB2    (0x02)
#define MOUSE_BUTTON_MB3    (0x04)
#define MOUSE_BUTTON_MB4    (0x08)
#define MOUSE_BUTTON_MB5    (0x10)

static struct {
	int x, y;
	int ox, oy;
	int sx, sy;
	int dx, dy;
	char button;
	char obutton;
} mouseInfo;

typedef enum {
	MOUSE_TYPE_MICROSOFT,
	MOUSE_TYPE_MOUSESYSTEMS,
	MOUSE_TYPE_BUSMOUSE,
	MOUSE_TYPE_MMSERIES,
	MOUSE_TYPE_LOGITECH,
	MOUSE_TYPE_PS2,
	MOUSE_TYPE_IMPS2,
	MOUSE_TYPE_EXPS2,
#if defined (__FreeBSD__)
	MOUSE_TYPE_SYSMOUSE,
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	MOUSE_TYPE_WSMOUSE,
#endif
	MOUSE_TYPE_NONE
} MOUSE_TYPE;

static struct {
	int fd;
	MOUSE_TYPE type;
	int cFlag;
	u_char headMask;
	u_char headId;
	u_char dataMask;
	int packetSize;
	int baudRate;
	char *device;
	int pasteButton;
	struct timeval timer;
	bool enable;
} mouse = {
	-1,
	MOUSE_TYPE_NONE,
	0,
	0,
	0,
	0,
	0,
	B1200,
	NULL,
	MOUSE_BUTTON_MB2,
	{ 0, 0 },
	false
};

static bool initialized;

static void finalize(void);
static void finalizer(void);
static void configBaudRate(const char *config);
static void configDevice(const char *config);
static void configPasteButton(const char *config);
static void configType(const char *config);
static void setBaudRate(const int baudRate, const int cFlag);
static bool setupIntelliMouse(void);
static void resetTimer(void);
static void analyzePacket(TVterm *p, const u_char *packet);
#if defined (__NetBSD__) || defined (__OpenBSD__)
static void analyzeWSConsEvents(TVterm *p);
#endif
static void execute(TVterm *p, int *dx, int *dy);
static void getSelectionRegion(int *sx, int *sy, int *ex, int *ey);
static bool isMouseButtonDown(char button);
static bool isMouseButtonMove(char button);
static bool isMouseButtonUp(char button);
static void selection(TVterm *p);
static void mouseTrackingX10(TVterm *p);
static void mouseTrackingVT200(TVterm *p);
static void mouseTrackingButtonEvents(TVterm *p);
static void mouseTrackingAnyEvents(TVterm *p);

void mouse_initialize(void)
{
	assert(!initialized);

	atexit(finalizer);
	initialized = true;
	clipboard_initialize();
}

static void finalize(void)
{
	assert(initialized);

	mouse_stop();
	if (mouse.device != NULL) {
		free(mouse.device);
		mouse.device = NULL;
	}
	mouse.enable = false;
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

static void configBaudRate(const char *config)
{
	assert(initialized);

	if (config != NULL)
		mouse.baudRate = atoi(config);
	switch (mouse.baudRate) {
	case 9600:
		mouse.baudRate = B9600;
		break;
	case 4800:
		mouse.baudRate = B4800;
		break;
	case 2400:
		mouse.baudRate = B2400;
		break;
	default:
		warnx("Invalid mouse baud rate %s", config);
		/* FALLTHROUGH */
	case 1200:
		mouse.baudRate = B1200;
		break;
	}
}

static void configDevice(const char *config)
{
	assert(initialized);

	if (config != NULL) {
		if (mouse.device)
			free(mouse.device);
		mouse.device = strdup(config);
		if (mouse.device == NULL)
			err(1, "strdup()");
	}
}

static void configPasteButton(const char *config)
{
	bool found;
	int i;

	static const struct {
		const char *key;
		const int pasteButton;
	} list[] = {
		{ "Middle", MOUSE_BUTTON_MB2 },
		{ "Right",  MOUSE_BUTTON_MB3 },
		{ NULL,     0                }
	};

	assert(initialized);

	mouse.pasteButton = MOUSE_BUTTON_MB2;
	if (config != NULL) {
		found = false;
		for (i = 0; list[i].key != NULL; i++) {
			if (strcasecmp(list[i].key, config) == 0) {
				mouse.pasteButton = list[i].pasteButton;
				found = true;
				break;
			}
		}
		if (!found)
			warnx("Invalid mouse paste button: %s", config);
	}
}

static void configType(const char *config)
{
	bool found;
	int i;

	static const struct {
		const char *name;
		const MOUSE_TYPE type;
		const int cFlag;
		const u_char headMask;
		const u_char headId;
		const u_char dataMask;
		const int packetSize;
		const bool enable;
	} list[] = {
		{
			"Microsoft", MOUSE_TYPE_MICROSOFT,
			(CS7 | CREAD | CLOCAL | HUPCL),
			0x40, 0x40, 0x40, 3, true
		},
		{
			"MouseSystems", MOUSE_TYPE_MOUSESYSTEMS,
			(CS8 | CSTOPB | CREAD | CLOCAL | HUPCL),
			0xf8, 0x80, 0x00, 5, true
		},
		{
			"BusMouse", MOUSE_TYPE_BUSMOUSE,
			0x00,
			0xf8, 0x80, 0x00, 5, true
		},
		{
			"MmSeries", MOUSE_TYPE_MMSERIES,
			(CS8 | PARENB | PARODD | CREAD | CLOCAL | HUPCL),
			0xe0, 0x80, 0x80, 3, true
		},
		{
			"Logitech", MOUSE_TYPE_LOGITECH,
			(CS8 | CSTOPB | CREAD | CLOCAL | HUPCL),
			0xe0, 0x80, 0x80, 3, true
		},
		{
			"PS2", MOUSE_TYPE_PS2,
			(CS8 | CREAD | CLOCAL | HUPCL),
			0xc8, 0x08, 0x00, 3, true
		},
		{
			"IMPS2", MOUSE_TYPE_IMPS2,
			(CS8 | CREAD | CLOCAL | HUPCL),
			0xc8, 0x08, 0x00, 4, true
		},
		{
			"EXPS2", MOUSE_TYPE_EXPS2,
			(CS8 | CREAD | CLOCAL | HUPCL),
			0xc8, 0x08, 0x00, 4, true
		},
#if defined (__FreeBSD__)
		{
			"SysMouse", MOUSE_TYPE_SYSMOUSE,
			(CS8 | CSTOPB | CREAD | CLOCAL | HUPCL),
			0xf8, 0x80, 0x00, 5, true
		},
#elif defined (__NetBSD__) || defined (__OpenBSD__)
		{
			"WSMouse", MOUSE_TYPE_WSMOUSE,
			0x00,
			0x00, 0x00, 0x00, 0x00, true
		},
#endif
		{
			"None", MOUSE_TYPE_NONE,
			0x00,
			0x00, 0x00, 0x00, 0x00, false
		},
		{
			NULL, MOUSE_TYPE_NONE,
			0x00,
			0x00, 0x00, 0x00, 0x00, false
		}
	};

	assert(initialized);

	mouse.type = MOUSE_TYPE_NONE;
	mouse.enable = false;
	if (config != NULL) {
		found = false;
		for (i = 0; list[i].name != NULL; i++) {
			if (strcasecmp(list[i].name, config) == 0) {
				mouse.type       = list[i].type;
				mouse.cFlag      = list[i].cFlag;
				mouse.headMask   = list[i].headMask;
				mouse.headId     = list[i].headId;
				mouse.dataMask   = list[i].dataMask;
				mouse.packetSize = list[i].packetSize;
				mouse.enable     = list[i].enable;
				found = true;
				break;
			}
		}
		if (!found)
			warnx("Invalid mouse type: %s", config);
	}
}

void mouse_configure(TCaps *caps)
{
	const char *config;

	assert(initialized);
	assert(caps != NULL);

	config = caps_findFirst(caps, "mouse.type");
	configType(config);
	if (mouse.type != MOUSE_TYPE_NONE) {
		if (mouse.type != MOUSE_TYPE_BUSMOUSE) {
			config = caps_findFirst(caps, "mouse.baudRate");
			configBaudRate(config);
		}
		config = caps_findFirst(caps, "mouse.device");
		configDevice(config);
		config = caps_findFirst(caps, "mouse.pasteButton");
		configPasteButton(config);
	}
}

static void setBaudRate(const int baudRate, const int cFlag)
{
	struct termios mio;
	const char *cf;

	assert(initialized);

	if (mouse.fd == -1)
		return;
	tcgetattr(mouse.fd, &mio);
	mio.c_iflag = IGNBRK | IGNPAR;
	mio.c_oflag = 0;
	mio.c_lflag = 0;
#if defined (__linux__)
	mio.c_line = 0;
#endif
	mio.c_cc[VTIME] = 0;
	mio.c_cc[VMIN] = 1;
	mio.c_cflag = cFlag;
	cfsetispeed(&mio, baudRate);
	cfsetospeed(&mio, baudRate);
	tcsetattr(mouse.fd, TCSAFLUSH, &mio);
	switch (mouse.baudRate) {
	case B9600: cf = "*q"; break;
	case B4800: cf = "*p"; break;
	case B2400: cf = "*o"; break;
	default:
		/* FALLTHROUGH */
	case B1200: cf = "*n"; break;
	}
	mio.c_cflag = cFlag;
	cfsetispeed(&mio, mouse.baudRate);
	cfsetospeed(&mio, mouse.baudRate);
	write(mouse.fd, cf, 2);
	usleep(100000);
	tcsetattr(mouse.fd, TCSAFLUSH, &mio);
}

int mouse_getFd(void)
{
	assert(initialized);

	return mouse.fd;
}

bool mouse_isEnable(void)
{
	assert(initialized);

	return mouse.enable;
}

static void resetTimer(void)
{
	assert(initialized);

	gettimeofday(&mouse.timer, NULL);
}

bool mouse_isTimeout(void)
{
	struct timeval now, diff;
	u_int elapsedTime;

	assert(initialized);

	timerclear(&diff);
	gettimeofday(&now, NULL);
	timersub(&now, &mouse.timer, &diff);
	elapsedTime = diff.tv_sec * 10 + diff.tv_usec / 100000; /* 0.1 sec. */
	return elapsedTime >= MOUSE_LIFETIME;
}

int mouse_getX(void)
{
	assert(initialized);

	return mouseInfo.x;
}

int mouse_getY(void)
{
	assert(initialized);

	return mouseInfo.y;
}

static bool setupIntelliMouse(void)
{
	int i;
	const u_char setSampleRates[3][2] = {
		{ '\xf3', 200 },
		{ '\xf3', 100 },
		{ '\xf3',  80 }
	};
	const u_char getDeviceId = '\xf2';
	char deviceId, acknowledge;
	size_t nbytes;

	assert(initialized);

	/* Set sample rate. */
	for (i = 0; i < 3; i++) {
		write(mouse.fd, setSampleRates[i], 2);
		nbytes = read(mouse.fd, &acknowledge, 1);
		if (nbytes != 1 || acknowledge != '\xfa')
			return false;
	}
	/* Get device id. */
	write(mouse.fd, &getDeviceId, 1);
	nbytes = read(mouse.fd, &acknowledge, 1);
	if (nbytes != 1 || acknowledge != '\xfa')
		return false;
	nbytes = read(mouse.fd, &deviceId, 1);
	if (nbytes != 1)
		return false;
	switch (deviceId) {
	case '\x03':
		mouse.type = MOUSE_TYPE_IMPS2;
		break;
	case '\x04':
		mouse.type = MOUSE_TYPE_EXPS2;
		break;
	default:
		mouse.type = MOUSE_TYPE_PS2;
		break;
	}
	return true;
}

void mouse_start(void)
{
	struct stat st;

	assert(initialized);

	mouse_stop();
	if (!mouse.enable)
		return;
	if (mouse.device == NULL) {
		mouse.type = MOUSE_TYPE_NONE;
		mouse.enable = false;
		return;
	}
	privilege_on();
	mouse.fd = open(mouse.device, O_RDWR | O_NONBLOCK);
	privilege_off();
	if (mouse.fd == -1) {
		warnx("Could not open mouse device: %s", mouse.device);
		mouse.type = MOUSE_TYPE_NONE;
		mouse.enable = false;
		return;
	}
	if (fstat(mouse.fd, &st) == -1)
		err(1, "fstat(%s)", mouse.device);
	if (!S_ISCHR(st.st_mode) && !S_ISFIFO(st.st_mode)) {
		warnx("%s is not a character device or FIFO.", mouse.device);
		mouse.type = MOUSE_TYPE_NONE;
		mouse.enable = false;
		return;
	}
	if (fcntl(mouse.fd, F_SETFD, FD_CLOEXEC) == -1)
		err(1, "fcntl(F_SETFD)");
	if (mouse.type == MOUSE_TYPE_IMPS2 || mouse.type == MOUSE_TYPE_EXPS2) {
		if (!setupIntelliMouse()) {
			mouse.type = MOUSE_TYPE_NONE;
			mouse.enable = false;
			return;
		}
	}
#if defined (__FreeBSD__)
	if (mouse.type == MOUSE_TYPE_SYSMOUSE) {
		mousemode_t mousemode;
		int level;

		level = 1;
		if (ioctl(mouse.fd, MOUSE_SETLEVEL, &level) == -1)
			err(1, "ioctl(MOUSE_SETLEVEL)");
		if (ioctl(mouse.fd, MOUSE_GETMODE, &mousemode) == -1)
			err(1, "ioctl(MOUSE_GETMODE)");
		mouse.headMask   = mousemode.syncmask[0];
		mouse.headId     = mousemode.syncmask[1];
		mouse.packetSize = mousemode.packetsize;
	}
#endif
	if (mouse.type != MOUSE_TYPE_BUSMOUSE) {
		setBaudRate(B9600, mouse.cFlag);
		setBaudRate(B4800, mouse.cFlag);
		setBaudRate(B2400, mouse.cFlag);
		setBaudRate(B1200, mouse.cFlag);
		if (mouse.type == MOUSE_TYPE_LOGITECH) {
			write(mouse.fd, "S", 1);
			setBaudRate(mouse.baudRate,
				    CS8 | PARENB | PARODD | CREAD |
				    CLOCAL | HUPCL);
		}
		write(mouse.fd, "Q", 1);
	}
}

void mouse_stop(void)
{
	assert(initialized);

	if (mouse.fd != -1) {
		close(mouse.fd);
		mouse.fd = -1;
	}
}

static void analyzePacket(TVterm *p, const u_char *packet)
{
	static int dx, dy;
	int dz;

	assert(initialized);
	assert(p != NULL);
	assert(packet != NULL);

	dz = 0;
	mouseInfo.button = MOUSE_BUTTON_NONE;
	switch (mouse.type) {
	case MOUSE_TYPE_MICROSOFT:
		mouseInfo.button |= ((packet[0] & 0x20) >> 5);
		mouseInfo.button |= ((packet[0] & 0x10) >> 2);
		dx += (char)(((packet[0] & 0x03) << 6) | (packet[1] & 0x3f));
		dy += (char)(((packet[0] & 0x0c) << 4) | (packet[2] & 0x3f));
		break;
#if defined (__FreeBSD__)
	case MOUSE_TYPE_SYSMOUSE:
		/* FALLTHROUGH */
#endif
	case MOUSE_TYPE_MOUSESYSTEMS:
		mouseInfo.button |= ((~packet[0] & 0x04) >> 2);
		mouseInfo.button |= ((~packet[0] & 0x02));
		mouseInfo.button |= ((~packet[0] & 0x01) << 2);
		dx +=  ((char)(packet[1]) + (char)(packet[3]));
		dy += -((char)(packet[2]) + (char)(packet[4]));
		if (mouse.packetSize >= 8) {
			dz = ((char)(packet[5] << 1) +
			      (char)(packet[6] << 1)) >> 1;
#if 0
			mouseInfo.button |= ((~packet[7] & 0x03) << 3);
#endif
		}
		break;
	case MOUSE_TYPE_BUSMOUSE:
		mouseInfo.button |= ((~packet[0] & 0x04) >> 2);
		mouseInfo.button |= ((~packet[0] & 0x02));
		mouseInfo.button |= ((~packet[0] & 0x01) << 2);
		dx +=  (char)packet[1];
		dy += -(char)packet[2];
		break;
	case MOUSE_TYPE_MMSERIES:
		/* FALLTHROUGH */
	case MOUSE_TYPE_LOGITECH:
		mouseInfo.button |= ((packet[0] & 0x04) >> 2);
		mouseInfo.button |= ((packet[0] & 0x02));
		mouseInfo.button |= ((packet[0] & 0x01) << 2);
		dx += (packet[0] & 0x10) ? packet[1] : -packet[1];
		dy += (packet[0] & 0x08) ? -packet[2] : packet[2];
		break;
	case MOUSE_TYPE_PS2:
		mouseInfo.button |= ((packet[0] & 0x01));
		mouseInfo.button |= ((packet[0] & 0x04) >> 1);
		mouseInfo.button |= ((packet[0] & 0x02) << 1);
		dx += (char)(packet[1]);
		dy -= (char)(packet[2]);
		break;
	case MOUSE_TYPE_IMPS2:
		mouseInfo.button |= ((packet[0] & 0x01));
		mouseInfo.button |= ((packet[0] & 0x04) >> 1);
		mouseInfo.button |= ((packet[0] & 0x02) << 1);
		dx += (char)(packet[1]);
		dy -= (char)(packet[2]);
		dz  = (char)(packet[3]);
		break;
	case MOUSE_TYPE_EXPS2:
		mouseInfo.button |= ((packet[0] & 0x01));
		mouseInfo.button |= ((packet[0] & 0x04) >> 1);
		mouseInfo.button |= ((packet[0] & 0x02) << 1);
		dx += (char)(packet[1]);
		dy -= (char)(packet[2]);
#if 0
		mouseInfo.button |= ((packet[3] & 0x10) >> 1);
		mouseInfo.button |= ((packet[3] & 0x20) >> 1);
#endif
		dz  = (char)((packet[3] & 0x0f) << 4) >> 4;
		break;
	case MOUSE_TYPE_NONE:
		/* FALLTHROUGH */
	default:
		return;
	}
	if (dz < 0) {
		mouseInfo.obutton &= ~MOUSE_BUTTON_MB4;
		mouseInfo.button  |=  MOUSE_BUTTON_MB4;
	}
	if (dz > 0) {
		mouseInfo.obutton &= ~MOUSE_BUTTON_MB5;
		mouseInfo.button  |=  MOUSE_BUTTON_MB5;
	}
	execute(p, &dx, &dy);
}

#if defined (__NetBSD__) || defined (__OpenBSD__)
static void analyzeWSConsEvents(TVterm *p)
{
	static int dx, dy;
	int dz;
	struct wscons_event wscons_event;
	ssize_t nbytes;

	assert(initialized);
	assert(p != NULL);

	for (;;) {
		nbytes = read(mouse.fd, &wscons_event,
			      sizeof(struct wscons_event));
		if (nbytes != sizeof(struct wscons_event))
			break;
		dz = 0;
		switch (wscons_event.type) {
		case WSCONS_EVENT_MOUSE_UP:
			switch (wscons_event.value) {
			case 0:
				mouseInfo.button &= ~MOUSE_BUTTON_MB1;
				break;
			case 1:
				mouseInfo.button &= ~MOUSE_BUTTON_MB2;
				break;
			case 2:
				mouseInfo.button &= ~MOUSE_BUTTON_MB3;
				break;
			}
			break;
		case WSCONS_EVENT_MOUSE_DOWN:
			switch (wscons_event.value) {
			case 0:
				mouseInfo.button |= MOUSE_BUTTON_MB1;
				break;
			case 1:
				mouseInfo.button |= MOUSE_BUTTON_MB2;
				break;
			case 2:
				mouseInfo.button |= MOUSE_BUTTON_MB3;
				break;
			}
			break;
		case WSCONS_EVENT_MOUSE_DELTA_X:
			dx += wscons_event.value;
			break;
		case WSCONS_EVENT_MOUSE_DELTA_Y:
			dy -= wscons_event.value;
			break;
		case WSCONS_EVENT_MOUSE_DELTA_Z:
			dz  = wscons_event.value;
			break;
		default:
			continue;
		}
		if (dz < 0) {
			mouseInfo.obutton &= ~MOUSE_BUTTON_MB4;
			mouseInfo.button  |=  MOUSE_BUTTON_MB4;
		}
		if (dz > 0) {
			mouseInfo.obutton &= ~MOUSE_BUTTON_MB5;
			mouseInfo.button  |=  MOUSE_BUTTON_MB5;
		}
		execute(p, &dx, &dy);
	}
}
#endif

static void execute(TVterm *p, int *dx, int *dy)
{
	assert(initialized);
	assert(p != NULL);
	assert(dx != NULL);
	assert(dy != NULL);

	if (!mouse.enable)
		return;
	resetTimer();
	mouseInfo.dx = *dx / (int)gFontsWidth;
	*dx -= mouseInfo.dx * (int)gFontsWidth;
	mouseInfo.dy = *dy / (int)gFontsHeight;
	*dy -= mouseInfo.dy * (int)gFontsHeight;
	if (mouseInfo.dx != 0 || mouseInfo.dy != 0) {
		mouseInfo.x += mouseInfo.dx;
		mouseInfo.y += mouseInfo.dy;
		if (mouseInfo.x < 0)
			mouseInfo.x = 0;
		else if (mouseInfo.x > p->xmax - 1)
			mouseInfo.x = p->xmax - 1;
		if (mouseInfo.y < 0)
			mouseInfo.y = 0;
		else if (mouseInfo.y > p->ymax - 1)
			mouseInfo.y = p->ymax - 1;
	}
	switch (p->mouseTracking) {
	case VTERM_MOUSE_TRACKING_NONE:
		selection(p);
		break;
	case VTERM_MOUSE_TRACKING_X10:
		mouseTrackingX10(p);
		break;
	case VTERM_MOUSE_TRACKING_VT200:
		mouseTrackingVT200(p);
		break;
	case VTERM_MOUSE_TRACKING_BTN_EVENT:
		mouseTrackingButtonEvents(p);
		break;
	case VTERM_MOUSE_TRACKING_ANY_EVENT:
		mouseTrackingAnyEvents(p);
		break;
	default:
		break;
	}
	mouseInfo.ox = mouseInfo.x;
	mouseInfo.oy = mouseInfo.y;
	mouseInfo.obutton = mouseInfo.button;
}

static void getSelectionRegion(int *sx, int *sy, int *ex, int *ey)
{
	int tmp;

	*sx = mouseInfo.sx;
	*sy = mouseInfo.sy;
	*ex = mouseInfo.x;
	*ey = mouseInfo.y;
	if (*sy > *ey) {
		tmp = *sx; *sx = *ex; *ex = tmp;
		tmp = *sy; *sy = *ey; *ey = tmp;
	} else if (*sy == *ey && *sx > *ex) {
		tmp = *sx; *sx = *ex; *ex = tmp;
	}
}

static bool isMouseButtonDown(char button)
{
	return !(mouseInfo.obutton & button) && (mouseInfo.button & button);
}

static bool isMouseButtonMove(char button)
{
	return (mouseInfo.obutton & button) && (mouseInfo.button & button) &&
	       (mouseInfo.ox != mouseInfo.x || mouseInfo.oy != mouseInfo.y);
}

static bool isMouseButtonUp(char button)
{
	return (mouseInfo.obutton & button) && !(mouseInfo.button & button);
}

static void selection(TVterm *p)
{
	char buf[4];
	int sx, sy, ex, ey;

	assert(initialized);
	assert(p != NULL);

	if (isMouseButtonDown(MOUSE_BUTTON_MB1)) {
		mouseInfo.sx = mouseInfo.x;
		mouseInfo.sy = mouseInfo.y;
	} else if (isMouseButtonDown(mouse.pasteButton)) {
		if (!keyboard_isScrollLocked())
			vterm_pasteText(p);
	} else if (isMouseButtonDown(MOUSE_BUTTON_MB4)) {
		if (p->cursor) {
			strncpy(buf, "\033OA", sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
			write_wrapper(p->term->masterPty, buf, strlen(buf));
		} else
			vterm_scroll_backward_line(p);
	} else if (isMouseButtonDown(MOUSE_BUTTON_MB5)) {
		if (p->cursor) {
			strncpy(buf, "\033OB", sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
			write_wrapper(p->term->masterPty, buf, strlen(buf));
		} else
			vterm_scroll_forward_line(p);
	} else if (isMouseButtonUp(MOUSE_BUTTON_MB1)) {
		getSelectionRegion(&sx, &sy, &ex, &ey);
		vterm_copyText(p, sx, sy, ex, ey);
	} else if (isMouseButtonMove(MOUSE_BUTTON_MB1)) {
		getSelectionRegion(&sx, &sy, &ex, &ey);
		vterm_reverseText(p, sx, sy, ex, ey);
		vterm_refresh(p);
		vterm_reverseText(p, sx, sy, ex, ey);
	}
}

static void mouseTrackingX10(TVterm *p)
{
	char buf[7];
	int b, x, y;

	if (isMouseButtonDown(MOUSE_BUTTON_MB1))
		b = 0;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB2))
		b = 1;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB3))
		b = 2;
	else
		return;
	if (keyboard_isShiftPressed())
		b += 4;
	if (keyboard_isAltPressed())
		b += 8;
	if (keyboard_isCtrlPressed())
		b += 16;
	x = mouseInfo.x + 1;
	if (x > 255 - 32) x = 255 - 32;
	y = mouseInfo.y + 1;
	if (y > 255 - 32) y = 255 - 32;
	snprintf(buf, sizeof(buf), "\033[M%c%c%c", b + 32, x + 32, y + 32);
	write_wrapper(p->term->masterPty, buf, strlen(buf));
}

static void mouseTrackingVT200(TVterm *p)
{
	char buf[7];
	int b, x, y;

	if (isMouseButtonDown(MOUSE_BUTTON_MB1))
		b = 0;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB2))
		b = 1;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB3))
		b = 2;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB4))
		b = 0 + 64;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB5))
		b = 1 + 64;
	else if (isMouseButtonUp(MOUSE_BUTTON_MB1))
		b = 3;
	else if (isMouseButtonUp(MOUSE_BUTTON_MB2))
		b = 3;
	else if (isMouseButtonUp(MOUSE_BUTTON_MB3))
		b = 3;
	else
		return;
	if (keyboard_isShiftPressed())
		b += 4;
	if (keyboard_isAltPressed())
		b += 8;
	if (keyboard_isCtrlPressed())
		b += 16;
	x = mouseInfo.x + 1;
	if (x > 255 - 32) x = 255 - 32;
	y = mouseInfo.y + 1;
	if (y > 255 - 32) y = 255 - 32;
	snprintf(buf, sizeof(buf), "\033[M%c%c%c", b + 32, x + 32, y + 32);
	write_wrapper(p->term->masterPty, buf, strlen(buf));
}

static void mouseTrackingButtonEvents(TVterm *p)
{
	char buf[7];
	int b, x, y;

	if (isMouseButtonDown(MOUSE_BUTTON_MB1))
		b = 0;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB2))
		b = 1;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB3))
		b = 2;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB4))
		b = 0 + 64;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB5))
		b = 1 + 64;
	else if (isMouseButtonUp(MOUSE_BUTTON_MB1))
		b = 3;
	else if (isMouseButtonUp(MOUSE_BUTTON_MB2))
		b = 3;
	else if (isMouseButtonUp(MOUSE_BUTTON_MB3))
		b = 3;
	else if (isMouseButtonMove(MOUSE_BUTTON_MB1))
		b = 0 + 32;
	else if (isMouseButtonMove(MOUSE_BUTTON_MB2))
		b = 1 + 32;
	else if (isMouseButtonMove(MOUSE_BUTTON_MB3))
		b = 2 + 32;
	else
		return;
	if (keyboard_isShiftPressed())
		b += 4;
	if (keyboard_isAltPressed())
		b += 8;
	if (keyboard_isCtrlPressed())
		b += 16;
	x = mouseInfo.x + 1;
	if (x > 255 - 32) x = 255 - 32;
	y = mouseInfo.y + 1;
	if (y > 255 - 32) y = 255 - 32;
	snprintf(buf, sizeof(buf), "\033[M%c%c%c", b + 32, x + 32, y + 32);
	write_wrapper(p->term->masterPty, buf, strlen(buf));
}

static void mouseTrackingAnyEvents(TVterm *p)
{
	char buf[7];
	int b, x, y;

	if (isMouseButtonDown(MOUSE_BUTTON_MB1))
		b = 0;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB2))
		b = 1;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB3))
		b = 2;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB4))
		b = 0 + 64;
	else if (isMouseButtonDown(MOUSE_BUTTON_MB5))
		b = 1 + 64;
	else if (isMouseButtonUp(MOUSE_BUTTON_MB1))
		b = 3;
	else if (isMouseButtonUp(MOUSE_BUTTON_MB2))
		b = 3;
	else if (isMouseButtonUp(MOUSE_BUTTON_MB3))
		b = 3;
	else if (isMouseButtonMove(MOUSE_BUTTON_MB1))
		b = 0 + 32;
	else if (isMouseButtonMove(MOUSE_BUTTON_MB2))
		b = 1 + 32;
	else if (isMouseButtonMove(MOUSE_BUTTON_MB3))
		b = 2 + 32;
	else if ((mouseInfo.button == MOUSE_BUTTON_NONE) &&
		 (mouseInfo.ox != mouseInfo.x || mouseInfo.oy != mouseInfo.y))
		b = 3;
	else
		return;
	if (keyboard_isShiftPressed())
		b += 4;
	if (keyboard_isAltPressed())
		b += 8;
	if (keyboard_isCtrlPressed())
		b += 16;
	x = mouseInfo.x + 1;
	if (x > 255 - 32) x = 255 - 32;
	y = mouseInfo.y + 1;
	if (y > 255 - 32) y = 255 - 32;
	snprintf(buf, sizeof(buf), "\033[M%c%c%c", b + 32, x + 32, y + 32);
	write_wrapper(p->term->masterPty, buf, strlen(buf));
}

void mouse_getPackets(TVterm *p)
{
	static u_char packet[MAX_PACKET_SIZE];
	static int packetSize;
	int i;
	ssize_t nbytes;
	u_char buf[BUF_SIZE];

	assert(initialized);
	assert(p != NULL);

#if defined (__NetBSD__) || defined (__OpenBSD__)
	if (mouse.type == MOUSE_TYPE_WSMOUSE) {
		analyzeWSConsEvents(p);
		return;
	}
#endif

	nbytes = read(mouse.fd, buf, BUF_SIZE);
	if (nbytes > 0) {
		for (i = 0; i < nbytes; i++) {
			if (packetSize == 0) {
				if ((buf[i] & mouse.headMask) == mouse.headId) {
					packet[0] = buf[i];
					packetSize = 1;
				}
				continue;
			}
			if (mouse.type != MOUSE_TYPE_PS2 &&
			    mouse.type != MOUSE_TYPE_IMPS2 &&
			    mouse.type != MOUSE_TYPE_EXPS2 &&
			    ((buf[i] & mouse.dataMask) || buf[i] == 0x80)) {
				packetSize = 0;
				continue;
			}
			packet[packetSize++] = buf[i];
			if (packetSize == mouse.packetSize) {
				analyzePacket(p, packet);
				packetSize = 0;
			}
		}
	}
}

