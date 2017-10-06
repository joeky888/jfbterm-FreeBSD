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


#ifndef INCLUDE_PCF_H
#define INCLUDE_PCF_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>

#include "font.h"

#define GLYPHPADOPTIONS (4)

typedef struct Raw_TPcfFormat {
	uint32_t id;
	uint32_t scan;
	uint32_t bit;
	uint32_t byte;
	uint32_t glyph;
} TPcfFormat;

typedef struct Raw_TPcfTable {
	uint32_t type;
	TPcfFormat format;
	uint32_t size;
	uint32_t offset;
} TPcfTable;

typedef struct Raw_TPcfProp {
	TPcfFormat format;
	uint32_t name;
	uint8_t isStringProp;
	uint32_t value;
} TPcfProp;

typedef struct Raw_TPcfProperties {
	TPcfFormat format;
	uint32_t nProps;
	TPcfProp *props;
	uint32_t stringSize;
	char *string;
} TPcfProperties;

typedef struct Raw_TPcfMetric {
	uint16_t leftSideBearing;
	uint16_t rightSideBearing;
	uint16_t characterWidth;
	uint16_t ascent;
	uint16_t descent;
	uint16_t attributes;
} TPcfMetric;

typedef struct Raw_TPcfMetrics {
	TPcfFormat format;
	uint32_t nMetrics;
	TPcfMetric *metrics;
} TPcfMetrics;

typedef struct Raw_TPcfAccelerators {
	TPcfFormat format;
	uint8_t noOverlap;
	uint8_t constantMetrics;
	uint8_t terminalFont;
	uint8_t constantWidth;
	uint8_t inkInside;
	uint8_t inkMetrics;
	uint8_t drawDirection;
	uint32_t fontAscent;
	uint32_t fontDescent;
	uint32_t maxOverlap;
	TPcfMetric minBounds;
	TPcfMetric maxBounds;
	TPcfMetric ink_minBounds;
	TPcfMetric ink_maxBounds;
} TPcfAccelerators;

typedef struct Raw_TPcfBitmaps {
	TPcfFormat format;
	uint32_t nBitmaps;
	uint32_t *bitmapOffsets;
	uint32_t bitmapSizes[GLYPHPADOPTIONS];
	u_char *bitmaps;
} TPcfBitmaps;

typedef struct Raw_TPcfBdfEncodings {
	TPcfFormat format;
	uint16_t firstCol;
	uint16_t lastCol;
	uint16_t firstRow;
	uint16_t lastRow;
	uint16_t defaultCh;
	uint16_t *encodings;
} TPcfBdfEncodings;

typedef struct Raw_TPcf {
	uint32_t nTables;
	TPcfTable *tables;
	TPcfProperties properties;
	TPcfMetrics metrics;
	TPcfAccelerators accelerators;
	TPcfBitmaps bitmaps;
	TPcfBdfEncodings bdfEncodings;
} TPcf;

void pcf_initialize(TPcf *p);
void pcf_finalize(TPcf *p);
void pcf_load(TPcf *p, FILE *stream);
void pcf_as_font(TPcf *p, TFont *font);

#endif /* INCLUDE_PCF_H */

