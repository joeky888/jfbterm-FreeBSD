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
#ifdef WITH_LIBPNG

#include <sys/types.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "splash-png.h"

u_char *read_png_file(FILE *stream, u_int *width, u_int *height)
{
	png_byte sig[8];
	png_structp png_ptr;
	png_infop info_ptr, end_info_ptr;
	png_bytep *row_pointers;
	png_uint_32 i, j, image_width, image_height;
	png_byte color_type, bit_depth;
	u_char *data;

	assert(stream != NULL);
	assert(width != NULL);
	assert(height != NULL);

	*width  = 0;
	*height = 0;
	if (fread(sig, 1, sizeof(sig), stream) != sizeof(sig)) {
		warnx("PNG file read canceled: %s",
		      "Could not read signature.");
		return NULL;
	}
	if (png_sig_cmp(sig, 0, sizeof(sig)) != 0) {
		warnx("PNG file format error: %s",
		      "Bad signature.");
		return NULL;
	}
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
					 (png_voidp)NULL, NULL, NULL);
	if (png_ptr == NULL) {
		warnx("PNG file read canceled: %s",
		      "png_create_read_struct() returned NULL.");
		return NULL;
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		warnx("PNG file read canceled: %s",
		      "png_create_info_struct() returned NULL.");
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return NULL;
	}
	end_info_ptr = png_create_info_struct(png_ptr);
	if (end_info_ptr == NULL) {
		warnx("PNG file read canceled: %s",
		      "png_create_info_struct() returned NULL.");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}
	png_init_io(png_ptr, stream);
	png_set_sig_bytes(png_ptr, sizeof(sig));
	png_read_info(png_ptr, info_ptr);
	image_width  = png_get_image_width(png_ptr, info_ptr);
	image_height = png_get_image_height(png_ptr, info_ptr);
	bit_depth    = png_get_bit_depth(png_ptr, info_ptr);
	color_type   = png_get_color_type(png_ptr, info_ptr);
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_gray_1_2_4_to_8(png_ptr);
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);
	if (color_type & PNG_COLOR_MASK_ALPHA)
		png_set_strip_alpha(png_ptr);
	if (bit_depth < 8)
		png_set_packing(png_ptr);
	if (color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);
	row_pointers = malloc(image_height * sizeof(png_bytep));
	if (row_pointers == NULL) {
		warnx("PNG file read canceled: %s", strerror(errno));
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
		return NULL;
	}
	for (i = 0; i < image_height; i++) {
		row_pointers[i] = malloc(image_width * 3);
		if (row_pointers[i] == NULL) {
			warnx("PNG file read canceled: %s", strerror(errno));
			png_destroy_read_struct(&png_ptr, &info_ptr,
						&end_info_ptr);
			for (j = 0; j < i; j++)
				free(row_pointers[j]);
			free(row_pointers);
			return NULL;
		}
	}
	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, end_info_ptr);
	data = malloc(image_width * 3 * image_height);
	if (data == NULL) {
		warnx("PNG file read canceled: %s", strerror(errno));
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
		for (i = 0; i < image_height; i++)
			free(row_pointers[i]);
		free(row_pointers);
		return NULL;
	}
	for (i = 0; i < image_height; i++)
		memcpy(data + i * image_width * 3, row_pointers[i],
		       image_width * 3);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
	for (i = 0; i < image_height; i++)
		free(row_pointers[i]);
	free(row_pointers);
	*width  = (u_int)image_width;
	*height = (u_int)image_height;
	return data;
}

#endif  /* WITH_LIBPNG */
#endif  /* ENABLE_SPLASH_SCREEN */

