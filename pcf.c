/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
 *
 * JFBTERM/ME - J framebuffer terminal/Multilingual Enhancement -
 * Copyright (C) 2003 Fumitoshi UKAI <ukai@debian.or.jp>
 * Copyright (C) 1999 Noritoshi MASUICHI <nmasu@ma3.justnet.ne.jp>
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
#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pcf.h"
#include "utilities.h"

#define PCF_VERSION            (('p' << 24) | ('c' << 16) | ('f' << 8) | 1)

/* Format id */
#define PCF_DEFAULT_FORMAT     (0)
#define PCF_INKBOUNDS          (2)
#define PCF_ACCEL_W_INKBOUNDS  (1)
#define PCF_COMPRESSED_METRICS (1)

/* Section type */
#define PCF_PROPERTIES         (1 << 0)
#define PCF_ACCELERATORS       (1 << 1)
#define PCF_METRICS            (1 << 2)
#define PCF_BITMAPS            (1 << 3)
#define PCF_INK_METRICS        (1 << 4)
#define PCF_BDF_ENCODINGS      (1 << 5)
#define PCF_SWIDTHS            (1 << 6)
#define PCF_GLYPH_NAMES        (1 << 7)
#define PCF_BDF_ACCELERATORS   (1 << 8)

#define PCF_PROP_SIZE          (4 + 1 + 4)
#define NO_SUCH_CHAR           (-1)

static void unexpectedEOF(void);
static uint8_t readUINT8(FILE *stream, size_t *position);
static uint16_t readUINT16LE(FILE *stream, size_t *position);
static uint16_t readUINT16BE(FILE *stream, size_t *position);
static uint16_t readUINT16(FILE *stream, TPcfFormat *format, size_t *position);
static uint32_t readUINT32LE(FILE *stream, size_t *position);
static uint32_t readUINT32BE(FILE *stream, size_t *position);
static uint32_t readUINT32(FILE *stream, TPcfFormat *format, size_t *position);
static void pcfFormat_load(TPcfFormat *p, FILE *stream, size_t *position);
static void pcfTable_load(TPcfTable *p, FILE *stream, size_t *position);
#ifdef PCF_DEBUG
static void pcfTable_debug(TPcfTable *p);
#endif
static void pcfProp_load(TPcfProp *p, FILE *stream, TPcfFormat *format,
			 size_t *position);
static void pcfProperties_initialize(TPcfProperties *p);
static void pcfProperties_finalize(TPcfProperties *p);
static void pcfProperties_load(TPcfProperties *p, FILE *stream,
			       size_t *position);
#ifdef PCF_DEBUG
static void pcfProperties_debug(TPcfProperties *p);
#endif
static void pcfMetric_load(TPcfMetric *p, FILE *stream, TPcfFormat *format,
			   size_t *position);
static void pcfMetric_load_compressed(TPcfMetric *p, FILE *stream,
				      size_t *position);
#ifdef PCF_DEBUG
static void pcfMetric_debug(TPcfMetric *p);
#endif
static void pcfMetrics_initialize(TPcfMetrics *p);
static void pcfMetrics_finalize(TPcfMetrics *p);
static void pcfMetrics_load(TPcfMetrics *p, FILE *stream, size_t *position);
static void pcfAccelerators_load(TPcfAccelerators *p, FILE *stream,
				 size_t *position);
#ifdef PCF_DEBUG
static void pcfAccelerators_debug(TPcfAccelerators *p);
#endif
static void pcfBitmaps_initialize(TPcfBitmaps *p);
static void pcfBitmaps_finalize(TPcfBitmaps *p);
static void pcfBitmaps_swap(TPcfBitmaps *p, TPcfFormat *format);
static void pcfBitmaps_load(TPcfBitmaps *p, FILE *stream, size_t *position);
static void pcfBdfEncodings_initialize(TPcfBdfEncodings *p);
static void pcfBdfEncodings_finalize(TPcfBdfEncodings *p);
static void pcfBdfEncodings_load(TPcfBdfEncodings *p, FILE *stream,
				 size_t *position);
#ifdef PCF_DEBUG
static void pcfBdfEncodings_debug(TPcfBdfEncodings *p);
#endif
static TPcfTable *searchSection(TPcf *p, uint32_t type);
static size_t seekSection(FILE *stream, size_t current, size_t offset);

static void unexpectedEOF(void)
{
	errx(1, "(FONT): Unexpected EOF.");
}

static uint8_t readUINT8(FILE *stream, size_t *position)
{
	uint8_t value;

	assert(stream != NULL);
	assert(position != NULL);

	if (fread(&value, sizeof(uint8_t), 1, stream) != 1)
		unexpectedEOF();
	*position += sizeof(uint8_t);
	return value;
}

static uint16_t readUINT16LE(FILE *stream, size_t *position)
{
	uint16_t value;

	assert(stream != NULL);
	assert(position != NULL);

	if (fread(&value, sizeof(uint16_t), 1, stream) != 1)
		unexpectedEOF();
	*position += sizeof(uint16_t);
	return UINT16_SWAP_ON_BE(value);
}

static uint16_t readUINT16BE(FILE *stream, size_t *position)
{
	uint16_t value;

	assert(stream != NULL);
	assert(position != NULL);

	if (fread(&value, sizeof(uint16_t), 1, stream) != 1)
		unexpectedEOF();
	*position += sizeof(uint16_t);
	return UINT16_SWAP_ON_LE(value);
}

static uint16_t readUINT16(FILE *stream, TPcfFormat *format, size_t *position)
{
	assert(stream != NULL);
	assert(format != NULL);
	assert(position != NULL);

	return format->bit != 0 ? readUINT16BE(stream, position) :
				  readUINT16LE(stream, position);
}

static uint32_t readUINT32LE(FILE *stream, size_t *position)
{
	uint32_t value;

	assert(stream != NULL);
	assert(position != NULL);

	if (fread(&value, sizeof(uint32_t), 1, stream) != 1)
		unexpectedEOF();
	*position += sizeof(uint32_t);
	return UINT32_SWAP_ON_BE(value);
}

static uint32_t readUINT32BE(FILE *stream, size_t *position)
{
	uint32_t value;

	assert(stream != NULL);
	assert(position != NULL);

	if (fread(&value, sizeof(uint32_t), 1, stream) != 1)
		unexpectedEOF();
	*position += sizeof(uint32_t);
	return UINT32_SWAP_ON_LE(value);
}

static uint32_t readUINT32(FILE *stream, TPcfFormat *format, size_t *position)
{
	assert(stream != NULL);
	assert(format != NULL);
	assert(position != NULL);

	return format->bit != 0 ? readUINT32BE(stream, position) :
				  readUINT32LE(stream, position);
}

static void pcfFormat_load(TPcfFormat *p, FILE *stream, size_t *position)
{
	uint32_t format;

	assert(p != NULL);
	assert(stream != NULL);
	assert(position != NULL);

	format = readUINT32LE(stream, position);
	p->id    = (format >> 8) & 0xfff;
	p->scan  = 1 << ((format >> 4) & 0x3);
	p->bit   = (format >> 3) & 1;
	p->byte  = (format >> 2) & 1;
	p->glyph = format & 0x3;
#if PCF_DEBUG
	fprintf(stderr, "[[FORMAT: %x = ", format);
	switch (p->id) {
	case PCF_DEFAULT_FORMAT:
		fprintf(stderr, "PCF_DEFAULT_FORMAT ");
		break;
	case PCF_INKBOUNDS:
		fprintf(stderr, "PCF_INKBOUNDS ");
		break;
	case PCF_ACCEL_W_INKBOUNDS: /* or PCF_COMPRESSED_METRICS */
		fprintf(stderr, "PCF_ACCEL_W_INKBOUNDS|PCF_COMPRESSED_METRICS ");
		break;
	default:
		fprintf(stderr, "???");
		break;
	}
	fprintf(stderr, "scan: %d bytes ", p->scan);
	if (p->bit != 0)
		fprintf(stderr, "MSB ");
	else
		fprintf(stderr, "LSB ");
	if (p->byte != 0)
		fprintf(stderr, "BE ");
	else
		fprintf(stderr, "LE ");
	fprintf(stderr, "glyph: %d (%d bytes)", p->glyph, 1 << p->glyph);
	fprintf(stderr, "]]\n");
#endif
}

static void pcfTable_load(TPcfTable *p, FILE *stream, size_t *position)
{
	assert(p != NULL);
	assert(stream != NULL);
	assert(position != NULL);

	p->type   = readUINT32LE(stream, position);
	pcfFormat_load(&(p->format), stream, position);
	p->size   = readUINT32LE(stream, position);
	p->offset = readUINT32LE(stream, position);
}

#ifdef PCF_DEBUG
static void pcfTable_debug(TPcfTable *p)
{
	assert(p != NULL);

	fprintf(stderr, "type = %u: ", p->type);
	switch (p->type) {
	case PCF_PROPERTIES:
		fprintf(stderr, "PCF_PROPERTIES, ");
		break;
	case PCF_ACCELERATORS:
		fprintf(stderr, "PCF_ACCELERATORS, ");
		break;
	case PCF_METRICS:
		fprintf(stderr, "PCF_METRICS, ");
		break;
	case PCF_BITMAPS:
		fprintf(stderr, "PCF_BITMAPS, ");
		break;
	case PCF_INK_METRICS:
		fprintf(stderr, "PCF_INK_METRICS, ");
		break;
	case PCF_BDF_ENCODINGS:
		fprintf(stderr, "PCF_BDF_ENCODINGS, ");
		break;
	case PCF_SWIDTHS:
		fprintf(stderr, "PCF_SWIDTHS, ");
		break;
	case PCF_GLYPH_NAMES:
		fprintf(stderr, "PCF_GLYPH_NAMES, ");
		break;
	case PCF_BDF_ACCELERATORS:
		fprintf(stderr, "PCF_BDF_ACCELERATORS, ");
		break;
	default:
		fprintf(stderr, "???");
		break;
	}
	fprintf(stderr, "size = %u, offset = %u", p->size, p->offset);
}
#endif

static void pcfProp_load(TPcfProp *p, FILE *stream, TPcfFormat *format,
			 size_t *position)
{
	assert(p != NULL);
	assert(stream != NULL);
	assert(format != NULL);
	assert(position != NULL);

	p->name         = readUINT32(stream, format, position);
	p->isStringProp = readUINT8(stream, position);
	p->value        = readUINT32(stream, format, position);
}

static void pcfProperties_initialize(TPcfProperties *p)
{
	assert(p != NULL);

	p->nProps = 0;
	p->props = NULL;
	p->stringSize = 0;
	p->string = NULL;
}

static void pcfProperties_finalize(TPcfProperties *p)
{
	assert(p != NULL);

	p->nProps = 0;
	if (p->props != NULL) {
		free(p->props);
		p->props = NULL;
	}
	p->stringSize = 0;
	if (p->string != NULL) {
		free(p->string);
		p->string = NULL;
	}
}

static void pcfProperties_load(TPcfProperties *p, FILE *stream,
			       size_t *position)
{
	uint32_t i, dummy;

	assert(p != NULL);
	assert(stream != NULL);
	assert(position != NULL);

	pcfFormat_load(&(p->format), stream, position);
	if (p->format.id != PCF_DEFAULT_FORMAT)
		errx(1, "(FONT): Bad format id = %d.", p->format.id);
	p->nProps = readUINT32(stream, &(p->format), position);
	p->props = malloc(sizeof(TPcfProp) * p->nProps);
	if (p->props == NULL)
		err(1, "malloc()");
	for (i = 0; i < p->nProps; i++)
		pcfProp_load(&(p->props[i]), stream, &(p->format), position);
	dummy = 3 - (((PCF_PROP_SIZE * p->nProps) + 3) % 4);
	for (i = 0; i < dummy; i++)
		readUINT8(stream, position);
	p->stringSize = readUINT32(stream, &(p->format), position);
	p->string = malloc(sizeof(char) * p->stringSize);
	if (p->string == NULL)
		err(1, "malloc()");
	if (fread(p->string, p->stringSize, 1, stream) != 1)
		unexpectedEOF();
	*position += sizeof(char) * p->stringSize;
}

#ifdef PCF_DEBUG
static void pcfProperties_debug(TPcfProperties *p)
{
	uint32_t i;

	assert(p != NULL);

	for (i = 0; i < p->nProps; i++) {
		if (p->props[i].isStringProp != 0) {
			fprintf(stderr, "%s = \"%s\"\n",
				p->string + (p->props[i].name),
				p->string + (p->props[i].value));
		} else {
			fprintf(stderr, "%s = %d\n",
				p->string + (p->props[i].name),
				p->props[i].value);
		}
	}
}
#endif

static void pcfMetric_load(TPcfMetric *p, FILE *stream, TPcfFormat *format,
			   size_t *position)
{
	assert(p != NULL);
	assert(stream != NULL);
	assert(format != NULL);
	assert(position != NULL);

	p->leftSideBearing  = readUINT16(stream, format, position);
	p->rightSideBearing = readUINT16(stream, format, position);
	p->characterWidth   = readUINT16(stream, format, position);
	p->ascent           = readUINT16(stream, format, position);
	p->descent          = readUINT16(stream, format, position);
	p->attributes       = readUINT16(stream, format, position);
}

static void pcfMetric_load_compressed(TPcfMetric *p, FILE *stream,
				      size_t *position)
{
	assert(p != NULL);
	assert(stream != NULL);
	assert(position != NULL);

	p->leftSideBearing  = readUINT8(stream, position) - 0x80;
	p->rightSideBearing = readUINT8(stream, position) - 0x80;
	p->characterWidth   = readUINT8(stream, position) - 0x80;
	p->ascent           = readUINT8(stream, position) - 0x80;
	p->descent          = readUINT8(stream, position) - 0x80;
	p->attributes       = 0;
}

#ifdef PCF_DEBUG
static void pcfMetric_debug(TPcfMetric *p)
{
	assert(p != NULL);

	fprintf(stderr, "[METRIC:%d<>%d(%d):%d^v%d:%d]",
		p->leftSideBearing, p->rightSideBearing, p->characterWidth,
		p->ascent, p->descent, p->attributes);
}
#endif

static void pcfMetrics_initialize(TPcfMetrics *p)
{
	assert(p != NULL);

	p->nMetrics = 0;
	p->metrics  = NULL;
}

static void pcfMetrics_finalize(TPcfMetrics *p)
{
	assert(p != NULL);

	p->nMetrics = 0;
	if (p->metrics != NULL) {
		free(p->metrics);
		p->metrics = NULL;
	}
}

static void pcfMetrics_load(TPcfMetrics *p, FILE *stream, size_t *position)
{
	uint32_t i;

	assert(p != NULL);
	assert(stream != NULL);
	assert(position != NULL);

	pcfFormat_load(&(p->format), stream, position);
	if (p->format.id == PCF_DEFAULT_FORMAT) {
		p->nMetrics = readUINT32(stream, &(p->format), position);
#ifdef PCF_DEBUG
		fprintf(stderr, "nMetrics: %d\n", p->nMetrics);
#endif
		p->metrics = malloc(sizeof(TPcfMetric) * p->nMetrics);
		if (p->metrics == NULL)
			err(1, "malloc()");
		for (i = 0; i < p->nMetrics; i++)
			pcfMetric_load(&(p->metrics[i]), stream,
				       &(p->format), position);
	} else if (p->format.id == PCF_COMPRESSED_METRICS) {
		p->nMetrics = readUINT16(stream, &(p->format), position);
#ifdef PCF_DEBUG
		fprintf(stderr, "nMetrics (compressed): %d\n", p->nMetrics);
#endif
		p->metrics = malloc(sizeof(TPcfMetric) * p->nMetrics);
		if (p->metrics == NULL)
			err(1, "malloc()");
		for (i = 0; i < p->nMetrics; i++)
			pcfMetric_load_compressed(&(p->metrics[i]), stream,
						  position);
	} else
		errx(1, "(FONT): Bad format id = %d.", p->format.id);
}

static void pcfAccelerators_load(TPcfAccelerators *p, FILE *stream,
				 size_t *position)
{
	uint8_t dummy;

	assert(p != NULL);
	assert(stream != NULL);
	assert(position != NULL);

	pcfFormat_load(&(p->format), stream, position);
	if (p->format.id != PCF_DEFAULT_FORMAT &&
	    p->format.id != PCF_ACCEL_W_INKBOUNDS)
		errx(1, "(FONT): Bad format id = %d.", p->format.id);
	p->noOverlap       = readUINT8(stream, position);
	p->constantMetrics = readUINT8(stream, position);
	p->terminalFont    = readUINT8(stream, position);
	p->constantWidth   = readUINT8(stream, position);
	p->inkInside       = readUINT8(stream, position);
	p->inkMetrics      = readUINT8(stream, position);
	p->drawDirection   = readUINT8(stream, position);
	dummy              = readUINT8(stream, position);
	p->fontAscent      = readUINT32(stream, &(p->format), position);
	p->fontDescent     = readUINT32(stream, &(p->format), position);
	p->maxOverlap      = readUINT32(stream, &(p->format), position);
	pcfMetric_load(&(p->minBounds), stream, &(p->format), position);
	pcfMetric_load(&(p->maxBounds), stream, &(p->format), position);
	if (p->format.id == PCF_ACCEL_W_INKBOUNDS) {
		pcfMetric_load(&(p->ink_minBounds), stream,
			       &(p->format), position);
		pcfMetric_load(&(p->ink_maxBounds), stream,
			       &(p->format), position);
	}
}

#ifdef PCF_DEBUG
static void pcfAccelerators_debug(TPcfAccelerators *p)
{
	assert(p != NULL);

	fprintf(stderr, "[ACCELERATORS:");
	pcfMetric_debug(&(p->minBounds));
	fprintf(stderr, "]\n");
}
#endif

static void pcfBitmaps_initialize(TPcfBitmaps *p)
{
	assert(p != NULL);

	p->nBitmaps = 0;
	p->bitmapOffsets = NULL;
	p->bitmaps = NULL;
}

static void pcfBitmaps_finalize(TPcfBitmaps *p)
{
	assert(p != NULL);

	p->nBitmaps = 0;
	if (p->bitmapOffsets != NULL) {
		free(p->bitmapOffsets);
		p->bitmapOffsets = NULL;
	}
	if (p->bitmaps != NULL) {
		free(p->bitmaps);
		p->bitmaps = NULL;
	}
}

static void pcfBitmaps_swap(TPcfBitmaps *p, TPcfFormat *format)
{
	uint32_t i, bitmapSize;
	u_char *bitmap, c, d;

	assert(p != NULL);
	assert(format != NULL);

	bitmapSize = p->bitmapSizes[format->glyph];
	bitmap = p->bitmaps;
	if (format->bit == 0) { /* LSB <==> MSB */
		for (i = 0; i < bitmapSize; i++) {
			c = *bitmap;
			d  = (c & 0x01) ? 0x80 : 0x00;
			d |= (c & 0x02) ? 0x40 : 0x00;
			d |= (c & 0x04) ? 0x20 : 0x00;
			d |= (c & 0x08) ? 0x10 : 0x00;
			d |= (c & 0x10) ? 0x08 : 0x00;
			d |= (c & 0x20) ? 0x04 : 0x00;
			d |= (c & 0x40) ? 0x02 : 0x00;
			d |= (c & 0x80) ? 0x01 : 0x00;
			*bitmap++ = d;
		}
	}
}

static void pcfBitmaps_load(TPcfBitmaps *p, FILE *stream, size_t *position)
{
	uint32_t i, bitmapSize;

	assert(p != NULL);
	assert(stream != NULL);
	assert(position != NULL);

	pcfFormat_load(&(p->format), stream, position);
	if (p->format.id != PCF_DEFAULT_FORMAT)
		errx(1, "(FONT): Bad format id = %d.", p->format.id);
	p->nBitmaps = readUINT32(stream, &(p->format), position);
#if PCF_DEBUG
	fprintf(stderr, "nBitmaps : %d\n", p->nBitmaps);
#endif
	p->bitmapOffsets = malloc(sizeof(uint32_t) * p->nBitmaps);
	if (p->bitmapOffsets == NULL)
		err(1, "malloc()");
	for (i = 0; i < p->nBitmaps; i++)
		p->bitmapOffsets[i] = readUINT32(stream, &(p->format),
						 position);
	for (i = 0; i < GLYPHPADOPTIONS; i++)
		p->bitmapSizes[i]   = readUINT32(stream, &(p->format),
						 position);
	bitmapSize = p->bitmapSizes[p->format.glyph];
#if PCF_DEBUG
	fprintf(stderr, "bitmapSize : %d bytes - %d (%d bytes/line)\n",
		bitmapSize, p->format.glyph, 1 << p->format.glyph);
#endif
	p->bitmaps = malloc(sizeof(u_char) * bitmapSize);
	if (p->bitmaps == NULL)
		err(1, "malloc()");
	if (fread(p->bitmaps, bitmapSize, 1, stream) != 1)
		unexpectedEOF();
	*position += sizeof(u_char) * bitmapSize;
	pcfBitmaps_swap(p, &(p->format));
}

static void pcfBdfEncodings_initialize(TPcfBdfEncodings *p)
{
	assert(p != NULL);

	p->encodings = NULL;
}

static void pcfBdfEncodings_finalize(TPcfBdfEncodings *p)
{
	assert(p != NULL);

	if (p->encodings != NULL) {
		free(p->encodings);
		p->encodings = NULL;
	}
}

static void pcfBdfEncodings_load(TPcfBdfEncodings *p, FILE *stream,
				 size_t *position)
{
	uint32_t i, n;

	assert(p != NULL);
	assert(stream != NULL);
	assert(position != NULL);

	pcfFormat_load(&(p->format), stream, position);
	if (p->format.id != PCF_DEFAULT_FORMAT)
		errx(1, "(FONT): Bad format id = %d.", p->format.id);
	p->firstCol  = readUINT16(stream, &(p->format), position);
	p->lastCol   = readUINT16(stream, &(p->format), position);
	p->firstRow  = readUINT16(stream, &(p->format), position);
	p->lastRow   = readUINT16(stream, &(p->format), position);
	p->defaultCh = readUINT16(stream, &(p->format), position);
	assert(p->lastCol >= p->firstCol);
	assert(p->lastRow >= p->firstRow);
	n = (p->lastCol - p->firstCol + 1) * (p->lastRow - p->firstRow + 1);
	p->encodings = malloc(sizeof(uint16_t) * n);
	if (p->encodings == NULL)
		err(1, "malloc()");
	for (i = 0; i < n; i++)
		p->encodings[i] = readUINT16(stream, &(p->format), position);
}

#ifdef PCF_DEBUG
static void pcfBdfEncodings_debug(TPcfBdfEncodings *p)
{
	assert(p != NULL);

	fprintf(stderr, "BDF_ENCODINGS[");
	fprintf(stderr, "col:%d-%d[%x-%x] ",
		p->firstCol, p->lastCol, p->firstCol, p->lastCol);
	fprintf(stderr, "row:%d-%d[%x-%x] ",
		p->firstRow, p->lastRow, p->firstRow, p->lastRow);
	fprintf(stderr, "defaultCh: %d ", p->defaultCh);
	fprintf(stderr, "n = %d]\n",
		(p->lastCol - p->firstCol + 1) * (p->lastRow - p->firstRow + 1));
}
#endif

static TPcfTable *searchSection(TPcf *p, uint32_t type)
{
	uint32_t i;
	TPcfTable *result;

	assert(p != NULL);

	result = NULL;
	for (i = 0; i < p->nTables; i++) {
		if (p->tables[i].type == type) {
			result = &(p->tables[i]);
			break;
		}
	}
	return result;
}

static size_t seekSection(FILE *stream, size_t current, size_t offset)
{
	size_t nbytes;
	size_t n;
	char dummy[1024];

	assert(stream != NULL);

	if (current > offset)
		errx(1, "(FONT): Backward seeking.");
	if (current == offset)
		return offset;
	nbytes = offset - current;
	while (nbytes != 0) {
		n = nbytes > sizeof(dummy) ? sizeof(dummy) : nbytes;
		if (fread(dummy, n, 1, stream) != 1)
			unexpectedEOF();
		nbytes -= n;
	}
	return offset;
}

void pcf_initialize(TPcf *p)
{
	assert(p != NULL);

	p->nTables = 0;
	p->tables = NULL;
	pcfProperties_initialize(&(p->properties));
	pcfMetrics_initialize(&(p->metrics));
	pcfBitmaps_initialize(&(p->bitmaps));
	pcfBdfEncodings_initialize(&(p->bdfEncodings));
}

void pcf_finalize(TPcf *p)
{
	assert(p != NULL);

	p->nTables = 0;
	if (p->tables != NULL) {
		free(p->tables);
		p->tables = NULL;
	}
	pcfProperties_finalize(&(p->properties));
	pcfMetrics_finalize(&(p->metrics));
	pcfBitmaps_finalize(&(p->bitmaps));
	pcfBdfEncodings_finalize(&(p->bdfEncodings));
}

void pcf_load(TPcf *p, FILE *stream)
{
	uint32_t version;
	size_t position;
	TPcfTable *table;
	uint32_t i;

	assert(p != NULL);
	assert(stream != NULL);

	position = 0;
	/* Table of Contents */
	version = readUINT32LE(stream, &position);
	if (version != PCF_VERSION)
		errx(1, "(FONT): PCF file format error: Bad signature.");
	p->nTables = readUINT32LE(stream, &position);
	p->tables = malloc(sizeof(TPcfTable) * p->nTables);
	if (p->tables == NULL)
		err(1, "malloc()");
	for (i = 0; i < p->nTables; i++) {
		pcfTable_load(&(p->tables[i]), stream, &position);
#if PCF_DEBUG
		fprintf(stderr, "Table %d :", i);
		pcfTable_debug(&(p->tables[i]));
		fprintf(stderr, "\n");
#endif
	}
#if PCF_DEBUG
	fprintf(stderr, "total gain = %d\n", position);
#endif

	/* Properties */
	table = searchSection(p, PCF_PROPERTIES);
	if (table == NULL)
		errx(1, "(FONT): Properties section not exist.");
	position = seekSection(stream, position, table->offset);
#if PCF_DEBUG
	fprintf(stderr, "total gain = %d\n", position);
	fprintf(stderr, "load Properties\n");
#endif
	pcfProperties_load(&(p->properties), stream, &position);
#if PCF_DEBUG
	pcfProperties_debug(&(p->properties));
	fprintf(stderr, "total gain = %d\n", position);
#endif

	/* Accelerators */
	table = searchSection(p, PCF_ACCELERATORS);
	if (searchSection(p, PCF_BDF_ACCELERATORS) == NULL) {
		if (table == NULL)
			errx(1, "(FONT): Accelerators section and BDF Accelerators section not found.");
#if PCF_DEBUG
		fprintf(stderr, "total gain = %d:%ld\n",
				position, ftell(stream));
#endif
		position = seekSection(stream, position, table->offset);
#if PCF_DEBUG
		fprintf(stderr, "total gain = %d:%ld\n",
				position, ftell(stream));
		fprintf(stderr, "load Accelerators\n");
#endif
		pcfAccelerators_load(&(p->accelerators), stream, &position);
#if PCF_DEBUG
		pcfAccelerators_debug(&(p->accelerators));
		fprintf(stderr, "total gain = %d\n", position);
#endif
	}

	/*  Metrics */
	table = searchSection(p, PCF_METRICS);
	if (table == NULL)
		errx(1, "(FONT): Metrics section not found.");
	position = seekSection(stream, position, table->offset);
#if PCF_DEBUG
	fprintf(stderr, "total gain = %d\n", position);
	fprintf(stderr, "load Metrics\n");
#endif
	pcfMetrics_load(&(p->metrics), stream, &position);
#if PCF_DEBUG
	fprintf(stderr, "total gain = %d\n", position);
#endif

	/* Bitmaps */
	table = searchSection(p, PCF_BITMAPS);
	if (table == NULL)
		errx(1, "(FONT): Bitmaps section not found.");
	position = seekSection(stream, position, table->offset);
#if PCF_DEBUG
	fprintf(stderr, "total gain = %d\n", position);
	fprintf(stderr, "load Bitmaps\n");
#endif
	pcfBitmaps_load(&(p->bitmaps), stream, &position);
#if PCF_DEBUG
	fprintf(stderr, "total gain = %d\n", position);
#endif

	/* BDF Encodings */
	table = searchSection(p, PCF_BDF_ENCODINGS);
	if (table == NULL)
		errx(1, "(FONT): BDF Encodings section not found.");
	position = seekSection(stream, position, table->offset);
#if PCF_DEBUG
	fprintf(stderr, "total gain = %d:%ld\n", position, ftell(stream));
	fprintf(stderr, "load BDF Encodings\n");
#endif
	pcfBdfEncodings_load(&(p->bdfEncodings), stream, &position);
#if PCF_DEBUG
	pcfBdfEncodings_debug(&(p->bdfEncodings));
	fprintf(stderr, "total gain = %d:%ld\n", position, ftell(stream));
#endif

	/* BDF Accelerators */
	table = searchSection(p, PCF_BDF_ACCELERATORS);
	if (table != NULL) {
		position = seekSection(stream, position, table->offset);
#if PCF_DEBUG
		fprintf(stderr, "total gain = %d\n", position);
		fprintf(stderr, "load BDF Accelerators\n");
#endif
		pcfAccelerators_load(&(p->accelerators), stream, &position);
#if PCF_DEBUG
		pcfAccelerators_debug(&(p->accelerators));
		fprintf(stderr, "total gain = %d\n", position);
#endif
	}
}

void pcf_as_font(TPcf *p, TFont *font)
{
	TPcfMetric *metric;
	uint32_t bitmapOffset, bitmapSize;
	uint16_t encoding, defaultCh, cols;
	uint8_t col, row;
	uint32_t i, glyphs;

	assert(p != NULL);
	assert(font != NULL);

	font->width  = p->accelerators.minBounds.rightSideBearing -
		       p->accelerators.minBounds.leftSideBearing;
	font->height = p->accelerators.minBounds.ascent +
		       p->accelerators.minBounds.descent;

	bitmapSize = p->bitmaps.bitmapSizes[p->bitmaps.format.glyph];
	font->bitmap = malloc(bitmapSize);
	if (font->bitmap == NULL)
		err(1, "malloc()");
	memcpy(font->bitmap, p->bitmaps.bitmaps, bitmapSize);

	font->bytesPerWidth = 1 << p->bitmaps.format.glyph;
	font->bytesPerChar = font->bytesPerWidth * font->height;
	font->colf = p->bdfEncodings.firstCol;
	font->coll = p->bdfEncodings.lastCol;
	font->rowf = p->bdfEncodings.firstRow;
	font->rowl = p->bdfEncodings.lastRow;
	font->colspan = font->coll - font->colf + 1;

	glyphs = (font->coll - font->colf + 1) * (font->rowl - font->rowf + 1);
	font->glyphs = malloc(sizeof(u_char *) * glyphs);
	if (font->glyphs == NULL)
		err(1, "malloc()");

	font->glyphWidths = NULL;
	if (p->accelerators.terminalFont == 0) {
		/* not terminal font */
		font->glyphWidths = malloc(sizeof(u_short) * glyphs);
		if (font->glyphWidths == NULL)
			err(1, "malloc()");
	}

	font->defaultGlyph = malloc(font->bytesPerChar);
	if (font->defaultGlyph == NULL)
		err(1, "malloc()");
	memset(font->defaultGlyph, 0xff, font->bytesPerChar);

	defaultCh = p->bdfEncodings.defaultCh;
	if (defaultCh != (uint16_t)NO_SUCH_CHAR) {
		col = defaultCh & 0xff;
		row = (defaultCh >> 8) & 0xff;
		if (p->bdfEncodings.firstCol <= col &&
		    p->bdfEncodings.lastCol  >= col &&
		    p->bdfEncodings.firstRow <= row &&
		    p->bdfEncodings.lastRow  >= row) {
			col -= p->bdfEncodings.firstCol;
			row -= p->bdfEncodings.firstRow;
			cols = (p->bdfEncodings.lastCol -
				p->bdfEncodings.firstCol + 1);
			defaultCh = row * cols + col;
		} else
			defaultCh = (uint16_t)NO_SUCH_CHAR;
	}

	for (i = 0; i < glyphs; i++) {
		encoding = p->bdfEncodings.encodings[i];
		if (encoding == (uint16_t)NO_SUCH_CHAR) {
			if (defaultCh == (uint16_t)NO_SUCH_CHAR) {
				font->glyphs[i] = font->defaultGlyph;
				if (font->glyphWidths != NULL)
					font->glyphWidths[i] = font->width;
				continue;
			}
			encoding = p->bdfEncodings.encodings[defaultCh];
		}
		assert(p->bitmaps.nBitmaps > encoding);
		bitmapOffset = p->bitmaps.bitmapOffsets[encoding];
		font->glyphs[i] = font->bitmap + bitmapOffset;
		if (font->glyphWidths != NULL) {
			assert(p->metrics.nMetrics > encoding);
			metric = &p->metrics.metrics[encoding];
			font->glyphWidths[i] = metric->rightSideBearing -
					       metric->leftSideBearing;
		}
	}
}

