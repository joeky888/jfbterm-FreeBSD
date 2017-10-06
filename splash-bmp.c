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

#include <sys/types.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "splash-bmp.h"
#include "utilities.h"

#define BI_RGB (0)

typedef struct tagBITMAPFILEHEADER {
	uint16_t bfType;
	uint32_t bfSize;
	uint16_t bfReserved1;
	uint16_t bfReserved2;
	uint32_t bfOffBits;
} __attribute__ ((packed)) BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
	uint32_t biSize;
	int32_t  biWidth;
	int32_t  biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	int32_t  biXPelsPerMeter;
	int32_t  biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
} __attribute__ ((packed)) BITMAPINFOHEADER;

u_char *read_bmp_file(FILE *stream, u_int *width, u_int *height)
{
	BITMAPFILEHEADER bf;
	BITMAPINFOHEADER bi;
	size_t bitmapSize, bytesPerLine;
	int32_t x, y;
	u_char b, *bitmap, *data;
	bool topDownDIB;

	assert(stream != NULL);
	assert(width != NULL);
	assert(height != NULL);

	*width  = 0;
	*height = 0;
	/* read bitmap file header */
	if (fread(&bf, sizeof(bf), 1, stream) != 1) {
		warnx("Bitmap file read canceled: %s",
		      "Could not read bitmap file header.");
		return NULL;
	}
	bf.bfType      = UINT16_SWAP_ON_BE(bf.bfType);
	bf.bfSize      = UINT32_SWAP_ON_BE(bf.bfSize);
	bf.bfReserved1 = UINT16_SWAP_ON_BE(bf.bfReserved1);
	bf.bfReserved2 = UINT16_SWAP_ON_BE(bf.bfReserved2);
	bf.bfOffBits   = UINT32_SWAP_ON_BE(bf.bfOffBits);
	if (bf.bfType != ('B' | ('M' << 8))) {
		warnx("Bitmap file format error: %s",
		      "Bad signature.");
		return NULL;
	}
	/* read bitmap info header */
	if (fread(&bi, sizeof(bi), 1, stream) != 1) {
		warnx("Bitmap file read canceled: %s",
		      "Could not read bitmap infomation header.");
		return NULL;
	}
	bi.biSize          = UINT32_SWAP_ON_BE(bi.biSize);
	bi.biWidth         =  INT32_SWAP_ON_BE(bi.biWidth);
	bi.biHeight        =  INT32_SWAP_ON_BE(bi.biHeight);
	bi.biPlanes        = UINT16_SWAP_ON_BE(bi.biPlanes);
	bi.biBitCount      = UINT16_SWAP_ON_BE(bi.biBitCount);
	bi.biCompression   = UINT32_SWAP_ON_BE(bi.biCompression);
	bi.biSizeImage     = UINT32_SWAP_ON_BE(bi.biSizeImage);
	bi.biXPelsPerMeter =  INT32_SWAP_ON_BE(bi.biXPelsPerMeter);
	bi.biYPelsPerMeter =  INT32_SWAP_ON_BE(bi.biYPelsPerMeter);
	bi.biClrUsed       = UINT32_SWAP_ON_BE(bi.biClrUsed);
	bi.biClrImportant  = UINT32_SWAP_ON_BE(bi.biClrImportant);
	if (bi.biCompression != BI_RGB) {
		warnx("Bitmap file format error: %s",
		      "Compressed bitmap not supported.");
		return NULL;
	}
	if (bi.biBitCount != 24) {
		warnx("Bitmap file format error: %dbpp not supported.",
		      bi.biBitCount);
		return NULL;
	}
	topDownDIB = false;
	if (bi.biHeight < 0) {
		topDownDIB = true;
		bi.biHeight *= -1;
	}
	if (bi.biWidth < 0) {
		warnx("Bitmap file format error: %s",
		      "Invalid format.");
		return NULL;
	}
	if (bi.biHeight == 0 || bi.biWidth == 0)
		return NULL;
	/* 32 bit boundary for each line */
	if ((bi.biWidth % 4) == 0)
		bytesPerLine = bi.biWidth * 3;
	else
		bytesPerLine = bi.biWidth * 3 + (4 - (bi.biWidth % 4));
	bitmapSize = bytesPerLine * bi.biHeight;
	/* read bitmap */
	if (fseek(stream, bf.bfOffBits, SEEK_SET) != 0) {
		warnx("Bitmap file read canceled: %s",
		      "Seek error.");
		return NULL;
	}
	bitmap = malloc(bitmapSize);
	if (bitmap == NULL) {
		warnx("Bitmap file read canceled: %s", strerror(errno));
		free(bitmap);
		return NULL;
	}
	if (fread(bitmap, 1, bitmapSize, stream) != bitmapSize) {
		warnx("Bitmap file read canceled: %s",
		      "Could not read bitmap.");
		free(bitmap);
		return NULL;
	}
	data = malloc(bi.biWidth * 3 * bi.biHeight);
	if (data == NULL) {
		warnx("Bitmap file read canceled: %s", strerror(errno));
		free(bitmap);
		return NULL;
	}
	/* bottom up DIB to top down DIB */
	if (topDownDIB)
		for (y = 0; y < bi.biHeight; y++)
			memcpy(data + y * bi.biWidth * 3,
			       bitmap + y * bytesPerLine,
			       bi.biWidth * 3);
	else
		for (y = 0; y < bi.biHeight; y++)
			memcpy(data + y * bi.biWidth * 3,
			       bitmap + ((bi.biHeight - 1) - y) * bytesPerLine,
			       bi.biWidth * 3);
	/* BGR to RGB */
	for (y = 0; y < bi.biHeight; y++) {
		for (x = 0; x < bi.biWidth * 3; x += 3) {
			b = data[y * bi.biWidth * 3 + x];
			data[y * bi.biWidth * 3 + x] =
					data[y * bi.biWidth * 3 + x + 2];
			data[y * bi.biWidth * 3 + x + 2] = b;
		}
	}
	free(bitmap);
	*width  = (u_int)bi.biWidth;
	*height = (u_int)bi.biHeight;
	return data;
}

#endif /* ENABLE_SPLASH_SCREEN */

