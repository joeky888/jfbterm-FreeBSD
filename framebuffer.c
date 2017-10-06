/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
 *
 * JFBTERM/ME - J framebuffer terminal/Multilingual Enhancement -
 * Copyright (C) 2003 Fumitoshi UKAI <ukai@debian.or.jp>
 * Copyright (C) 1999 Noritoshi MASUICHI <nmasu@ma3.justnet.ne.jp>
 *
 * KON2 - Kanji ON Console -
 * Copyright (C) 1992,1993 Atusi MAEDA <mad@math.keio.ac.jp>
 * Copyright (C) 1992-1996 Takashi MANABE <manabe@papilio.tutics.tut.ac.jp>
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
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined (__linux__)
#include <sys/sysmacros.h>
#include <linux/fb.h>
#include <linux/kd.h>
#elif defined (__FreeBSD__)
#include <osreldate.h>
#if __FreeBSD_version >= 410000
#include <sys/consio.h>
#include <sys/fbio.h>
#else
#include <machine/console.h>
#endif
#include <libutil.h>
#elif defined (__NetBSD__) || defined (__OpenBSD__)
#include <time.h>
#include <dev/wscons/wsconsio.h>
#endif

#ifdef ENABLE_VGA16FB
#if defined (__linux__)
#include <sys/io.h>
#elif defined (__FreeBSD__)
#include <machine/sysarch.h>
#include <machine/bus.h>
#include <sys/kbio.h>
#endif
#endif

#include "console.h"
#include "accessor.h"
#include "framebuffer.h"
#include "memctl.h"
#include "palette.h"
#include "privilege.h"

TFrameBuffer gFramebuffer;

static TFrameBuffer *const self = &gFramebuffer;
static bool initialized;

#if defined (__linux__)
static const TFrameBufferAccessor accessors[] = {
#ifdef ENABLE_VGA16FB
	{
		4, FB_TYPE_VGA_PLANES, FB_VISUAL_PSEUDOCOLOR,
		accessor_fill_vga16fb,
		accessor_overlay_vga16fb,
		accessor_reverse_vga16fb
	},
#endif
#ifdef ENABLE_8BPP
	{
		8, FB_TYPE_PACKED_PIXELS, FB_VISUAL_PSEUDOCOLOR,
		accessor_fill_8bpp,
		accessor_overlay_8bpp,
		accessor_reverse_8bpp
	},
#endif
#ifdef ENABLE_15BPP
	{
		15, FB_TYPE_PACKED_PIXELS, FB_VISUAL_TRUECOLOR,
		accessor_fill_15bpp,
		accessor_overlay_15bpp,
		accessor_reverse_15bpp
	},
#endif
#ifdef ENABLE_15BPP
	{
		15, FB_TYPE_PACKED_PIXELS, FB_VISUAL_DIRECTCOLOR,
		accessor_fill_15bpp,
		accessor_overlay_15bpp,
		accessor_reverse_15bpp
	},
#endif
#ifdef ENABLE_16BPP
	{
		16, FB_TYPE_PACKED_PIXELS, FB_VISUAL_TRUECOLOR,
		accessor_fill_16bpp,
		accessor_overlay_16bpp,
		accessor_reverse_16bpp
	},
#endif
#ifdef ENABLE_16BPP
	{
		16, FB_TYPE_PACKED_PIXELS, FB_VISUAL_DIRECTCOLOR,
		accessor_fill_16bpp,
		accessor_overlay_16bpp,
		accessor_reverse_16bpp
	},
#endif
#ifdef ENABLE_24BPP
	{
		24, FB_TYPE_PACKED_PIXELS, FB_VISUAL_TRUECOLOR,
		accessor_fill_24bpp,
		accessor_overlay_24bpp,
		accessor_reverse_24bpp
	},
#endif
#ifdef ENABLE_24BPP
	{
		24, FB_TYPE_PACKED_PIXELS, FB_VISUAL_DIRECTCOLOR,
		accessor_fill_24bpp,
		accessor_overlay_24bpp,
		accessor_reverse_24bpp
	},
#endif
#ifdef ENABLE_32BPP
	{
		32, FB_TYPE_PACKED_PIXELS, FB_VISUAL_TRUECOLOR,
		accessor_fill_32bpp,
		accessor_overlay_32bpp,
		accessor_reverse_32bpp
	},
#endif
#ifdef ENABLE_32BPP
	{
		32, FB_TYPE_PACKED_PIXELS, FB_VISUAL_DIRECTCOLOR,
		accessor_fill_32bpp,
		accessor_overlay_32bpp,
		accessor_reverse_32bpp
	},
#endif
	{
		0, 0, 0, NULL, NULL, NULL
	}
};
#elif defined (__FreeBSD__)
static const TFrameBufferAccessor accessors[] = {
#ifdef ENABLE_VGA16FB
	{
		4, V_INFO_MM_PLANAR,
		accessor_fill_vga16fb,
		accessor_overlay_vga16fb,
		accessor_reverse_vga16fb
	},
#endif
#ifdef ENABLE_8BPP
	{
		8, V_INFO_MM_PACKED,
		accessor_fill_8bpp,
		accessor_overlay_8bpp,
		accessor_reverse_8bpp
	},
#endif
#ifdef ENABLE_15BPP
	{
		15, V_INFO_MM_DIRECT,
		accessor_fill_15bpp,
		accessor_overlay_15bpp,
		accessor_reverse_15bpp
	},
#endif
#ifdef ENABLE_16BPP
	{
		16, V_INFO_MM_DIRECT,
		accessor_fill_16bpp,
		accessor_overlay_16bpp,
		accessor_reverse_16bpp
	},
#endif
#ifdef ENABLE_24BPP
	{
		24, V_INFO_MM_DIRECT,
		accessor_fill_24bpp,
		accessor_overlay_24bpp,
		accessor_reverse_24bpp
	},
#endif
#ifdef ENABLE_32BPP
	{
		32, V_INFO_MM_DIRECT,
		accessor_fill_32bpp,
		accessor_overlay_32bpp,
		accessor_reverse_32bpp
	},
#endif
	{
		0, 0, NULL, NULL, NULL
	}
};
#elif defined (__NetBSD__) || defined (__OpenBSD__)
static const TFrameBufferAccessor accessors[] = {
#ifdef ENABLE_8BPP
	{
		8,
		accessor_fill_8bpp,
		accessor_overlay_8bpp,
		accessor_reverse_8bpp
	},
#endif
#ifdef ENABLE_15BPP
	{
		15,
		accessor_fill_15bpp,
		accessor_overlay_15bpp,
		accessor_reverse_15bpp
	},
#endif
#ifdef ENABLE_16BPP
	{
		16,
		accessor_fill_16bpp,
		accessor_overlay_16bpp,
		accessor_reverse_16bpp
	},
#endif
#ifdef ENABLE_24BPP
	{
		24,
		accessor_fill_24bpp,
		accessor_overlay_24bpp,
		accessor_reverse_24bpp
	},
#endif
#ifdef ENABLE_32BPP
	{
		32,
		accessor_fill_32bpp,
		accessor_overlay_32bpp,
		accessor_reverse_32bpp
	},
#endif
	{
		0, NULL, NULL, NULL
	}
};
#else
	#error not implement
#endif

#ifdef ENABLE_VGA16FB
static struct vgaregs {
	unsigned char attr[21];
	unsigned char grph[9];
} vga16fbregs = {
	attr: { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		0x01, 0x00, 0x0f, 0x00, 0x00 },
	grph: { 0x00, 0x0f, 0x00, 0x20, 0x03, 0x00, 0x05, 0x00,
		0xff },
};
#endif

static struct {
	unsigned long base;
	unsigned long size;
	bool enable;
} writecombine = {
	0,
	0,
	false
};

#if defined (__FreeBSD__) || defined (__OpenBSD__)
#define DEFAULT_VIDEO_WIDTH  (800)
#define DEFAULT_VIDEO_HEIGHT (600)
#define DEFAULT_VIDEO_DEPTH  (8)
static struct {
	int width;
	int height;
	int depth;
} video = {
	DEFAULT_VIDEO_WIDTH,
	DEFAULT_VIDEO_HEIGHT,
	DEFAULT_VIDEO_DEPTH
};
#endif

#if defined (__linux__)
static struct fb_var_screeninfo old_fb_var_screeninfo;
static bool modified_fb_var_screen_info;
#elif defined (__FreeBSD__)
static video_display_start_t old_video_display_start;
static bool modified_video_display_start;
static int old_video_mode;
static bool modified_video_mode;
#endif

static void finalize(void);
static void finalizer(void);
#if defined (__FreeBSD__) || defined (__OpenBSD__)
static void configVideoMode(const char *config);
#endif
static void configWriteCombineMode(const char *config);
static void configWriteCombineBase(const char *config);
static void configWriteCombineSize(const char *config);
#if defined (__linux__)
static void getVariableScreenInfo(struct fb_var_screeninfo *fb_var_screeninfo);
static void setVariableScreenInfo(struct fb_var_screeninfo *fb_var_screeninfo);
static void getFixedScreenInfo(struct fb_fix_screeninfo *fb_fix_screeninfo);
#endif
#if defined (__FreeBSD__)
static void setVideoMode(int *mode);
static void getVideoInfo(int mode, video_info_t *video_info);
static void getVideoAdapterInfo(video_adapter_info_t *video_adapter_info);
#if __FreeBSD_version >= 603000
static bool loadVESAModule(void);
#endif
static int findVideoMode(int width, int height, int depth);
static void getDisplayStart(video_display_start_t *video_display_start);
#endif
#if defined (__linux__)
static void setDisplayStart(struct fb_var_screeninfo *fb_var_screeninfo);
#elif defined (__FreeBSD__)
static void setDisplayStart(video_display_start_t *video_display_start);
#endif
#if defined (__NetBSD__) || defined (__OpenBSD__)
static void getFrameBufferInformation(struct wsdisplay_fbinfo *wsdisplay_fbinfo);
static int getLineBytes(void);
#endif
#if defined (__OpenBSD__)
static bool setGraphicsMode(int width, int height, int depth);
#endif
#ifdef ENABLE_VGA16FB
static void vgaSetRegisters(struct vgaregs *vgaregs);
static void vgaSetBlank(bool blank);
#endif
#if defined (__linux__)
static int getAccessorIndex(struct fb_var_screeninfo *fb_var_screeninfo,
			    struct fb_fix_screeninfo *fb_fix_screeninfo);
#elif defined (__FreeBSD__)
static int getAccessorIndex(video_info_t *video_info);
#elif defined (__NetBSD__) || defined (__OpenBSD__)
static int getAccessorIndex(struct wsdisplay_fbinfo *wsdisplay_fbinfo);
#endif

void framebuffer_initialize(void)
{
	int console;
#if defined (__linux__)
	int fd;
	struct fb_con2fbmap c2m;
#endif

	assert(!initialized);

	atexit(finalizer);
	initialized = true;
	self->device[0] = '\0';
	self->fd = -1;
	self->height = 0;
	self->width = 0;
	self->bytesPerLine = 0;
	self->offset = 0;
	self->length = 0;
	self->memory = MAP_FAILED;

#if __FreeBSD_version >= 603000
	if (!loadVESAModule())
		errx(1, "VESA kernel module not loaded.");
#endif

	/* Get framebuffer device */
	console = console_getActive();
	if (console < 1)
		errx(1, "Could not get active console.");
#if defined (__linux__)
	privilege_on();
	fd = open("/dev/fb0", O_RDWR);
	privilege_off();
	if (fd == -1)
		err(1, "%s", "/dev/fb0");
	c2m.console = console;
	c2m.framebuffer = 0;
	if (ioctl(fd, FBIOGET_CON2FBMAP, &c2m) == -1)
		warn("ioctl(FBIOGET_CON2FBMAP)");
	close(fd);
	if (c2m.framebuffer == -1)
		c2m.framebuffer = 0;
	snprintf(self->device, MAXPATHLEN, "/dev/fb%d", c2m.framebuffer);
	self->device[MAXPATHLEN - 1] = '\0';
#elif defined (__FreeBSD__)
	snprintf(self->device, MAXPATHLEN, "/dev/ttyv%x", console - 1);
	self->device[MAXPATHLEN - 1] = '\0';
#elif defined (__NetBSD__)
	snprintf(self->device, MAXPATHLEN, "/dev/ttyE%d", console - 1);
	self->device[MAXPATHLEN - 1] = '\0';
#elif defined (__OpenBSD__)
	snprintf(self->device, MAXPATHLEN, "/dev/ttyC%d", console - 1);
	self->device[MAXPATHLEN - 1] = '\0';
#else
	#error not implement
#endif
	setenv("FRAMEBUFFER", self->device, 1);
}

static void finalize(void)
{
	assert(initialized);

	if (self->memory != MAP_FAILED) {
		if (munmap(self->memory, self->length) == -1)
			warn("munmap()");
		self->memory = MAP_FAILED;
	}
	palette_restore();
#if defined (__linux__)
	if (modified_fb_var_screen_info) {
		old_fb_var_screeninfo.activate = FB_ACTIVATE_NOW;
		setVariableScreenInfo(&old_fb_var_screeninfo);
		setDisplayStart(&old_fb_var_screeninfo);
		modified_fb_var_screen_info = false;
	}
#elif defined (__FreeBSD__)
	if (modified_video_display_start) {
		setDisplayStart(&old_video_display_start);
		modified_video_display_start = false;
	}
	if (modified_video_mode) {
		setVideoMode(&old_video_mode);
		modified_video_mode = false;
	}
#endif
	close(self->fd);
	self->fd = -1;
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

#if defined (__FreeBSD__) || defined (__OpenBSD__)
static void configVideoMode(const char *config)
{
	int width, height, depth;

	assert(initialized);

	if (config != NULL) {
		if (sscanf(config, "%dx%dx%d", &width, &height, &depth) != 3) {
			warnx("Invalid video mode: %s", config);
			width  = DEFAULT_VIDEO_WIDTH;
			height = DEFAULT_VIDEO_HEIGHT;
			depth  = DEFAULT_VIDEO_DEPTH;
		}
		video.width  = width;
		video.height = height;
		video.depth  = depth;
	}
}
#endif

static void configWriteCombineMode(const char *config)
{
	bool found;
	int i;

	static const struct {
		const char *key;
		const bool enable;
	} list[] = {
		{ "On",  true  },
		{ "Off", false },
		{ NULL,  false }
	};

	assert(initialized);

	writecombine.enable = false;
	if (config != NULL) {
		found = false;
		for (i = 0; list[i].key != NULL; i++) {
			if (strcasecmp(list[i].key, config) == 0) {
				writecombine.enable = list[i].enable;
				found = true;
				break;
			}
		}
		if (!found)
			warnx("Invalid write-combining mode: %s", config);
	}
}

static void configWriteCombineBase(const char *config)
{
	unsigned long base;
	char *end;

	assert(initialized);

	if (config != NULL) {
		base = strtoul(config, &end, 16);
		if (base != ULONG_MAX && *end == '\0')
			writecombine.base = base;
		else
			warnx("Invalid write-combining base: %s", config);
	}
}

static void configWriteCombineSize(const char *config)
{
	unsigned long size;
	char *end;

	assert(initialized);

	if (config != NULL) {
		size = strtoul(config, &end, 16);
		if (size != ULONG_MAX && *end == '\0')
			writecombine.size = size;
		else
			warnx("Invalid write-combining size: %s", config);
	}
}

void framebuffer_configure(TCaps *caps)
{
	const char *config;

	assert(initialized);
	assert(caps != NULL);

#if defined (__FreeBSD__) || defined (__OpenBSD__)
	config = caps_findFirst(caps, "video.mode");
	configVideoMode(config);
#endif
	config = caps_findFirst(caps, "writecombine.mode");
	configWriteCombineMode(config);
	config = caps_findFirst(caps, "writecombine.base");
	configWriteCombineBase(config);
	config = caps_findFirst(caps, "writecombine.size");
	configWriteCombineSize(config);
}

#if defined (__linux__)
static void getVariableScreenInfo(struct fb_var_screeninfo *fb_var_screeninfo)
{
	assert(initialized);
	assert(fb_var_screeninfo != NULL);

	if (ioctl(self->fd, FBIOGET_VSCREENINFO, fb_var_screeninfo) == -1)
		errx(1, "Could not get variable screen information.");
}
#endif

#if defined (__linux__)
static void setVariableScreenInfo(struct fb_var_screeninfo *fb_var_screeninfo)
{
	assert(initialized);
	assert(fb_var_screeninfo != NULL);

	if (ioctl(self->fd, FBIOPUT_VSCREENINFO, fb_var_screeninfo) == -1)
		errx(1, "Could not set variable screen information.");
}
#endif

#if defined (__linux__)
static void getFixedScreenInfo(struct fb_fix_screeninfo *fb_fix_screeninfo)
{
	assert(initialized);
	assert(fb_fix_screeninfo != NULL);

	if (ioctl(self->fd, FBIOGET_FSCREENINFO, fb_fix_screeninfo) == -1)
		errx(1, "Could not get fixed screen information.");
}
#endif

#if defined (__FreeBSD__)
static void setVideoMode(int *mode)
{
	assert(initialized);
	assert(mode != NULL);

	if (ioctl(self->fd, FBIO_SETMODE, mode) == -1)
		errx(1, "Could not set video mode.");
}
#endif

#if defined (__FreeBSD__)
static void getVideoInfo(int mode, video_info_t *video_info)
{
	assert(initialized);
	assert(video_info != NULL);

	video_info->vi_mode = mode;
	if (ioctl(self->fd, FBIO_MODEINFO, video_info) == -1)
		errx(1, "Could not get video infomation.");
}
#endif

#if defined (__FreeBSD__)
static void getVideoAdapterInfo(video_adapter_info_t *video_adapter_info)
{
	assert(initialized);
	assert(video_adapter_info != NULL);

	if (ioctl(self->fd, FBIO_ADPINFO, video_adapter_info) == -1)
		errx(1, "Could not get video adapter infomation.");
}
#endif

#if __FreeBSD_version >= 603000
static bool loadVESAModule(void)
{
	bool result;

	assert(initialized);

	result = false;
	if (kld_isloaded("vesa") != 0)
		result = true; /* already loaded */
	else {
		privilege_on();
		if (kld_load("vesa") == 0)
			result = true; /* load succeed */
		privilege_off();
	}
	return result;
}
#endif

#if defined (__FreeBSD__)
static int findVideoMode(int width, int height, int depth)
{
	int result;
	video_info_t video_info;

	assert(initialized);

	result = -1;
	bzero(&video_info, sizeof(video_info));
	video_info.vi_width  = width;
	video_info.vi_height = height;
	video_info.vi_depth  = depth;
	if (ioctl(self->fd, FBIO_FINDMODE, &video_info) != -1)
		result = video_info.vi_mode;
	return result;
}
#endif

#if defined (__FreeBSD__)
static void getDisplayStart(video_display_start_t *video_display_start)
{
	assert(initialized);
	assert(video_display_start != NULL);

	if (ioctl(self->fd, FBIO_GETDISPSTART, video_display_start) == -1)
		warn("ioctl(FBIO_GETDISPSTART)");
}
#endif

#if defined (__linux__)
static void setDisplayStart(struct fb_var_screeninfo *fb_var_screeninfo)
{
	assert(initialized);
	assert(fb_var_screeninfo != NULL);

	if (ioctl(self->fd, FBIOPAN_DISPLAY, fb_var_screeninfo) == -1)
		warn("ioctl(FBIOPAN_DISPLAY)");
}
#elif defined (__FreeBSD__)
static void setDisplayStart(video_display_start_t *video_display_start)
{
	assert(initialized);
	assert(video_display_start != NULL);

	if (ioctl(self->fd, FBIO_SETDISPSTART, video_display_start) == -1)
		warn("ioctl(FBIO_SETDISPSTART)");
}
#endif

#if defined (__NetBSD__) || defined (__OpenBSD__)
static void getFrameBufferInformation(struct wsdisplay_fbinfo *wsdisplay_fbinfo)
{
	assert(initialized);
	assert(wsdisplay_fbinfo != NULL);

	if (ioctl(self->fd, WSDISPLAYIO_GINFO, wsdisplay_fbinfo) == -1)
		errx(1, "Could not get framebuffer infomation.");
}
#endif

#if defined (__NetBSD__) || defined (__OpenBSD__)
static int getLineBytes(void)
{
	int linebytes;

	assert(initialized);

	if (ioctl(self->fd, WSDISPLAYIO_LINEBYTES, &linebytes) == -1)
		errx(1, "Could not get line bytes.");
	if (linebytes == -1)
		errx(1, "Could not get line bytes.");
	return linebytes;
}
#endif

#if defined (__OpenBSD__)
static bool setGraphicsMode(int width, int height, int depth)
{
	bool result;
	struct wsdisplay_gfx_mode wsdisplay_gfx_mode;

	assert(initialized);

	result = false;
	wsdisplay_gfx_mode.width  = width;
	wsdisplay_gfx_mode.height = height;
	wsdisplay_gfx_mode.depth  = depth;
	if (ioctl(self->fd, WSDISPLAYIO_SETGFXMODE, &wsdisplay_gfx_mode) != -1)
		result = true;
	return result;
}
#endif

#ifdef ENABLE_VGA16FB
static void vgaSetRegisters(struct vgaregs *vgaregs)
{
	int i;

	PORT_INBYTE(0x3da); PORT_OUTBYTE(0x3c0, 0x00);
	/* Graphics Registers */
	for (i = 0; i < sizeof(vgaregs->grph); i++) {
		PORT_OUTBYTE(0x3ce, i);
		PORT_OUTBYTE(0x3cf, vgaregs->grph[i]);
	}
	/* Attribute Controller Registers */
	for (i = 0; i < sizeof(vgaregs->attr); i++) {
		PORT_INBYTE(0x3da);
		PORT_OUTBYTE(0x3c0, i);
		PORT_OUTBYTE(0x3c0, vgaregs->attr[i]);
	}
	PORT_INBYTE(0x3da); PORT_OUTBYTE(0x3c0, 0x20);
}

static void vgaSetBlank(bool blank)
{
	unsigned char c;

	PORT_OUTBYTE(0x3c4, 0x01);
	c = PORT_INBYTE(0x3c5);
	PORT_OUTBYTE(0x3c4, 0x01);
	PORT_OUTBYTE(0x3c5, ((blank) ? (c |= 0x20) : (c &= 0xdf)));
}
#endif

#if defined (__linux__)
void framebuffer_getColorMap(struct fb_cmap *cmap)
{
	assert(initialized);
	assert(cmap != NULL);

	if (ioctl(self->fd, FBIOGETCMAP, cmap) == -1)
		errx(1, "Could not get color map.");
}
#elif defined (__FreeBSD__)
void framebuffer_getColorMap(video_color_palette_t *cmap)
{
#ifdef ENABLE_VGA16FB
	unsigned char r, g, b;
	int i;
#endif

	assert(initialized);
	assert(cmap != NULL);

	switch (gFramebuffer.accessor.bitsPerPixel) {
#ifdef ENABLE_VGA16FB
	case 4:
		PORT_OUTBYTE(0x3c6, 0xff);
		PORT_INBYTE(0x3da); PORT_OUTBYTE(0x3c7, 0x00);
		for (i = cmap->index; i < cmap->count; i++) {
			r = PORT_INBYTE(0x3c9) << 2; PORT_INBYTE(0x84);
			g = PORT_INBYTE(0x3c9) << 2; PORT_INBYTE(0x84);
			b = PORT_INBYTE(0x3c9) << 2; PORT_INBYTE(0x84);
			cmap->red[i] = r;
			cmap->green[i] = g;
			cmap->blue[i] = b;
		}
		PORT_INBYTE(0x3da); PORT_OUTBYTE(0x3c0, 0x20);
		break;
#endif
	default:
		if (ioctl(self->fd, FBIO_GETPALETTE, cmap) == -1)
			errx(1, "Could not get color map.");
		break;
	}
}
#elif defined (__NetBSD__) || defined (__OpenBSD__)
void framebuffer_getColorMap(struct wsdisplay_cmap *cmap)
{
	assert(initialized);
	assert(cmap != NULL);

	if (ioctl(self->fd, WSDISPLAYIO_GETCMAP, cmap) == -1)
		errx(1, "Could not get color map.");
}
#else
	#error not implement
#endif

#if defined (__linux__)
void framebuffer_setColorMap(struct fb_cmap *cmap)
{
	assert(initialized);
	assert(cmap != NULL);

	if (!console_isActive()) {
		warnx("Set color map skipped.");
		return;
	}
	if (ioctl(self->fd, FBIOPUTCMAP, cmap) == -1)
		errx(1, "Could not set color map.");
}
#elif defined (__FreeBSD__)
void framebuffer_setColorMap(video_color_palette_t *cmap)
{
#ifdef ENABLE_VGA16FB
	unsigned char r, g, b;
	int i;
#endif

	assert(initialized);
	assert(cmap != NULL);

	if (!console_isActive()) {
		warnx("Set color map skipped.");
		return;
	}
	switch (gFramebuffer.accessor.bitsPerPixel) {
#ifdef ENABLE_VGA16FB
	case 4:
		PORT_OUTBYTE(0x3c6, 0xff);
		PORT_INBYTE(0x3da); PORT_OUTBYTE(0x3c8, 0x00);
		for (i = cmap->index; i < cmap->count; i++) {
			r = cmap->red[i] >> 2;
			g = cmap->green[i] >> 2;
			b = cmap->blue[i] >> 2;
			PORT_OUTBYTE(0x3c9, r); PORT_INBYTE(0x84);
			PORT_OUTBYTE(0x3c9, g); PORT_INBYTE(0x84);
			PORT_OUTBYTE(0x3c9, b); PORT_INBYTE(0x84);
		}
		PORT_INBYTE(0x3da); PORT_OUTBYTE(0x3c0, 0x20);
		break;
#endif
	default:
		if (ioctl(self->fd, FBIO_SETPALETTE, cmap) == -1)
			errx(1, "Could not set color map.");
		break;
	}
}
#elif defined (__NetBSD__) || defined (__OpenBSD__)
void framebuffer_setColorMap(struct wsdisplay_cmap *cmap)
{
	assert(initialized);
	assert(cmap != NULL);

	if (!console_isActive()) {
		warnx("Set color map skipped.");
		return;
	}
	if (ioctl(self->fd, WSDISPLAYIO_PUTCMAP, cmap) == -1)
		errx(1, "Could not set color map.");
}
#else
	#error not implement
#endif

bool framebuffer_setBlank(const int blank)
{
	assert(initialized);

	if (!console_isActive())
		return false;
#if defined (__linux__)
	switch (gFramebuffer.accessor.bitsPerPixel) {
#ifdef ENABLE_VGA16FB
	case 4:
		vgaSetBlank(blank != FB_BLANK_UNBLANK);
		break;
#endif
	default:
		if (ioctl(self->fd, FBIOBLANK, blank) == -1) {
			warn("ioctl(FBIOBLANK)");
			return false;
		}
		break;
	}
#elif defined (__FreeBSD__)
	switch (gFramebuffer.accessor.bitsPerPixel) {
#ifdef ENABLE_VGA16FB
	case 4:
		vgaSetBlank(blank != V_DISPLAY_ON);
		break;
#endif
	default:
		if (ioctl(self->fd, FBIO_BLANK, &blank) == -1) {
			warn("ioctl(FBIO_BLANK)");
			return false;
		}
		break;
	}
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	if (ioctl(self->fd, WSDISPLAYIO_SVIDEO, &blank) == -1) {
		warn("ioctl(WSDISPLAYIO_SVIDEO)");
		return false;
	}
#else
	#error not implement
#endif
	return true;
}

void framebuffer_open(void)
{
	struct stat st;
	int i;
	long pageMask;

#if defined (__linux__)
	struct fb_var_screeninfo fb_var_screeninfo;
	struct fb_fix_screeninfo fb_fix_screeninfo;
#elif defined (__FreeBSD__)
	video_info_t video_info;
	video_adapter_info_t video_adapter_info;
	int video_mode;
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	struct wsdisplay_fbinfo wsdisplay_fbinfo;
	int wsdisplay_type;
#else
	#error not implement
#endif

	assert(initialized);

	/* Open framebuffer device */
	privilege_on();
	self->fd = open(self->device, O_RDWR);
	privilege_off();
	if (self->fd == -1)
		err(1, "%s", self->device);
	if (fcntl(self->fd, F_SETFD, FD_CLOEXEC) == -1)
		err(1, "fcntl(F_SETFD)");

	/* Check framebuffer device */
	if (fstat(self->fd, &st) == -1)
		err(1, "fstat(%s)", self->device);
	if (!S_ISCHR(st.st_mode))
		errx(1, "%s is not a character device.",
		     self->device);
#if defined (__linux__)
	if (major(st.st_rdev) != FB_MAJOR)
		errx(1, "%s is not a framebuffer device.", self->device);
#elif defined (__FreeBSD__)
	if (ioctl(self->fd, FBIO_GETMODE, &old_video_mode) == -1)
		errx(1, "%s is not a framebuffer device.", self->device);
	console_setSC_PIXEL_MODE(old_video_mode >= M_VESA_BASE &&
				 old_video_mode <= M_VESA_MODE_MAX);
#elif defined (__NetBSD__)
	if (ioctl(self->fd, WSDISPLAYIO_GTYPE, &wsdisplay_type) == -1)
		errx(1, "%s is not a framebuffer device.", self->device);
	if (wsdisplay_type != WSDISPLAY_TYPE_VESA)
		errx(1, "%s is not a framebuffer device.", self->device);
#elif defined (__OpenBSD__)
	if (ioctl(self->fd, WSDISPLAYIO_GTYPE, &wsdisplay_type) == -1)
		errx(1, "%s is not a framebuffer device.", self->device);
	if (wsdisplay_type != WSDISPLAY_TYPE_PCIVGA)
		errx(1, "%s is not a framebuffer device.", self->device);
#else
	#error not implement
#endif

#if defined (__linux__)
	console_setMode(KD_GRAPHICS);
	getVariableScreenInfo(&fb_var_screeninfo);
	old_fb_var_screeninfo = fb_var_screeninfo;
	if (fb_var_screeninfo.xres_virtual != fb_var_screeninfo.xres ||
	    fb_var_screeninfo.yres_virtual != fb_var_screeninfo.yres) {
		fb_var_screeninfo.xres_virtual = fb_var_screeninfo.xres;
		fb_var_screeninfo.yres_virtual = fb_var_screeninfo.yres;
		fb_var_screeninfo.xoffset = 0;
		fb_var_screeninfo.yoffset = 0;
		fb_var_screeninfo.activate = FB_ACTIVATE_NOW;
		setVariableScreenInfo(&fb_var_screeninfo);
		modified_fb_var_screen_info = true;
	}
	getFixedScreenInfo(&fb_fix_screeninfo);
#elif defined (__FreeBSD__)
	video_mode = findVideoMode(video.width, video.height, video.depth);
	if (video_mode == -1)
		errx(1, "Could not set graphics mode (%dx%dx%d). "
			"Please check your video card information.",
			video.width, video.height, video.depth);
	if (old_video_mode != video_mode) {
		setVideoMode(&video_mode);
		modified_video_mode = true;
	}
	console_setMode(KD_GRAPHICS);
	getVideoInfo(video_mode, &video_info);
	if (!(video_info.vi_flags & V_INFO_GRAPHICS))
		errx(1, "Unable to use graphics.");
	if (!(video_info.vi_flags & V_INFO_LINEAR) &&
	    video_info.vi_mem_model != V_INFO_MM_PLANAR)
		errx(1, "Unable to use linear framebuffer.");
	getVideoAdapterInfo(&video_adapter_info);
	getDisplayStart(&old_video_display_start);
#elif defined (__NetBSD__)
	console_setMode(WSDISPLAYIO_MODE_MAPPED);
	getFrameBufferInformation(&wsdisplay_fbinfo);
#elif defined (__OpenBSD__)
	if (!setGraphicsMode(video.width, video.height, video.depth))
		errx(1, "Could not set graphics mode (%dx%dx%d). "
			"Please check your video card information.",
			video.width, video.height, video.depth);
	console_setMode(WSDISPLAYIO_MODE_DUMBFB);
	getFrameBufferInformation(&wsdisplay_fbinfo);
#else
	#error not implement
#endif

	/* Framebuffer accessor */
#if defined (__linux__)
	i = getAccessorIndex(&fb_var_screeninfo, &fb_fix_screeninfo);
#elif defined (__FreeBSD__)
	i = getAccessorIndex(&video_info);
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	i = getAccessorIndex(&wsdisplay_fbinfo);
#else
	#error not implement
#endif
	if (i == -1)
		errx(1, "Framebuffer accessor not found.");
	self->accessor = accessors[i];

	/* Move viewport to upper left corner */
#if defined (__linux__)
	if (fb_var_screeninfo.xoffset != 0 || fb_var_screeninfo.yoffset != 0) {
		fb_var_screeninfo.xoffset = 0;
		fb_var_screeninfo.yoffset = 0;
		setDisplayStart(&fb_var_screeninfo);
		modified_fb_var_screen_info = true;
	}
#elif defined (__FreeBSD__)
	if (old_video_display_start.x != 0 || old_video_display_start.y != 0) {
		video_display_start_t video_display_start;
		video_display_start.x = 0;
		video_display_start.y = 0;
		setDisplayStart(&video_display_start);
		modified_video_display_start = true;
	}
#endif

	/* Framebuffer information */
#if defined (__linux__)
	self->width = fb_var_screeninfo.xres;
	self->height = fb_var_screeninfo.yres;
	self->bytesPerLine = fb_fix_screeninfo.line_length;
	self->offset = (u_long)fb_fix_screeninfo.smem_start;
	self->length = fb_fix_screeninfo.smem_len;
#elif defined (__FreeBSD__)
	self->width = video_info.vi_width;
	self->height = video_info.vi_height;
	self->bytesPerLine = video_adapter_info.va_line_width;
	self->offset = (u_long)video_adapter_info.va_window;
	self->length = video_adapter_info.va_window_size;
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	self->width = wsdisplay_fbinfo.width;
	self->height = wsdisplay_fbinfo.height;
	self->bytesPerLine = getLineBytes();
	self->offset = (u_long)0;
	if (self->width == self->bytesPerLine) {
		switch (wsdisplay_fbinfo.depth) {
		case 15:
			/* FALLTHROUGH */
		case 16:
			self->length = self->width * self->height * 2;
			break;
		case 24:
			self->length = self->width * self->height * 3;
			break;
		case 32:
			self->length = self->width * self->height * 4;
			break;
		default:
			self->length = self->width * self->height;
			break;
		}
	} else
		self->length = self->bytesPerLine * self->height;
#else
	#error not implement
#endif

	/* Memory mapping */
	/* Same as getpagesize(3) */
	pageMask = sysconf(_SC_PAGESIZE) - 1;
	self->offset = self->offset & pageMask;
	self->length = (self->length + self->offset + pageMask) & ~pageMask;
	self->memory = mmap(NULL, self->length, PROT_READ | PROT_WRITE,
			    MAP_SHARED, self->fd, (off_t)0);
	if (self->memory == MAP_FAILED)
		errx(1, "Unable to memory map the framebuffer.");
	self->memory += self->offset;

	/* write-combining */
	if (writecombine.enable) {
#if defined (__linux__)
		if (writecombine.base != 0 &&
		    fb_fix_screeninfo.smem_start != writecombine.base)
			warnx("Invalid write-combining base detected. "
			      "Start of framebuffer memory is %#lx.",
			      fb_fix_screeninfo.smem_start);
		writecombine.base = fb_fix_screeninfo.smem_start;
#elif defined (__FreeBSD__)
		if (writecombine.base != 0 &&
		    video_adapter_info.va_window != writecombine.base)
			warnx("Invalid write-combining base detected. "
			      "Start of framebuffer memory is %#x.",
			      video_adapter_info.va_window);
		writecombine.base = video_adapter_info.va_window;
		if (writecombine.size != 0 &&
		    video_adapter_info.va_window_size != writecombine.size)
			warnx("Invalid write-combining size detected. "
			      "Framebuffer size is %#x.",
			      video_adapter_info.va_window_size);
		writecombine.size = video_adapter_info.va_window_size;
#endif
		if (writecombine.base != 0 && writecombine.size != 0)
			memctl_setWriteCombine(writecombine.base,
					       writecombine.size);
	}

	/* VGA */
#ifdef ENABLE_VGA16FB
#if defined (__linux__)
	if (fb_fix_screeninfo.type == FB_TYPE_VGA_PLANES &&
	    fb_fix_screeninfo.visual == FB_VISUAL_PSEUDOCOLOR &&
	    fb_var_screeninfo.bits_per_pixel == 4) {
		privilege_on();
		i = ioctl(STDIN_FILENO, KDENABIO, 0);
		privilege_off();
		if (i == -1)
			err(1, "ioctl(KDENABIO)");
		vgaSetRegisters(&vga16fbregs);
	}
#elif defined (__FreeBSD__)
	if (video_info.vi_mem_model == V_INFO_MM_PLANAR &&
	    video_info.vi_depth == 4) {
		privilege_on();
		i = ioctl(STDIN_FILENO, KDENABIO, 0);
		privilege_off();
		if (i == -1)
			err(1, "ioctl(KDENABIO)");
		vgaSetRegisters(&vga16fbregs);
	}
#endif
#endif

	/* Palette */
#if defined (__linux__)
	palette_initialize(&fb_var_screeninfo, &fb_fix_screeninfo);
#elif defined (__FreeBSD__)
	palette_initialize(&video_info, &video_adapter_info);
#elif defined (__NetBSD__) || defined (__OpenBSD__)
	palette_initialize(&wsdisplay_fbinfo, wsdisplay_type);
#else
	#error not implement
#endif
}

void framebuffer_reset(void)
{
	switch (gFramebuffer.accessor.bitsPerPixel) {
#ifdef ENABLE_VGA16FB
	case 4:
		vgaSetRegisters(&vga16fbregs);
		break;
#endif
	default:
		break;
	}
}

#if defined (__linux__)
static int getAccessorIndex(struct fb_var_screeninfo *fb_var_screeninfo,
			    struct fb_fix_screeninfo *fb_fix_screeninfo)
{
	int i;

	assert(initialized);
	assert(fb_var_screeninfo != NULL);
	assert(fb_fix_screeninfo != NULL);

	for (i = 0; accessors[i].bitsPerPixel != 0; i++) {
		if (accessors[i].type         == fb_fix_screeninfo->type &&
		    accessors[i].visual       == fb_fix_screeninfo->visual &&
		    accessors[i].bitsPerPixel == fb_var_screeninfo->bits_per_pixel) {
			return i;
		}
	}
	return -1;
}
#elif defined (__FreeBSD__)
static int getAccessorIndex(video_info_t *video_info)
{
	int i;

	assert(initialized);
	assert(video_info != NULL);

	for (i = 0; accessors[i].bitsPerPixel != 0; i++) {
		if (accessors[i].memoryModel  == video_info->vi_mem_model &&
		    accessors[i].bitsPerPixel == video_info->vi_depth)
			return i;
	}
	return -1;
}
#elif defined (__NetBSD__) || defined (__OpenBSD__)
static int getAccessorIndex(struct wsdisplay_fbinfo *wsdisplay_fbinfo)
{
	int i;

	assert(initialized);
	assert(wsdisplay_fbinfo != NULL);

	for (i = 0; accessors[i].bitsPerPixel != 0; i++) {
		if (accessors[i].bitsPerPixel == wsdisplay_fbinfo->depth)
			return i;
	}
	return -1;
}
#else
	#error not implement
#endif

