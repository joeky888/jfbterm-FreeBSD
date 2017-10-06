/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
 *
 * JFBTERM Unofficial eXtension Patch
 * Copyright (C) 2006 Young Chol Song <nskystars at yahoo dot com>
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

#ifdef ENABLE_SPLASH_SCREEN

#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framebuffer.h"
#include "palette.h"
#include "splash-bmp.h"
#include "splash-png.h"
#include "splash.h"
#include "utilities.h"

#define SPLASH_GET(bitmap, width, x, y, c) \
	(*((bitmap) + (width) * 3 * (y) + (x) * 3 + (c)))

#define DEFAULT_SPLASH_TINT (5)

TSplash gSplash;

static TSplash *const self = &gSplash;
static bool initialized;

static struct {
	char *image;
	int tint;
} splash = {
	NULL,
	DEFAULT_SPLASH_TINT
};

static void finalize(void);
static void finalizer(void);
static void configImage(const char *config);
static void configTint(const char *config);
static void loadImage(void);
static void unloadImage(void);
static void applyTint(void);
static void resizeImage(int width, int height);
static void swapRGBOrder(void);
static void convert24bppTo16bpp(void);
static void convert24bppTo32bpp(void);

void splash_initialize(void)
{
	assert(!initialized);

	atexit(finalizer);
	initialized = true;
	self->bitmap = NULL;
	self->width  = 0;
	self->height = 0;
	self->enable = false;
}

static void finalize(void)
{
	assert(initialized);

	unloadImage();
	if (splash.image != NULL) {
		free(splash.image);
		splash.image = NULL;
	}
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

static void configImage(const char *config)
{
	assert(initialized);

	if (config != NULL) {
		if (splash.image != NULL)
			free(splash.image);
		splash.image = strdup(config);
		if (splash.image == NULL)
			err(1, "strdup()");
	}
}

static void configTint(const char *config)
{
	int tint;

	assert(initialized);

	if (config != NULL) {
		tint = atoi(config);
		if (tint >= 0 && tint <= 10)
			splash.tint = tint;
		else
			warnx("Invalid splash tint: %s", config);
	}
}

void splash_configure(TCaps *caps)
{
	const char *config;

	assert(initialized);
	assert(caps != NULL);

	config = caps_findFirst(caps, "splash.image");
	configImage(config);
	config = caps_findFirst(caps, "splash.tint");
	configTint(config);
}

static void loadImage(void)
{
	FILE *stream;

	assert(initialized);

	unloadImage();
	if (splash.image == NULL)
		return;
	if (strlen(splash.image) > 4 &&
	    strrcmp(splash.image, ".bmp") == 0) {
		if ((stream = fopen(splash.image, "r")) != NULL) {
			self->bitmap = read_bmp_file(stream,
						     &(self->width),
						     &(self->height));
			fclose(stream);
		} else
			warnx("Could not read bitmap file.");
	}
#ifdef WITH_LIBPNG
	else if (strlen(splash.image) > 4 &&
		 strrcmp(splash.image, ".png") == 0) {
		if ((stream = fopen(splash.image, "r")) != NULL) {
			self->bitmap = read_png_file(stream,
						     &(self->width),
						     &(self->height));
			fclose(stream);
		} else
			warnx("Could not read PNG file.");
	}
#endif
	if (self->bitmap != NULL)
		self->enable = true;
}

static void unloadImage(void)
{
	assert(initialized);

	self->enable = false;
	if (self->bitmap != NULL) {
		free(self->bitmap);
		self->bitmap = NULL;
	}
}

static void applyTint(void)
{
	u_int x, y;
	float t;

	assert(initialized);

	if (!self->enable)
		return;
	if (self->bitmap == NULL)
		return;
	t = ((10 - splash.tint) / 10.0f);
	for (y = 0; y < self->height; y++) {
		for (x = 0; x < self->width; x++) {
			SPLASH_GET(self->bitmap, self->width, x, y, 0) *= t;
			SPLASH_GET(self->bitmap, self->width, x, y, 1) *= t;
			SPLASH_GET(self->bitmap, self->width, x, y, 2) *= t;
		}
	}
}

static void resizeImage(int width, int height)
{
	u_int x, y, w, h;
	u_char *bitmap;

	assert(initialized);
	assert(width > 0);
	assert(height > 0);

	if (!self->enable)
		return;
	if (self->bitmap == NULL)
		return;
	bitmap = malloc(height * width * 3);
	if (bitmap == NULL)
		err(1, "malloc()");
	for (y = 0; y < height; y++) {
		h = y * self->height / height;
		for (x = 0; x < width; x++) {
			w = x * self->width / width;
			SPLASH_GET(bitmap, width, x, y, 0) =
				SPLASH_GET(self->bitmap, self->width, w, h, 0);
			SPLASH_GET(bitmap, width, x, y, 1) =
				SPLASH_GET(self->bitmap, self->width, w, h, 1);
			SPLASH_GET(bitmap, width, x, y, 2) =
				SPLASH_GET(self->bitmap, self->width, w, h, 2);
		}
	}
	free(self->bitmap);
	self->bitmap = bitmap;
	self->width  = width;
	self->height = height;
}

static void swapRGBOrder(void)
{
	u_char r, g, b;
	u_int x, y;
	int rIndex, gIndex, bIndex;

	assert(initialized);

	if (!self->enable)
		return;
	if (self->bitmap == NULL)
		return;
	rIndex = palette_getRIndex();
	gIndex = palette_getGIndex();
	bIndex = palette_getBIndex();
	if (rIndex == 0 && gIndex == 1 && bIndex == 2)
		return;
	for (y = 0; y < self->height; y++) {
		for (x = 0; x < self->width * 3; x += 3) {
			r = self->bitmap[y * self->width * 3 + x + 0];
			g = self->bitmap[y * self->width * 3 + x + 1];
			b = self->bitmap[y * self->width * 3 + x + 2];
			self->bitmap[y * self->width * 3 + x + rIndex] = r;
			self->bitmap[y * self->width * 3 + x + gIndex] = g;
			self->bitmap[y * self->width * 3 + x + bIndex] = b;
		}
	}
}

static void convert24bppTo16bpp(void)
{
	u_char r, g, b;
	u_int x, y;
	int rIndex, gIndex, bIndex;
	int rLength, gLength, bLength;
	int rOffset, gOffset, bOffset;
	uint16_t *p;
	u_char *bitmap;

	assert(initialized);

	rIndex  = palette_getRIndex();
	gIndex  = palette_getGIndex();
	bIndex  = palette_getBIndex();
	rLength = palette_getRLength();
	gLength = palette_getGLength();
	bLength = palette_getBLength();
	rOffset = palette_getROffset();
	gOffset = palette_getGOffset();
	bOffset = palette_getBOffset();
	p = malloc(self->height * self->width * sizeof(uint16_t));
	if (p == NULL)
		err(1, "malloc()");
	bitmap = (u_char *)p;
	for (y = 0; y < self->height; y++) {
		for (x = 0; x < self->width * 3; x += 3) {
			r = self->bitmap[y * self->width * 3 + x + rIndex];
			g = self->bitmap[y * self->width * 3 + x + gIndex];
			b = self->bitmap[y * self->width * 3 + x + bIndex];
			*p = (((uint16_t)(r) >> (8 - rLength)) << rOffset) |
			     (((uint16_t)(g) >> (8 - gLength)) << gOffset) |
			     (((uint16_t)(b) >> (8 - bLength)) << bOffset);
			p++;
		}
	}
	free(self->bitmap);
	self->bitmap = bitmap;
}

static void convert24bppTo32bpp(void)
{
	u_char r, g, b;
	u_int x, y;
	int rIndex, gIndex, bIndex;
	int rLength, gLength, bLength;
	int rOffset, gOffset, bOffset;
	uint32_t *p;
	u_char *bitmap;

	assert(initialized);

	rIndex  = palette_getRIndex();
	gIndex  = palette_getGIndex();
	bIndex  = palette_getBIndex();
	rLength = palette_getRLength();
	gLength = palette_getGLength();
	bLength = palette_getBLength();
	rOffset = palette_getROffset();
	gOffset = palette_getGOffset();
	bOffset = palette_getBOffset();
	p = malloc(self->height * self->width * sizeof(uint32_t));
	if (p == NULL)
		err(1, "malloc()");
	bitmap = (u_char *)p;
	for (y = 0; y < self->height; y++) {
		for (x = 0; x < self->width * 3; x += 3) {
			r = self->bitmap[y * self->width * 3 + x + rIndex];
			g = self->bitmap[y * self->width * 3 + x + gIndex];
			b = self->bitmap[y * self->width * 3 + x + bIndex];
			*p = (((uint32_t)(r) >> (8 - rLength)) << rOffset) |
			     (((uint32_t)(g) >> (8 - gLength)) << gOffset) |
			     (((uint32_t)(b) >> (8 - bLength)) << bOffset);
			p++;
		}
	}
	free(self->bitmap);
	self->bitmap = bitmap;
}

void splash_load(void)
{
	assert(initialized);

	if (splash.image == NULL)
		return;
	if (gFramebuffer.accessor.bitsPerPixel != 15 &&
	    gFramebuffer.accessor.bitsPerPixel != 16 &&
	    gFramebuffer.accessor.bitsPerPixel != 24 &&
	    gFramebuffer.accessor.bitsPerPixel != 32) {
		warnx("Splash is not supported on %dbpp.",
		      gFramebuffer.accessor.bitsPerPixel);
		return;
	}
	loadImage();
	swapRGBOrder();
	applyTint();
	resizeImage(gFramebuffer.width, gFramebuffer.height);
	switch (gFramebuffer.accessor.bitsPerPixel) {
	case 15:
		/* FALLTHROUGH */
	case 16:
		convert24bppTo16bpp();
		break;
	case 24:
		break;
	case 32:
		convert24bppTo32bpp();
		break;
	default:
		break;
	}
}

#endif /* ENABLE_SPLASH_SCREEN */

