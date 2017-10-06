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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "csv.h"
#include "eastasianwidth.h"
#include "font.h"
#include "framebuffer.h"
#include "pcf.h"
#include "palette.h"
#include "picofont.h"
#include "privilege.h"
#include "utilities.h"

u_short gFontsWidth = 0;
u_short gFontsHeight = 0;

#define DEFAULT_FONT_SHADOW_COLOR (0) /* Black */

typedef enum {
	FONT_EFFECT_OFF,
	FONT_EFFECT_SHADOW
} FONT_EFFECT;

static struct {
	FONT_EFFECT effect;
	uint8_t shadowColor;
} font = {
	FONT_EFFECT_OFF,
	DEFAULT_FONT_SHADOW_COLOR
};

static bool initialized;
static pid_t child;

#ifdef ENABLE_UTF8
struct unifontGlyphList {
	uint16_t ucs2;
	u_char *glyph;
	u_short glyphWidth;
	bool ambiguous;
	u_char *halfWidthGlyph;
	u_short halfWidthGlyphWidth;
	u_char *fullWidthGlyph;
	u_short fullWidthGlyphWidth;
	struct unifontGlyphList *next;
};

struct unifontGlyphList *unifontGlyphList = NULL;
#endif

static struct {
	u_short bytesPerWidth;
	u_short bytesPerChar;
	u_char *glyph;
} underlineGlyph;

static void finalize(void);
static void finalizer(void);
static void configFontset(TCapValue *capValue);
static void configEffect(const char *config);
static void configShadowColor(const char *config);
static TFont *getFontByName(const char *name);
static int codeToIndex(TFont *p, uint16_t code);
static const u_char *getDefaultGlyph(TFont *p, uint16_t code, u_short *width);
static const u_char *getStandardGlyph(TFont *p, uint16_t code, u_short *width);
static void childHandler(int signum);
static FILE *openStream(const char *path);
static void setPCFFont(TFont *p, const char *path, FONT_HALF half);
static void setFontAlias(TFont *dst, TFont *src, FONT_HALF half);
static void setFontsSize(void);
#ifdef ENABLE_UTF8
static bool isFullWidth(uint16_t ucs2);
static bool isHalfWidth(uint16_t ucs2);
static bool isAmbiguous(uint16_t ucs2);
static void unifontGlyphList_initialize(void);
static void unifontGlyphList_add(uint16_t ucs2, bool ambiguous,
				 u_char *halfWidthGlyph,
				 u_short halfWidthGlyphWidth,
				 u_char *fullWidthGlyph,
				 u_short fullWidthGlyphWidth);
static void unifontGlyphList_finalize(void);
static u_char *createFullWidthGlyph(TFont *p, uint16_t ucs2,
				    u_short *glyphWidth);
static u_char *createHalfWidthGlyph(TFont *p, uint16_t ucs2,
				    u_short *glyphWidth);
static void createUnifontGlyph(TFont *p);
static void saveUnifontGlyph(TFont *p);
static void restoreUnifontGlyph(TFont *p);
#endif
static void underlineGlyph_initialize(void);
static void createUnderlineGlyph(void);
static void underlineGlyph_finalize(void);

#define FMACRO_94__FONT(final, align, fontname) { \
		.getGlyph = getDefaultGlyph, \
		.name = fontname, \
		.width = 1, \
		.height = 1, \
		.signature = FONT_SIGNATURE_SINGLE | FONT_SIGNATURE_94CHAR | final, \
		.half = align,	\
		.alias = false, \
		.glyphs = NULL, \
		.glyphWidths = NULL, \
		.defaultGlyph = NULL, \
		.bitmap = NULL,	\
		.colf = 0xff, \
		.coll = 0x00, \
		.rowf = 0xff, \
		.rowl = 0x00, \
		.colspan = 0x00, \
		.bytesPerWidth = 2, \
		.bytesPerChar = 1 }

#define FMACRO_96__FONT(final, align, fontname) { \
		.getGlyph = getDefaultGlyph, \
		.name = fontname, \
		.width = 1, \
		.height = 1, \
		.signature = FONT_SIGNATURE_SINGLE | FONT_SIGNATURE_96CHAR | final, \
		.half = align,	\
		.alias = false, \
		.glyphs = NULL, \
		.glyphWidths = NULL, \
		.defaultGlyph = NULL, \
		.bitmap = NULL,	\
		.colf = 0xff, \
		.coll = 0x00, \
		.rowf = 0xff, \
		.rowl = 0x00, \
		.colspan = 0x00, \
		.bytesPerWidth = 2, \
		.bytesPerChar = 1 }

#define FMACRO_94N_FONT(final, align, fontname) { \
		.getGlyph = getDefaultGlyph, \
		.name = fontname, \
		.width = 2, \
		.height = 1, \
		.signature = FONT_SIGNATURE_DOUBLE | FONT_SIGNATURE_94CHAR | final, \
		.half = align,	\
		.alias = false, \
		.glyphs = NULL, \
		.glyphWidths = NULL, \
		.defaultGlyph = NULL,	\
		.bitmap = NULL,	\
		.colf = 0xff, \
		.coll = 0x00, \
		.rowf = 0xff, \
		.rowl = 0x00, \
		.colspan = 0x00, \
		.bytesPerWidth = 2, \
		.bytesPerChar = 2 }

TFont gFonts[] = {
	FMACRO_94__FONT(0x40, FONT_HALF_LEFT,  "iso646-1973irv"),
	/* CJK Character sets */
	FMACRO_94N_FONT(0x40, FONT_HALF_LEFT,  "jisc6226-1978"),
	FMACRO_94N_FONT(0x41, FONT_HALF_LEFT,  "gb2312-80"),
	FMACRO_94N_FONT(0x42, FONT_HALF_LEFT,  "jisx0208-1983"),
#if 0
	FMACRO_94N_FONT(0x42, FONT_HALF_LEFT,  "jisx0208-1990"),
#endif
	FMACRO_94N_FONT(0x43, FONT_HALF_LEFT,  "ksc5601-1987"),
	FMACRO_94N_FONT(0x44, FONT_HALF_LEFT,  "jisx0212-1990"),
	FMACRO_94N_FONT(0x45, FONT_HALF_LEFT,  "ccitt-extended-gb" /*aka iso-ir-165*/),
	FMACRO_94N_FONT(0x47, FONT_HALF_LEFT,  "cns11643-1992.1"),
	FMACRO_94N_FONT(0x48, FONT_HALF_LEFT,  "cns11643-1992.2"),
	FMACRO_94N_FONT(0x49, FONT_HALF_LEFT,  "cns11643-1992.3"),
	FMACRO_94N_FONT(0x4a, FONT_HALF_LEFT,  "cns11643-1992.4"),
	FMACRO_94N_FONT(0x4b, FONT_HALF_LEFT,  "cns11643-1992.5"),
	FMACRO_94N_FONT(0x4c, FONT_HALF_LEFT,  "cns11643-1992.6"),
	FMACRO_94N_FONT(0x4d, FONT_HALF_LEFT,  "cns11643-1992.7"),
	/* Domestic ISO 646 Character sets */
	FMACRO_94__FONT(0x41, FONT_HALF_LEFT,  "bs4730"),
	FMACRO_94__FONT(0x42, FONT_HALF_LEFT,  "ansix3.4-1968" /* aka ASCII */),
	FMACRO_94__FONT(0x43, FONT_HALF_LEFT,  "nats1-finland-sweden"),
	FMACRO_94__FONT(0x47, FONT_HALF_LEFT,  "sen850200b"),
	FMACRO_94__FONT(0x48, FONT_HALF_LEFT,  "sen850200c"),
	FMACRO_94__FONT(0x4a, FONT_HALF_LEFT,  "jisc6220-1969roman"),
	FMACRO_94__FONT(0x59, FONT_HALF_LEFT,  "uni0204-70"),
	FMACRO_94__FONT(0x4c, FONT_HALF_LEFT,  "olivetti-portuguese"),
	FMACRO_94__FONT(0x5a, FONT_HALF_LEFT,  "olivetti-spanish"),
	FMACRO_94__FONT(0x4b, FONT_HALF_LEFT,  "din66003"),
	FMACRO_94__FONT(0x52, FONT_HALF_LEFT,  "nfz62-010-1973"),
	FMACRO_94__FONT(0x54, FONT_HALF_LEFT,  "gb1988-80"),
	FMACRO_94__FONT(0x60, FONT_HALF_LEFT,  "ns4551-1"),
	FMACRO_94__FONT(0x61, FONT_HALF_LEFT,  "ns4551-2"),
	FMACRO_94__FONT(0x66, FONT_HALF_LEFT,  "nfz62-010-1982"),
	FMACRO_94__FONT(0x67, FONT_HALF_LEFT,  "ibm-portuguese"),
	FMACRO_94__FONT(0x68, FONT_HALF_LEFT,  "ibm-spanish"),
	FMACRO_94__FONT(0x69, FONT_HALF_LEFT,  "msz7795.3"),
	FMACRO_94__FONT(0x6e, FONT_HALF_LEFT,  "jisc6229-1984ocr-b"),
	FMACRO_94__FONT(0x75, FONT_HALF_LEFT,  "ccitt-t.61-1"),
	FMACRO_94__FONT(0x77, FONT_HALF_LEFT,  "csaz243.4-1985alt1-1"),
	FMACRO_94__FONT(0x78, FONT_HALF_LEFT,  "csaz243.4-1985alt1-2"),
	FMACRO_94__FONT(0x7a, FONT_HALF_LEFT,  "jusi.b1.002"),
	/* Domestic ISO 646 Character sets with I-oct */
#if 0
	FMACRO_94__FONT(0x41, FONT_HALF_LEFT,  "nc99-10:81"),
	FMACRO_94__FONT(0x42, FONT_HALF_LEFT,  "iso646-1992invariants"),
#endif
	/* Quasi ISO 646 Character sets */
	FMACRO_94__FONT(0x45, FONT_HALF_LEFT,  "nats1-denmark-norway"),
	FMACRO_94__FONT(0x55, FONT_HALF_LEFT,  "honeywell-bull-latin-greek"),
	FMACRO_94__FONT(0x56, FONT_HALF_LEFT,  "british-post-office-teletext"),
	FMACRO_94__FONT(0x57, FONT_HALF_LEFT,  "inis-irv-subset"),
	/* Greek Character sets */
	FMACRO_94__FONT(0x5b, FONT_HALF_LEFT,  "olivetti-greek"),
	FMACRO_94__FONT(0x5c, FONT_HALF_LEFT,  "olivetti-latin-greek"),
	FMACRO_94__FONT(0x58, FONT_HALF_LEFT,  "iso5428-1974"),
	FMACRO_94__FONT(0x53, FONT_HALF_LEFT,  "iso5428-1980"),
	/* Greek Character sets  with I-oct */
#if 0
	FMACRO_94__FONT(0x40, FONT_HALF_LEFT,  "ccitt-greek1"),
#endif
	/* Cyrillic Character sets */
	FMACRO_94__FONT(0x4e, FONT_HALF_LEFT,  "iso5427-1981"),
	FMACRO_94__FONT(0x5e, FONT_HALF_LEFT,  "inis-cyrillic-extension"),
	FMACRO_94__FONT(0x51, FONT_HALF_LEFT,  "iso5427-1981extension"),
	FMACRO_96__FONT(0x40, FONT_HALF_LEFT,  "ecma-94cyrillic"),
	FMACRO_94__FONT(0x7b, FONT_HALF_LEFT,  "jusi.b1.003"),
	FMACRO_94__FONT(0x7d, FONT_HALF_LEFT,  "jusi.b1.004"),
	FMACRO_96__FONT(0x4f, FONT_HALF_LEFT,  "stsev358-88"),
	/* ISO 8859 variants */
	FMACRO_96__FONT(0x41, FONT_HALF_RIGHT, "iso8859.1-1987"),
	FMACRO_96__FONT(0x42, FONT_HALF_RIGHT, "iso8859.2-1987"),
	FMACRO_96__FONT(0x43, FONT_HALF_RIGHT, "iso8859.3-1988"),
	FMACRO_96__FONT(0x44, FONT_HALF_RIGHT, "iso8859.4-1988"),
	FMACRO_96__FONT(0x4c, FONT_HALF_RIGHT, "iso8859.5-1988"),
	FMACRO_96__FONT(0x47, FONT_HALF_RIGHT, "iso8859.6-1987"),
	FMACRO_96__FONT(0x46, FONT_HALF_RIGHT, "iso8859.7-1987"),
	FMACRO_96__FONT(0x48, FONT_HALF_RIGHT, "iso8859.8-1988"),
	FMACRO_96__FONT(0x4d, FONT_HALF_RIGHT, "iso8859.9-1989"),
	FMACRO_96__FONT(0x56, FONT_HALF_RIGHT, "iso8859.10-1992"),
	FMACRO_96__FONT(0x54, FONT_HALF_RIGHT, "iso8859.11-2001"),
	FMACRO_96__FONT(0x59, FONT_HALF_RIGHT, "iso8859.13-1998"),
	FMACRO_96__FONT(0x5f, FONT_HALF_RIGHT, "iso8859.14-1998"),
	FMACRO_96__FONT(0x62, FONT_HALF_RIGHT, "iso8859.15-1999"),
	FMACRO_96__FONT(0x66, FONT_HALF_RIGHT, "iso8859.16-2001"),
	/* CCITT T.101 Mosaic Charactor Sets */
	FMACRO_94__FONT(0x79, FONT_HALF_LEFT,  "ccitt-mosaic-1"),
	FMACRO_96__FONT(0x7d, FONT_HALF_LEFT,  "ccitt-mosaic-sup1"),
	FMACRO_94__FONT(0x63, FONT_HALF_LEFT,  "ccitt-mosaic-sup2"),
	FMACRO_94__FONT(0x64, FONT_HALF_LEFT,  "ccitt-mosaic-sup3"),
	/* Japano */
	FMACRO_94__FONT(0x49, FONT_HALF_LEFT,  "jisc6220-1969kana"),
	/* JISX0213 */
	FMACRO_94N_FONT(0x4f, FONT_HALF_LEFT,  "jisx0213-2000-1"),
	FMACRO_94N_FONT(0x50, FONT_HALF_LEFT,  "jisx0213-2000-2"),
	/* Linux Private */
	FMACRO_94__FONT(0x30, FONT_HALF_LEFT,  "vt100-graphics"),
#ifdef ENABLE_UTF8
	/* Unicode 5.1.0 */
	{
		.getGlyph = getDefaultGlyph,
		.name = "iso10646.1",
		.width = 1,
		.height = 1,
		.signature = FONT_SIGNATURE_OTHER,
		.half = FONT_HALF_UNI,
		.alias = false,
		.glyphs = NULL,
		.glyphWidths = NULL,
		.defaultGlyph = NULL,
		.bitmap = NULL,
		.colf = 0xff,
		.coll = 0x00,
		.rowf = 0xff,
		.rowl = 0x00,
		.colspan = 0x00,
		.bytesPerWidth = 1,
		.bytesPerChar = 1
	},
#endif
	{
		.getGlyph = getDefaultGlyph,
		.name = NULL,
		.width = 0,
		.height = 0,
		.signature = 0x00000000,
		.half = FONT_HALF_LEFT,
		.alias = false,
		.glyphs = NULL,
		.glyphWidths = NULL,
		.defaultGlyph = NULL,
		.bitmap = NULL,
		.colf = 0x00,
		.coll = 0x00,
		.rowf = 0x00,
		.rowl = 0x00,
		.colspan = 0x00,
		.bytesPerWidth = 0,
		.bytesPerChar = 0
	}
};

void font_initialize(void)
{
	assert(!initialized);

	atexit(finalizer);
#ifdef ENABLE_UTF8
	unifontGlyphList_initialize();
#endif
	underlineGlyph_initialize();
	initialized = true;
}

static void finalize(void)
{
	TFont *p;

	assert(initialized);

	for (p = gFonts; p->name != NULL; p++) {
		if (!p->alias && font_isLoaded(p)) {
			p->getGlyph = getDefaultGlyph;
			if (p->glyphs != NULL) {
				free(p->glyphs);
				p->glyphs = NULL;
			}
			if (p->glyphWidths != NULL) {
				free(p->glyphWidths);
				p->glyphWidths = NULL;
			}
			if (p->defaultGlyph != NULL) {
				free(p->defaultGlyph);
				p->defaultGlyph = NULL;
			}
			if (p->bitmap != NULL) {
				free(p->bitmap);
				p->bitmap = NULL;
			}
		}
	}
#ifdef ENABLE_UTF8
	unifontGlyphList_finalize();
#endif
	underlineGlyph_finalize();
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

static void configFontset(TCapValue *capValue)
{
	struct sigaction act, oldact;
	const char *name, *type, *side, *path;
	TFont *dst, *src;
	TCapValue *p;
	TCsv csv;
	FONT_HALF half;
	int i;

	assert(initialized);
	assert(capValue != NULL);

	bzero(&act, sizeof(act));
	act.sa_handler = childHandler;
	act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
	sigaction(SIGCHLD, &act, &oldact);
	for (p = capValue; p != NULL; p = p->next) {
		csv_initialize(&csv, p->value);
		if (csv.tokens != 4) {
			warnx("FONT : Skipped (BAD FORMAT)");
			csv_finalize(&csv);
			continue;
		}
		name = csv_getToken(&csv);
		assert(name != NULL);
		type = csv_getToken(&csv);
		assert(type != NULL);
		side = csv_getToken(&csv);
		assert(side != NULL);
		path = csv_getToken(&csv);
		assert(path != NULL);
		fprintf(stderr, "FONT : [%s]/%s/%s://%s/\n",
			name, type, side, path);
		if ((dst = getFontByName(name)) == NULL) {
			warnx("FONT : Skipped (Unknown FONTSET=`%s')", name);
			csv_finalize(&csv);
			continue;
		}
		i = lookup(true, side, "L", "R", "U", (char *)NULL);
		switch (i) {
		case 0:
			half = FONT_HALF_LEFT;
			break;
		case 1:
			half = FONT_HALF_RIGHT;
			break;
		case 2:
			half = FONT_HALF_UNI;
			break;
		default:
			warnx("FONT : Skipped (BAD FORMAT)");
			csv_finalize(&csv);
			continue;
		}
		i = lookup(true, type, "pcf", "alias", (char *)NULL);
		switch (i) {
		case 0:
			setPCFFont(dst, path, half);
			break;
		case 1:
			if ((src = getFontByName(path)) == NULL) {
				warnx("FONT : Skipped (Unknown ALIAS FONTSET=`%s')", path);
				csv_finalize(&csv);
				continue;
			}
			setFontAlias(dst, src, half);
			break;
		default:
			warnx("FONT : Skipped (BAD FORMAT)");
			csv_finalize(&csv);
			continue;
		}
		csv_finalize(&csv);
	}

	sigaction(SIGCHLD, &oldact, NULL);
#ifdef ENABLE_UTF8
	if (!font_isLoaded(&(gFonts[0]))) {
		i = font_getIndexByName("iso10646.1");
		if (i == -1 || !font_isLoaded(&(gFonts[i])))
			errx(1, "FONT : ISO8859/ISO10646 not loaded.");
	}
	i = font_getIndexByName("iso10646.1");
	if (i != -1)
		createUnifontGlyph(&(gFonts[i]));
#else
	if (!font_isLoaded(&(gFonts[0])))
		errx(1, "FONT : ISO8859 not loaded.");
#endif
	setFontsSize();
	createUnderlineGlyph();
}

static void configEffect(const char *config)
{
	bool found;
	int i;

	static const struct {
		const char *key;
		const FONT_EFFECT effect;
	} list[] = {
		{ "Shadow", FONT_EFFECT_SHADOW },
		{ "Off",    FONT_EFFECT_OFF    },
		{ NULL,     FONT_EFFECT_OFF    }
	};

	assert(initialized);

	font.effect = FONT_EFFECT_OFF;
	if (config != NULL) {
		found = false;
		for (i = 0; list[i].key != NULL; i++) {
			if (strcasecmp(list[i].key, config) == 0) {
				font.effect = list[i].effect;
				found = true;
				break;
			}
		}
		if (!found)
			warnx("Invalid font effect: %s", config);
	}
}

static void configShadowColor(const char *config)
{
	int ansiColor;

	assert(initialized);

	if (config != NULL) {
		ansiColor = atoi(config);
		if (ansiColor >= 0 && ansiColor <= 15)
			font.shadowColor = palette_ansiToVGA(ansiColor);
		else
			warnx("Invalid font shadow color: %s", config);
	}
}

void font_configure(TCaps *caps)
{
	TCapability *capability;
	const char *config;

	assert(initialized);
	assert(caps != NULL);

	capability = caps_find(caps, "fontset");
	if (capability == NULL || capability->values == NULL)
		errx(1, "No font specified.");
	configFontset(capability->values);
	config = caps_findFirst(caps, "font.effect");
	configEffect(config);
	config = caps_findFirst(caps, "font.shadow.color");
	configShadowColor(config);
}

void font_showList(FILE *stream)
{
	TFont *p;
	char c;
	u_int n, u, d, f1, f2;

	/* not require any initialize */
	assert(stream != NULL);

	for (p = gFonts; p->name != NULL; p++) {
		n = p->signature;
		c = n & 0xff;
		u = (n >> 4) & 15;
		d = (n >> 0) & 15;
		f1 = n & FONT_SIGNATURE_DOUBLE;
		f2 = n & FONT_SIGNATURE_96CHAR;
		if (p->half == FONT_HALF_UNI)
			fprintf(stream, " [--/--(-) uni ] %s\n", p->name);
		else {
			fprintf(stream, " [%02u/%02u(%c) %s%s] %s\n",
				u, d, c,
				f2 ? "96" : "94",
				f1 ? "^N" : "  ",
				p->name);
		}
	}
}

int font_getIndexBySignature(const u_int signature)
{
	TFont *p;
	int i;

	/* not require any initialize */
	for (p = gFonts, i = 0; p->name != NULL; p++, i++)
		if (p->signature == signature)
			return i;
	return -1;
}

int font_getIndexByName(const char *name)
{
	TFont *p;
	int i;

	/* not require any initialize */
	assert(name != NULL);

	for (p = gFonts, i = 0; p->name != NULL; p++, i++)
		if (strcasecmp(p->name, name) == 0)
			return i;
	return -1;
}

static TFont *getFontByName(const char *name)
{
	TFont *p;

	assert(initialized);
	assert(name != NULL);

	for (p = gFonts; p->name != NULL; p++)
		if (strcasecmp(p->name, name) == 0)
			return p;
	return NULL;
}

static int codeToIndex(TFont *p, uint16_t code)
{
	uint8_t col, row;

	assert(initialized);
	assert(p != NULL);

	col = code & 0xff;
	row = (code >> 8) & 0xff;
	if (col < p->colf || p->coll < col || row < p->rowf || p->rowl < row)
		return -1;
	return (col - p->colf) + (row - p->rowf) * p->colspan;
}

static const u_char *getDefaultGlyph(TFont *p, uint16_t code, u_short *width)
{
	static u_char glyph[PICOFONT_HEIGHT * 2];

	u_char *cp;
	u_int i, a, b;

	assert(initialized);
	assert(p != NULL);
	assert(width != NULL);

	cp = glyph;
	*width = p->width;
	*cp++ = 0x80;
	*cp++ = 0x00;
	for (i = 0; i < 16; i += 4) {
		b = (code >> (12 - i)) & 0xf;
		b <<= 1;
		a = (code >> (28 - i)) & 0xf;
		a <<= 1;
		*cp++ = 0x80 | gPicoFontLeft[b++];
		*cp++ = gPicoFontLeft[a++];
		*cp++ = 0x80 | gPicoFontLeft[b];
		*cp++ = gPicoFontLeft[a];
		*cp++ = 0x80;
		*cp++ = 0x00;
	}
	*cp++ = 0x80;
	*cp++ = 0x00;
	if (p->bytesPerChar == 2) {
		*cp++ = 0xff;
		*cp = 0xfe;
	} else {
		*cp++ = 0xfe;
		*cp = 0x00;
	}
	return glyph;
}

static const u_char *getStandardGlyph(TFont *p, uint16_t code, u_short *width)
{
	int i;

	assert(initialized);
	assert(p != NULL);
	assert(width != NULL);

	i = codeToIndex(p, code);
	if (i == -1) {
		*width = p->width;
		return p->defaultGlyph;
	}
	if (p->glyphWidths != NULL)
		*width = p->glyphWidths[i];
	else
		*width = p->width;
	return p->glyphs[i];
}

static void childHandler(int signum)
{
	int status, errsv;
	pid_t pid;

	UNUSED_VARIABLE(signum);
	errsv = errno;
	pid = waitpid(child, &status, WNOHANG);
	if (pid == (pid_t)-1 && errno != ECHILD)
		err(1, "waitpid()");
	if (pid == child || (pid == (pid_t)-1 && errno == ECHILD)) {
		if (WIFSIGNALED(status))
			errx(1, "Child process died with signal %s",
			     sys_siglist[WTERMSIG(status)]);
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
			errx(1, "Child process exited with status %d",
			     WEXITSTATUS(status));
	}
	errno = errsv;
}

static FILE *openStream(const char *path)
{
	int i, fd, filedes[2];

	assert(initialized);
	assert(path != NULL);

	if ((strlen(path) > 3 && strrcmp(path, ".gz") == 0) ||
	    (strlen(path) > 2 && strrcmp(path, ".Z") == 0)) {
		if (pipe(filedes) == -1)
			return NULL;
		fflush(stdout);
		fflush(stderr);
		child = fork();
		if (child == (pid_t)-1)
			err(1, "fork()");
		else if (child == (pid_t)0) {
			/* Child process */
			privilege_drop();
			if ((fd = open(path, O_RDONLY)) == -1) {
				close(filedes[0]);
				close(filedes[1]);
				exit(EXIT_FAILURE);
				/* NOTREACHED */
			}
			close(filedes[0]);
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			dup2(fd, STDIN_FILENO);
			dup2(filedes[1], STDOUT_FILENO);
			for (i = getdtablesize(); i > STDERR_FILENO; i--)
				close(i);
#ifndef GUNZIP_PATH
	#error GUNZIP_PATH is not defined.
#else
			execl(GUNZIP_PATH, GUNZIP_PATH, "-c", (char *)NULL);
#endif
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		} else {
			/* Parent process */
			close(filedes[1]);
			return fdopen(filedes[0], "r");
		}
	} else
		return fopen(path, "r");
	return NULL;
}

static void setPCFFont(TFont *p, const char *path, FONT_HALF half)
{
	FILE *stream;
	TPcf pcf;

	assert(initialized);
	assert(p != NULL);
	assert(path != NULL);

	pcf_initialize(&pcf);
	if (access(path, R_OK) != 0) {
		warnx("PCF : READ ACCESS ERROR : %s", path);
		return;
	}
	stream = openStream(path);
	if (stream == NULL) {
		warnx("PCF : CANNOTOPEN : %s", path);
		return;
	}
	pcf_load(&pcf, stream);
	fclose(stream);
	pcf_as_font(&pcf, p);
	p->half = half;
	p->getGlyph = getStandardGlyph;
	pcf_finalize(&pcf);
}

static void setFontAlias(TFont *dst, TFont *src, FONT_HALF half)
{
	assert(initialized);
	assert(dst != NULL);
	assert(src != NULL);

	dst->getGlyph = src->getGlyph;
	dst->width = src->width;
	dst->height = src->height;
	dst->half = half;
	dst->alias = true;
	dst->glyphs = src->glyphs;
	dst->glyphWidths = src->glyphWidths;
	dst->defaultGlyph = src->defaultGlyph;
	dst->bitmap = src->bitmap;
	dst->colf = src->colf;
	dst->coll = src->coll;
	dst->rowf = src->rowf;
	dst->rowl = src->rowl;
	dst->colspan = src->colspan;
	dst->bytesPerWidth = src->bytesPerWidth;
	dst->bytesPerChar = src->bytesPerChar;
}

static void setFontsSize(void)
{
	TFont *p;
	u_short width;

	assert(initialized);

	for (p = gFonts; p->name != NULL; p++) {
		if (gFontsHeight < p->height)
			gFontsHeight = p->height;
		width = (p->signature & FONT_SIGNATURE_DOUBLE) ?
			(p->width + 1) / 2 : p->width;
		if (gFontsWidth < width)
			gFontsWidth = width;
	}
	if (PICOFONT_WIDTH <= gFontsWidth && PICOFONT_HEIGHT <= gFontsHeight) {
		for (p = gFonts; p->name != NULL; p++) {
			if (p->getGlyph == getDefaultGlyph) {
				p->height = PICOFONT_HEIGHT;
				p->width = PICOFONT_WIDTH * p->width;
			}
		}
	}
}

bool font_isLoaded(TFont *p)
{
	assert(initialized);
	assert(p != NULL);

	return p->getGlyph != getDefaultGlyph;
}

#ifdef ENABLE_UTF8
static bool isFullWidth(uint16_t ucs2)
{
	return eastasianwidth_isFullWidth(ucs2) ||
	       eastasianwidth_isWide(ucs2);
}

static bool isHalfWidth(uint16_t ucs2)
{
	return eastasianwidth_isHalfWidth(ucs2) ||
	       eastasianwidth_isNarrow(ucs2) ||
	       eastasianwidth_isNeutral(ucs2);
}

static bool isAmbiguous(uint16_t ucs2)
{
	return eastasianwidth_isAmbiguous(ucs2);
}

static void unifontGlyphList_initialize(void)
{
	assert(unifontGlyphList == NULL);

	unifontGlyphList = NULL;
}

static void unifontGlyphList_add(uint16_t ucs2, bool ambiguous,
				 u_char *halfWidthGlyph,
				 u_short halfWidthGlyphWidth,
				 u_char *fullWidthGlyph,
				 u_short fullWidthGlyphWidth)
{
	struct unifontGlyphList *p, *n;

	n = malloc(sizeof(struct unifontGlyphList));
	if (n == NULL)
		err(1, "malloc()");
	n->ucs2 = ucs2;
	n->glyph = NULL;
	n->glyphWidth = 0;
	n->ambiguous = ambiguous;
	n->halfWidthGlyph = halfWidthGlyph;
	n->halfWidthGlyphWidth = halfWidthGlyphWidth;
	n->fullWidthGlyph = fullWidthGlyph;
	n->fullWidthGlyphWidth = fullWidthGlyphWidth;
	n->next = NULL;
	if (unifontGlyphList != NULL) {
		p = unifontGlyphList;
		while (p->next != NULL)
			p = p->next;
		p->next = n;
	} else
		unifontGlyphList = n;
}

static void unifontGlyphList_finalize(void)
{
	struct unifontGlyphList *current, *next;

	current = unifontGlyphList;
	while (current != NULL) {
		next = current->next;
		if (current->halfWidthGlyph != NULL)
			free(current->halfWidthGlyph);
		if (current->fullWidthGlyph != NULL)
			free(current->fullWidthGlyph);
		free(current);
		current = next;
	}
	unifontGlyphList = NULL;
}

static u_char *createFullWidthGlyph(TFont *p, uint16_t ucs2,
				    u_short *glyphWidth)
{
	int i, x, y;
	u_char *glyph, *s, *d, *cp, c;

	assert(initialized);
	assert(p != NULL);
	assert(glyphWidth != NULL);

	i = codeToIndex(p, ucs2);
	if (i == -1)
		return NULL; /* out of range */
	if (p->width != p->glyphWidths[i])
		return NULL; /* fullwidth */
	glyph = calloc(p->bytesPerWidth * 2 * p->height, sizeof(u_char));
	if (glyph == NULL)
		err(1, "calloc()");
	s = p->glyphs[i];
	for (y = 0; y < p->height; y++) {
		cp = s;
		d = glyph + (y * p->bytesPerWidth);
		for (x = p->glyphWidths[i]; x >= 8; x -= 8) {
			c = *cp++;
			if (c & 0x80) d[0] |= 0xc0;
			if (c & 0x40) d[0] |= 0x30;
			if (c & 0x20) d[0] |= 0x0c;
			if (c & 0x10) d[0] |= 0x03;
			if (c & 0x08) d[1] |= 0xc0;
			if (c & 0x04) d[1] |= 0x30;
			if (c & 0x02) d[1] |= 0x0c;
			if (c & 0x01) d[1] |= 0x03;
			d += 2;
		}
		if (x > 0) {
			c = *cp++;
			if (c & 0x80) d[0] |= 0xc0;
			if (c & 0x40) d[0] |= 0x30;
			if (c & 0x20) d[0] |= 0x0c;
			if (c & 0x10) d[0] |= 0x03;
			if (c & 0x08) d[1] |= 0xc0;
			if (c & 0x04) d[1] |= 0x30;
			if (c & 0x02) d[1] |= 0x0c;
			if (c & 0x01) d[1] |= 0x03;
		}
		s += p->bytesPerWidth;
	}
	*glyphWidth = p->glyphWidths[i] * 2;
	return glyph;
}

static u_char *createHalfWidthGlyph(TFont *p, uint16_t ucs2,
				    u_short *glyphWidth)
{
	int i, x, y;
	u_char *glyph, *s, *d, *cp, c;

	assert(initialized);
	assert(p != NULL);
	assert(glyphWidth != NULL);

	i = codeToIndex(p, ucs2);
	if (i == -1)
		return NULL; /* out of range */
	if (p->width == p->glyphWidths[i])
		return NULL; /* halfwidth */
	glyph = calloc(p->bytesPerWidth * p->height, sizeof(u_char));
	if (glyph == NULL)
		err(1, "calloc()");
	s = p->glyphs[i];
	for (y = 0; y < p->height; y++) {
		cp = s;
		d = glyph + (y * p->bytesPerWidth);
		for (x = p->glyphWidths[i]; x >= 16; x -= 16) {
			c = *cp++;
			if ((c & 0x80) || (c & 0x40)) *d |= 0x80;
			if ((c & 0x20) || (c & 0x10)) *d |= 0x40;
			if ((c & 0x08) || (c & 0x04)) *d |= 0x20;
			if ((c & 0x02) || (c & 0x01)) *d |= 0x10;
			c = *cp++;
			if ((c & 0x80) || (c & 0x40)) *d |= 0x08;
			if ((c & 0x20) || (c & 0x10)) *d |= 0x04;
			if ((c & 0x08) || (c & 0x04)) *d |= 0x02;
			if ((c & 0x02) || (c & 0x01)) *d |= 0x01;
			d++;
		}
		if (x > 0) {
			c = *cp++;
			if ((c & 0x80) || (c & 0x40)) *d |= 0x80;
			if ((c & 0x20) || (c & 0x10)) *d |= 0x40;
			if ((c & 0x08) || (c & 0x04)) *d |= 0x20;
			if ((c & 0x02) || (c & 0x01)) *d |= 0x10;
		}
		if (x > 8) {
			c = *cp++;
			if ((c & 0x80) || (c & 0x40)) *d |= 0x08;
			if ((c & 0x20) || (c & 0x10)) *d |= 0x04;
			if ((c & 0x08) || (c & 0x04)) *d |= 0x02;
			if ((c & 0x02) || (c & 0x01)) *d |= 0x01;
		}
		s += p->bytesPerWidth;
	}
	*glyphWidth = (p->glyphWidths[i] / 2);
	return glyph;
}

static void createUnifontGlyph(TFont *p)
{
	uint16_t ucs2;
	u_char *halfWidthGlyph, *fullWidthGlyph;
	u_short halfWidthGlyphWidth, fullWidthGlyphWidth;
	bool ambiguous;

	assert(initialized);
	assert(p != NULL);

	if (p->half != FONT_HALF_UNI)
		return; /* font is not unifont */
	if (!font_isLoaded(p))
		return; /* unifont is not loaded */
	halfWidthGlyph = fullWidthGlyph = NULL;
	halfWidthGlyphWidth = fullWidthGlyphWidth = 0;
	for (ucs2 = 0x0000; ucs2 != 0xffff; ucs2++) {
		if (isFullWidth(ucs2)) {
			halfWidthGlyph = createFullWidthGlyph(p, ucs2,
							&halfWidthGlyphWidth);
			fullWidthGlyph = NULL;
			fullWidthGlyphWidth = 0;
			ambiguous = false;
		} else if (isHalfWidth(ucs2)) {
			halfWidthGlyph = createHalfWidthGlyph(p, ucs2,
							&halfWidthGlyphWidth);
			fullWidthGlyph = NULL;
			fullWidthGlyphWidth = 0;
			ambiguous = false;
		} else if (isAmbiguous(ucs2)) {
			halfWidthGlyph = createHalfWidthGlyph(p, ucs2,
							&halfWidthGlyphWidth);
			fullWidthGlyph = createFullWidthGlyph(p, ucs2,
							&fullWidthGlyphWidth);
			ambiguous = true;
		} else
			continue;
		if (halfWidthGlyph != NULL || fullWidthGlyph != NULL)
			unifontGlyphList_add(ucs2, ambiguous,
					     halfWidthGlyph,
					     halfWidthGlyphWidth,
					     fullWidthGlyph,
					     fullWidthGlyphWidth);
	}
	saveUnifontGlyph(p);
}

static void saveUnifontGlyph(TFont *p)
{
	int i;
	struct unifontGlyphList *current;

	assert(initialized);
	assert(p != NULL);

	if (p->half != FONT_HALF_UNI)
		return; /* font is not unifont */
	if (!font_isLoaded(p))
		return; /* unifont is not loaded */
	for (current = unifontGlyphList; current != NULL;
	     current = current->next) {
		i = codeToIndex(p, current->ucs2);
		if (i == -1)
			return; /* out of range */
		if (current->glyph == NULL) {
			current->glyph = p->glyphs[i];
			current->glyphWidth = p->glyphWidths[i];
		}
	}
}

static void restoreUnifontGlyph(TFont *p)
{
	int i;
	struct unifontGlyphList *current;

	assert(initialized);
	assert(p != NULL);

	if (p->half != FONT_HALF_UNI)
		return; /* font is not unifont */
	if (!font_isLoaded(p))
		return; /* unifont is not loaded */
	for (current = unifontGlyphList; current != NULL;
	     current = current->next) {
		i = codeToIndex(p, current->ucs2);
		if (i == -1)
			return; /* out of range */
		if (current->glyph != NULL) {
			p->glyphs[i] = current->glyph;
			p->glyphWidths[i] = current->glyphWidth;
		}
	}
}

void font_unifontGlyph(TFont *p, int ambiguousWidth)
{
	int i;
	struct unifontGlyphList *current;

	assert(initialized);
	assert(p != NULL);

	if (p->half != FONT_HALF_UNI)
		return; /* font is not unifont */
	if (!font_isLoaded(p))
		return; /* unifont is not loaded */
	restoreUnifontGlyph(p);
	if (ambiguousWidth != 1 && ambiguousWidth != 2)
		return;
	for (current = unifontGlyphList; current != NULL;
	     current = current->next) {
		i = codeToIndex(p, current->ucs2);
		if (i == -1)
			return; /* out of range */
		if (current->ambiguous) {
			if (ambiguousWidth == 1 &&
			    current->halfWidthGlyph != NULL) {
				p->glyphs[i] = current->halfWidthGlyph;
				p->glyphWidths[i] =
						current->halfWidthGlyphWidth;
			} else if (ambiguousWidth == 2 &&
				   current->fullWidthGlyph != NULL) {
				p->glyphs[i] = current->fullWidthGlyph;
				p->glyphWidths[i] =
						current->fullWidthGlyphWidth;
			}
		} else {
			if (current->halfWidthGlyph != NULL) {
				p->glyphs[i] = current->halfWidthGlyph;
				p->glyphWidths[i] =
						current->halfWidthGlyphWidth;
			} else if (current->fullWidthGlyph != NULL) {
				p->glyphs[i] = current->fullWidthGlyph;
				p->glyphWidths[i] =
						current->fullWidthGlyphWidth;
			}
		}
	}
}
#endif

static void underlineGlyph_initialize(void)
{
	underlineGlyph.bytesPerWidth = 0;
	underlineGlyph.bytesPerChar  = 0;
	underlineGlyph.glyph = NULL;
}

static void createUnderlineGlyph(void)
{
	u_short width, height;

	width  = gFontsWidth * 2;
	height = gFontsHeight;
	if ((width % 8) != 0)
		width = width + (8 - (width % 8));
	underlineGlyph.bytesPerWidth = width / 8;
	underlineGlyph.bytesPerChar  = underlineGlyph.bytesPerWidth * height;
	underlineGlyph.glyph = malloc(underlineGlyph.bytesPerChar);
	if (underlineGlyph.glyph == NULL)
		err(1, "malloc()");
	memset(underlineGlyph.glyph, 0x00, underlineGlyph.bytesPerChar);
	memset(underlineGlyph.glyph + underlineGlyph.bytesPerChar -
			underlineGlyph.bytesPerWidth,
	       0xff, underlineGlyph.bytesPerWidth);
}

static void underlineGlyph_finalize(void)
{
	underlineGlyph.bytesPerWidth = 0;
	underlineGlyph.bytesPerChar  = 0;
	if (underlineGlyph.glyph != NULL) {
		free(underlineGlyph.glyph);
		underlineGlyph.glyph = NULL;
	}
}

void font_draw(TFont *p, uint16_t code, uint8_t foregroundColor,
	       uint8_t backgroundColor, u_int x, u_int y,
	       bool underline, bool doubleColumn)
{
	u_int fontX, fontY, fontWidth, fontHeight, shadowWidth, shadowHeight;
	const u_char *glyph;
	u_short glyphWidth;

	assert(initialized);
	assert(p != NULL);

	fontX = gFontsWidth * x;
	fontY = gFontsHeight * y;
	fontWidth = doubleColumn ? gFontsWidth * 2 : gFontsWidth;
	fontHeight = gFontsHeight;
	gFramebuffer.accessor.fill(&gFramebuffer,
				   fontX, fontY,
				   fontWidth, fontHeight,
				   backgroundColor);
	if (code == 0x00)
		return;
	glyph = p->getGlyph(p, code, &glyphWidth);
	if (font.effect == FONT_EFFECT_SHADOW &&
	    glyphWidth > 0 && p->height > 0 && backgroundColor == 0) {
		shadowWidth  = (fontWidth > glyphWidth) ?
			       glyphWidth : glyphWidth - 1;
		shadowHeight = (fontHeight > p->height) ?
			       p->height : p->height - 1;
		gFramebuffer.accessor.overlay(&gFramebuffer,
					      glyph,
					      p->bytesPerWidth,
					      fontX + 1, fontY + 1,
					      shadowWidth, shadowHeight,
					      font.shadowColor);
	}
	gFramebuffer.accessor.overlay(&gFramebuffer,
				      glyph,
				      p->bytesPerWidth,
				      fontX, fontY,
				      glyphWidth, p->height,
				      foregroundColor);
	if (underline)
		gFramebuffer.accessor.overlay(&gFramebuffer,
					      underlineGlyph.glyph,
					      underlineGlyph.bytesPerWidth,
					      fontX, fontY,
					      fontWidth, fontHeight,
					      foregroundColor);
}

