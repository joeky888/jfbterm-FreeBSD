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

#if defined (__linux__) || defined (__NetBSD__)
#define LIBICONV_PLUG
#endif

#include <sys/ioctl.h>
#include <sys/types.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "bell.h"
#include "csv.h"
#include "cursor.h"
#include "font.h"
#include "framebuffer.h"
#include "mouse.h"
#include "palette.h"
#include "sequence.h"
#include "term.h"
#include "utilities.h"
#include "vterm.h"
#include "vtermlow.h"

static bool parse_encoding(const char *encoding, int idx[6]);
static void vterm_switch_to_ISO2022(TVterm *p);
static bool vterm_is_ISO2022(TVterm *p);
#ifdef ENABLE_UTF8
static int get_UTF8_index(const char *encoding);
static void vterm_switch_to_UTF8(TVterm *p);
#endif
#ifdef ENABLE_OTHER_CODING_SYSTEM
static void codingSystem_initialize(TCodingSystem *codingSystem);
static void codingSystem_finalize(TCodingSystem *codingSystem);
static bool parse_otherCS(const char *encoding, TCodingSystem *codingSystem);
static void vterm_finish_otherCS(TVterm *p);
static bool isCJKEncoding(const char *name);
static bool vterm_switch_to_otherCS(TVterm *p, TCodingSystem *codingSystem);
#endif
static void vterm_set_default_encoding(TVterm *p, const char *encoding);
static void vterm_set_default_invoke_and_designate(TVterm *p);
static void vterm_push_current_pen(TVterm *p, bool flag);
static void vterm_pop_pen_and_set_current_pen(TVterm *p, bool flag);
static inline bool IS_GL_AREA(TVterm *p, u_char c);
static inline bool IS_GR_AREA(TVterm *p, u_char c);
static inline void INSERT_N_CHARS_IF_NEEDED(TVterm *p, int n);
static inline void SET_WARP_FLAG_IF_NEEDED(TVterm *p);
static int vterm_put_normal_char(TVterm *p, u_char c);
#ifdef ENABLE_UTF8
static int vterm_put_uchar(TVterm *p, uint16_t code);
#endif
static bool vterm_iso_C0_set(TVterm *p, u_char c);
static bool vterm_iso_C1_set(TVterm *p, u_char c);
#ifdef ENABLE_UTF8
static int vterm_put_utf8_char(TVterm *p, u_char c);
#endif
#ifdef ENABLE_OTHER_CODING_SYSTEM
static int vterm_put_otherCS_char(TVterm *p, u_char c);
#endif
static void vterm_esc_start(TVterm *p, u_char c);
static void vterm_esc_set_attr(TVterm *p, int value);
static void vterm_set_mode(TVterm *p, int value, bool question, bool flag);
static void vterm_esc_report(TVterm *p, u_char c, int value);
static void vterm_set_region(TVterm *p, int ymin, int ymax);
static void vterm_set_window_size(TVterm *p);
static void vterm_esc_status_line(TVterm *p, u_char c);
static void vterm_esc_bracket(TVterm *p, u_char c);
static void vterm_esc_rbracket(TVterm *p, u_char c);
static void vterm_invoke_gx(TVterm *p, TFontSpec *fs, u_int n);
static void vterm_re_invoke_gx(TVterm *p, TFontSpec *fs);
static void vterm_esc_designate_font(TVterm *p, u_char c);
static void vterm_esc_designate_otherCS(TVterm *p, u_char c);
static void vterm_esc_traditional_multibyte_fix(TVterm *p, u_char c);

void vterm_initialize(TVterm *p, TTerm *term, TCaps *caps, u_int history,
		      u_int cols, u_int rows, const char *encoding,
		      int ambiguous)
{
	p->term = term;
	p->caps = caps;
	p->history = (history == 0 || history >= rows) ? history : rows;
	p->historyTop = 0;
	p->top = 0;
	p->tab = 8;
	p->cols = cols;
	p->cols4 = (cols + 7) & ~3;
	p->rows = rows;
	p->tsize = (p->history + p->rows) * p->cols4;
	p->xmax = p->cols;
	p->ymin = 0;
	p->ymax = p->rows;
	p->scroll = 0;
	pen_initialize(&(p->pen));
	p->savedPen = NULL;
	p->savedPenSL = NULL;
	p->statusLine = VTERM_STATUS_LINE_NONE;
	p->mouseTracking = VTERM_MOUSE_TRACKING_NONE;
	p->gDefaultL = 0;
	p->gDefaultR = 1;
	p->gDefaultIdx[0] = 0;                  /* G0 <== ASCII */
	p->gDefaultIdx[1] = 1;                  /* G1 <== JIS X 0208 */
	p->gDefaultIdx[2] = 0;                  /* G2 <== ASCII */
	p->gDefaultIdx[3] = 0;                  /* G3 <== ASCII */
	p->escSignature = 0;
	p->escGn = 0;
	p->gIdx[0] = 0;
	p->gIdx[1] = 0;
	p->gIdx[2] = 0;
	p->gIdx[3] = 0;
#ifdef ENABLE_UTF8
	p->utf8DefaultIdx = 0;
	p->utf8Idx = 0;
	p->utf8remain = 0;
	p->ucs2 = 0;
	p->ambiguous = ambiguous;
#endif
#ifdef ENABLE_OTHER_CODING_SYSTEM
	p->otherCS = NULL;
#endif
	p->dbcsLeadByte = '\0';
	p->dbcsHalf = FONT_HALF_LEFT;
	p->dbcsIdx = 0;
	p->altCs = false;
	p->wrap = false;
	p->insert = false;
	p->cursor = false;
	p->active = true;
	p->textClear = true;
	p->esc = NULL;
	p->text = calloc(p->tsize, sizeof(uint16_t));
	if (p->text == NULL)
		err(1, "calloc()");
	p->fontIndex = calloc(p->tsize, sizeof(u_int));
	if (p->fontIndex == NULL)
		err(1, "calloc()");
	p->rawText = calloc(p->tsize, sizeof(uint16_t));
	if (p->rawText == NULL)
		err(1, "calloc()");
	p->foreground = calloc(p->tsize, sizeof(uint8_t));
	if (p->foreground == NULL)
		err(1, "calloc()");
	p->background = calloc(p->tsize, sizeof(uint8_t));
	if (p->background == NULL)
		err(1, "calloc()");
	p->flag = calloc(p->tsize, sizeof(uint8_t));
	if (p->flag == NULL)
		err(1, "calloc()");
	vterm_set_default_encoding(p, encoding);
	vterm_set_default_invoke_and_designate(p);
	vterm_set_window_size(p);
}

void vterm_finalize(TVterm *p)
{
	p->active = false;
	pen_finalize(&(p->pen));
	if (p->savedPen != NULL) {
		pen_finalize(p->savedPen);
		free(p->savedPen);
		p->savedPen = NULL;
	}
	if (p->savedPenSL != NULL) {
		pen_finalize(p->savedPenSL);
		free(p->savedPenSL);
		p->savedPenSL = NULL;
	}
#ifdef ENABLE_OTHER_CODING_SYSTEM
	if (vterm_is_otherCS(p)) {
		codingSystem_finalize(p->otherCS);
		free(p->otherCS);
		p->otherCS = NULL;
	}
#endif
	if (p->text != NULL) {
		free(p->text);
		p->text = NULL;
	}
	if (p->fontIndex != NULL) {
		free(p->fontIndex);
		p->fontIndex = NULL;
	}
	if (p->rawText != NULL) {
		free(p->rawText);
		p->rawText = NULL;
	}
	if (p->foreground != NULL) {
		free(p->foreground);
		p->foreground = NULL;
	}
	if (p->background != NULL) {
		free(p->background);
		p->background = NULL;
	}
	if (p->flag != NULL) {
		free(p->flag);
		p->flag = NULL;
	}
}

static bool parse_encoding(const char *encoding, int idx[6])
{
	TCsv csv;
	const char *token;
	int i, n;

	assert(encoding != NULL);

	/* GL,GR,G0,G1,G2,G3 */
	csv_initialize(&csv, encoding);
	if (csv.tokens != 6) {
		csv_finalize(&csv);
		return false; /* bad format */
	}
	/* GL, GR */
	for (i = 0; i < 2; i++) {
		token = csv_getToken(&csv);
		assert(token != NULL);
		n = lookup(true, token, "G0", "G1", "G2", "G3", (char *)NULL);
		if (n == -1) n = i;
		idx[i] = n;
	}
	/* G0 .. G3 */
	for (i = 0; i < 4; i++) {
		token = csv_getToken(&csv);
		assert(token != NULL);
		n = font_getIndexByName(token);
		if (n == -1) n = 0;
		idx[i + 2] = n;
	}
	csv_finalize(&csv);
	return true;
}

static void vterm_switch_to_ISO2022(TVterm *p)
{
#ifdef ENABLE_OTHER_CODING_SYSTEM
	vterm_finish_otherCS(p);
#endif
#ifdef ENABLE_UTF8
	p->utf8Idx = 0;
	p->utf8remain = 0;
	p->ucs2 = 0;
#endif
}

static bool vterm_is_ISO2022(TVterm *p)
{
#ifdef ENABLE_OTHER_CODING_SYSTEM
	if (vterm_is_otherCS(p))
		return false;
#endif
#ifdef ENABLE_UTF8
	if (vterm_is_UTF8(p))
		return false;
#endif
	return true;
}

#ifdef ENABLE_UTF8
static int get_UTF8_index(const char *encoding)
{
	int i;
	TCsv csv;
	const char *token;

	assert(encoding != NULL);

	/* UTF-8,<fontsetname> */
	csv_initialize(&csv, encoding);
	if (csv.tokens != 2) {
		csv_finalize(&csv);
		return 0; /* bad format */
	}
	/* "UTF-8" */
	token = csv_getToken(&csv);
	assert(token != NULL);
	if (strcasecmp(token, "UTF-8") != 0) {
		csv_finalize(&csv);
		return 0; /* bad format */
	}
	/* <fontsetname> */
	token = csv_getToken(&csv);
	assert(token != NULL);
	i = font_getIndexByName(token);
	if (i == -1 || !font_isLoaded(&(gFonts[i]))) {
		csv_finalize(&csv);
		return 0;
	}
	csv_finalize(&csv);
	return i;
}

static void vterm_switch_to_UTF8(TVterm *p)
{
	const char *config;
	int i;

#ifdef ENABLE_OTHER_CODING_SYSTEM
	vterm_finish_otherCS(p);
#endif
	if (p->utf8DefaultIdx == 0) {
		config = caps_findEntry(p->caps, "encoding.", "UTF-8");
		if (config != NULL)
			p->utf8DefaultIdx = get_UTF8_index(config);
	}
	p->utf8Idx = p->utf8DefaultIdx;
	p->utf8remain = 0;
	p->ucs2 = 0;
	if ((i = font_getIndexByName("iso10646.1")) != -1)
		font_unifontGlyph(&(gFonts[i]), p->ambiguous);
}

bool vterm_is_UTF8(TVterm *p)
{
	return p->utf8Idx != 0;
}
#endif

#ifdef ENABLE_OTHER_CODING_SYSTEM
static void codingSystem_initialize(TCodingSystem *codingSystem)
{
	assert(codingSystem != NULL);

	codingSystem->cd = (iconv_t)-1;
	codingSystem->fromcode = NULL;
	codingSystem->tocode = NULL;
	memset(codingSystem->inbuf, '\0', sizeof(codingSystem->inbuf));
	codingSystem->inbuflen = 0;
	memset(codingSystem->outbuf, '\0', sizeof(codingSystem->outbuf));
	codingSystem->gSavedL = 0;
	codingSystem->gSavedR = 0;
	codingSystem->gSavedIdx[0] = 0;
	codingSystem->gSavedIdx[1] = 0;
	codingSystem->gSavedIdx[2] = 0;
	codingSystem->gSavedIdx[3] = 0;
	codingSystem->utf8SavedIdx = 0;
}

static void codingSystem_finalize(TCodingSystem *codingSystem)
{
	assert(codingSystem != NULL);

	if (codingSystem->cd != (iconv_t)-1) {
		iconv_close(codingSystem->cd);
		codingSystem->cd = (iconv_t)-1;
	}
	if (codingSystem->fromcode != NULL) {
		free(codingSystem->fromcode);
		codingSystem->fromcode = NULL;
	}
	if (codingSystem->tocode != NULL) {
		free(codingSystem->tocode);
		codingSystem->tocode = NULL;
	}
}

static bool parse_otherCS(const char *encoding, TCodingSystem *codingSystem)
{
	TCsv csv;
	const char *token;

	assert(encoding != NULL);
	assert(codingSystem != NULL);

	/* other,<coding-system>,iconv,<std-coding-system> */
	csv_initialize(&csv, encoding);
	if (csv.tokens != 4) {
		csv_finalize(&csv);
		return false; /* bad format */
	}
	/* "other" */
	token = csv_getToken(&csv);
	assert(token != NULL);
	if (strcasecmp(token, "other") != 0) {
		csv_finalize(&csv);
		return false; /* bad format */
	}
	/* <coding-system> */
	token = csv_getToken(&csv);
	assert(token != NULL);
	codingSystem->fromcode = strdup(token);
	if (codingSystem->fromcode == NULL)
		err(1, "strdup()");
	/* "iconv" */
	token = csv_getToken(&csv);
	assert(token != NULL);
	if (strcasecmp(token, "iconv") != 0) {
		free(codingSystem->fromcode);
		codingSystem->fromcode = NULL;
		csv_finalize(&csv);
		return false; /* bad format */
	}
	/* <std-coding-system> */
	token = csv_getToken(&csv);
	assert(token != NULL);
	codingSystem->tocode = strdup(token);
	if (codingSystem->tocode == NULL)
		err(1, "strdup()");
	csv_finalize(&csv);
	return true;
}

static void vterm_finish_otherCS(TVterm *p)
{
	int i;

	/* XXX restore ISO-2022 state? */
	if (vterm_is_otherCS(p)) {
		/* restore designate/invoke state */
		for (i = 0; i < 4; i++)
			p->gIdx[i] = p->otherCS->gSavedIdx[i];
		vterm_invoke_gx(p, &(p->gl), p->otherCS->gSavedL);
		vterm_invoke_gx(p, &(p->gr), p->otherCS->gSavedR);
		p->tgl = p->gl; p->tgr = p->gr;
		p->dbcsLeadByte = '\0';
		p->utf8Idx = p->otherCS->utf8SavedIdx;
		p->utf8remain = 0;
		p->ucs2 = 0;
		codingSystem_finalize(p->otherCS);
		free(p->otherCS);
		p->otherCS = NULL;
	}
}

static bool isCJKEncoding(const char *name)
{
	static const char *cjkEncodings[] = {
#if defined (__linux__)
		/* libc6 2.3.6.ds1-13et */
		/* Chinese encodings */
		"GB_1988-80", "ISO-IR-57", "CN", "ISO646-CN", "CSISO58GB1988", "GB_198880",
		"BIG5", "BIG-FIVE", "BIGFIVE", "BIG-5", "CN-BIG5", "CP950",
		"BIG5HKSCS", "BIG5-HKSCS",
		"EUC-CN", "EUCCN", "GB2312", "csGB2312", "CN-GB",
		"GBK", "GB13000", "CP936", "MS936", "WINDOWS-936",
		"EUC-TW", "EUCTW", "OSF0005000a",
		"ISO-2022-CN", "CSISO2022CN", "ISO2022CN",
		"ISO-2022-CN-EXT", "ISO2022CNEXT",
		"GB18030",
		/* Japanese encodings */
		"JIS_C6220-1969-RO", "ISO-IR-14", "JP", "ISO646-JP", "CSISO14JISC6220RO", "JIS_C62201969RO",
		"JIS_C6229-1984-B", "ISO-IR-92", "ISO646-JP-OCR-B", "JP-OCR-B", "CSISO92JISC62991984B", "JIS_C62291984B",
		"SJIS", "SHIFT-JIS", "SHIFT_JIS", "MS_KANJI", "CSSHIFTJIS",
		"CP932", "WINDOWS-31J", "MS932", "SJIS-OPEN", "SJIS-WIN", "CSWINDOWS31J",
		"IBM281", "EBCDIC-JP-E", "CP281", "CSIBM281",
		"IBM290", "CP290", "EBCDIC-JP-KANA", "CSIBM290", "OSF10020122",
		"IBM932", "IBM-932", "CSIBM932",
		"IBM943", "IBM-943", "CSIBM943",
		"EUC-JP-MS", "EUCJP-MS", "EUCJP-OPEN", "EUCJP-WIN",
		"EUC-JP", "EUCJP", "CSEUCPKDFMTJAPANESE", "OSF00030010", "UJIS",
		"ISO-2022-JP", "CSISO2022JP", "ISO2022JP",
		"ISO-2022-JP-2", "CSISO2022JP2", "ISO2022JP2",
		"ISO-2022-JP-3",
		"EUC-JISX0213",
		"Shift_JISX0213",
 		/* Korean encodings */
		"KSC5636", "ISO646-KR", "CSKSC5636",
		"EUC-KR", "EUCKR", "CSEUCKR", "OSF0004000a",
		"UHC", "MSCP949", "CP949", "OSF100203B5",
		"JOHAB", "MSCP1361", "CP1361",
		"ISO-2022-KR", "CSISO2022KR", "ISO2022KR",
#elif defined (__FreeBSD__)
		/* libiconv-1.11_1 (converters/libiconv) */
		/* Chinese encodings */
		"BIG5", "BIG5-HKSCS", "BIG5-HKSCS:1999", "BIG5-HKSCS:2001",
		"CP936", "CP950", "EUC-CN", "EUC-TW", "GB18030", "GBK",
		"GB_1988-80", "GB_2312-80", "HZ", "ISO-2022-CN",
		"ISO-2022-CN-EXT", "ISO-IR-165",
		/* EXTRA_ENCODINGS */
		"DEC-HANYU", "BIG5-2003",
		/* Japanese encodings */
		"CP932", "EUC-JP", "ISO-2022-JP", "ISO-2022-JP-1",
		"ISO-2022-JP-2", "JIS_C6220-1969-RO", "JIS_X0201", "JIS_X0208",
		"JIS_X0212", "SHIFT_JIS",
		/* EXTRA_ENCODINGS */
		"DEC-KANJI", "EUC-JISX0213", "SHIFT_JISX0213", "ISO-2022-JP-3",
		/* EXTRA_PATCHES */
		"EUCJP-MS", "WINDOWS-31J",
 		/* Korean encodings */
		"CP949", "EUC-KR", "ISO-2022-KR", "JOHAB",
#elif defined (__NetBSD__)
		/* NetBSD 4.0.1 citrus iconv */
		/* Chinese encodings */
		"Big5-E", "big5e", "big-5e",
		"Big5-ETen", "big5", "big5eten",
		"Big5-HKSCS", "big5hkscs",
		"Big5-Plus", "big5+", "big-5+",
		"cp936", "mscp936",
		"cp950", "mscp950",
		"DECHanyu", "dec-hanyu", "dec_hanyu",
		"euc-cn", "euccn", "gb2312",
		"euc-tw", "euctw", "cns11643",
		"gb12345",
		"gb18030",
		"gbk",
		"iso-2022-cn", "iso2022-cn",
		"iso-2022-cn-ext", "iso2022-cnext",
		"HZ", "HZ-GB2312",
		/* Japanese encodings */
		"cp281", "ibm281", "ebcdic-jp-e",
		"cp290", "ibm290", "ebcdic-jp-kana",
		"cp932", "mscp932",
		"cp942", "ibm942",
		"cp942c", "ibm942c",
		"cp943", "ibm943",
		"cp943c", "ibm943c",
		"cp50220", "mscp50220", "windows-50220",
		"cp50221", "mscp50221", "windows-50221",
		"cp50222", "mscp50222", "windows-50222",
		"cp51932", "mscp51932", "windows-51932",
		"euc-jp", "eucjp",
		"euc-jp-ms", "eucjp-ms",
		"euc-jp-2004", "euc-jisx0213",
		"iso-2022-jp", "iso2022-jp",
		"iso-2022-jp-1", "iso2022-jp1",
		"iso-2022-jp-2", "iso2022-jp2",
		"iso-2022-jp-2004", "iso2022-jp2004", "iso-2022-jp-3", "iso2022-jp3",
		"iso646-jp", "iso-ir-14", "jis_c6220-1969-ro", "jp",
		"shift_jis", "sjis",
		"Shift_JIS-2004", "shift_jisx0213",
		"iso646-jp-ocr-b", "jis_c6229-1984-b", "iso-ir-92", "jp-ocr-b",
 		/* Korean encodings */
		"cp949", "mscp949", "uhc",
		"euc-kr", "euckr",
		"iso-2022-kr", "iso2022-kr",
		"iso646-kr", "ksc5636",
		"JOHAB", "CP1361",
#elif defined (__OpenBSD__)
		/* libiconv-1.12 (converters/libiconv) */
		/* Chinese encodings */
		"BIG5", "BIG5-2003", "BIG5-HKSCS", "BIG5-HKSCS:1999",
		"BIG5-HKSCS:2001", "CP936", "CP950", "DEC-HANYU", "EUC-CN",
		"EUC-TW", "GB18030", "GBK", "GB_1988-80", "GB_2312-80", "HZ",
		"ISO-2022-CN", "ISO-2022-CN-EXT", "ISO-IR-165",
		/* Japanese encodings */
		"CP932", "DEC-KANJI", "EUC-JISX0213", "EUC-JP", "ISO-2022-JP",
		"ISO-2022-JP-1", "ISO-2022-JP-2", "ISO-2022-JP-3",
		"JIS_C6220-1969-RO", "JIS_X0201", "JIS_X0208", "JIS_X0212",
		"SHIFT_JIS", "SHIFT_JISX0213",
		 /* Korean encodings */
		"CP949", "EUC-KR", "ISO-2022-KR", "JOHAB", "KSC_5601",
#else
	#error not implement
#endif
		NULL
	};
	int i;

	assert(name != NULL);

	for (i = 0; cjkEncodings[i] != NULL; i++) {
#if defined (__FreeBSD__) || defined (__OpenBSD__)
		if (strcasecmp(cjkEncodings[i],
			       iconv_canonicalize(name)) == 0)
			return true;
#else
		if (strcasecmp(cjkEncodings[i], name) == 0)
			return true;
#endif
	}
	return false;
}

static bool vterm_switch_to_otherCS(TVterm *p, TCodingSystem *codingSystem)
{
	const char *config;
	char *tocode;
	int i, idx[6];

	tocode = codingSystem->tocode;
	vterm_finish_otherCS(p);
	/* save current designate/invoke */
	codingSystem->gSavedL = p->gl.invokedGn;
	codingSystem->gSavedR = p->gr.invokedGn;
	for (i = 0; i < 4; i++)
		codingSystem->gSavedIdx[i] = p->gIdx[i];
	codingSystem->utf8SavedIdx = p->utf8Idx;

 retry:
	if (strcasecmp(codingSystem->tocode, "UTF-8") == 0) {
		vterm_switch_to_UTF8(p);
		tocode = "UCS-2BE";
	} else {
		config = caps_findEntry(p->caps, "encoding.",
					codingSystem->tocode);
		if (config == NULL) {
			free(codingSystem->tocode);
			codingSystem->tocode = strdup("UTF-8");
			if (codingSystem->tocode == NULL)
				err(1, "strdup()");
			goto retry;
		}
		if (!parse_encoding(config, idx)) {
			free(codingSystem->tocode);
			codingSystem->tocode = strdup("UTF-8");
			if (codingSystem->tocode == NULL)
				err(1, "strdup()");
			goto retry;
		}
		for (i = 0; i < 4; i++)
			p->gIdx[i] = idx[2 + i];
		vterm_invoke_gx(p, &(p->gl), idx[0]);
		vterm_invoke_gx(p, &(p->gr), idx[1]);
		p->tgl = p->gl; p->tgr = p->gr;
		p->dbcsLeadByte = '\0';
	}
	codingSystem->cd = iconv_open(tocode, codingSystem->fromcode);
	if (codingSystem->cd == (iconv_t)-1) {
		if (strcasecmp(codingSystem->tocode, "UTF-8") != 0) {
			free(codingSystem->tocode);
			codingSystem->tocode = strdup("UTF-8");
			if (codingSystem->tocode == NULL)
				err(1, "strdup()");
			goto retry;
		}
		return false;
	}
	p->otherCS = codingSystem;
	if ((i = font_getIndexByName("iso10646.1")) != -1) {
		if (isCJKEncoding(p->otherCS->fromcode))
			font_unifontGlyph(&(gFonts[i]), 2);
		else
			font_unifontGlyph(&(gFonts[i]), p->ambiguous);
	}
	return true;
}

bool vterm_is_otherCS(TVterm *p)
{
	return p->otherCS != NULL;
}

char *vterm_format_otherCS(const char *encoding)
{
	char *other;
	size_t len;

	assert(encoding != NULL);

	if (strcasecmp(encoding, "UTF-8") != 0) {
		len = strlen("other,,iconv,UTF-8") + strlen(encoding) + 1;
		other = malloc(len);
		if (other == NULL)
			err(1, "malloc()");
		snprintf(other, len, "other,%s,iconv,UTF-8", encoding);
		other[len - 1] = '\0';
	} else
		other = strdup("UTF-8,iso10646.1");
	return other;
}
#endif

static void vterm_set_default_encoding(TVterm *p, const char *encoding)
{
	const char *token;
	int i, idx[6];
	TCsv csv;

	assert(encoding != NULL);

	csv_initialize(&csv, encoding);
	/* get first token */
	token = csv_getToken(&csv);
#ifdef ENABLE_OTHER_CODING_SYSTEM
	if (csv.tokens == 4 && strcasecmp(token, "other") == 0) {
		/* other,<coding-system>,iconv,<std-coding-system> */
		TCodingSystem *codingSystem;
		codingSystem = malloc(sizeof(TCodingSystem));
		if (codingSystem == NULL)
			err(1, "malloc()");
		codingSystem_initialize(codingSystem);
		if (!parse_otherCS(encoding, codingSystem) ||
		    !vterm_switch_to_otherCS(p, codingSystem)) {
			codingSystem_finalize(codingSystem);
			free(codingSystem);
		}
		csv_finalize(&csv);
		return;
	}
#endif
#ifdef ENABLE_UTF8
	if (csv.tokens == 2 && strcasecmp(token, "UTF-8") == 0) {
		/* UTF-8,<fontsetname> */
		p->utf8DefaultIdx = get_UTF8_index(encoding);
		vterm_switch_to_UTF8(p);
		csv_finalize(&csv);
		return;
	}
#endif
	csv_finalize(&csv);
	/* ISO-2022 */
	if (parse_encoding(encoding, idx)) {
		/* GL */
		p->gDefaultL = idx[0];
		/* GR */
		p->gDefaultR = idx[1];
		/* G0 .. G3 */
		for (i = 0; i < 4; i++)
			p->gDefaultIdx[i] = idx[2 + i];
	}
}

static void vterm_set_default_invoke_and_designate(TVterm *p)
{
	/* G0 <== ASCII */
	p->gIdx[0] = p->gDefaultIdx[0];
	/* G1 <== JIS X 0208 */
	p->gIdx[1] = p->gDefaultIdx[1];
	/* G2 <== ASCII */
	p->gIdx[2] = p->gDefaultIdx[2];
	/* G3 <== ASCII */
	p->gIdx[3] = p->gDefaultIdx[3];
	/* GL <== G0 */
	vterm_invoke_gx(p, &(p->gl), p->gDefaultL);
	/* GR <== G1 */
	vterm_invoke_gx(p, &(p->gr), p->gDefaultR);
	/* Next char's GL <== GL */
	p->tgl = p->gl;
	/* Next char's GR <== GR */
	p->tgr = p->gr;
	p->dbcsLeadByte = '\0';
#ifdef ENABLE_UTF8
	p->utf8Idx = p->utf8DefaultIdx;
	p->utf8remain = 0;
	p->ucs2 = 0;
#endif
	p->altCs = false;
}

static void vterm_push_current_pen(TVterm *p, bool flag)
{
	TPen *pen;
	TPen **base;

	base = flag ? &(p->savedPen) : &(p->savedPenSL);
	pen = malloc(sizeof(TPen));
	if (pen == NULL)
		err(1, "malloc()");
	pen_initialize(pen);
	pen_copy(pen, &(p->pen));
	pen->prev = *base;
	*base = pen;
}

static void vterm_pop_pen_and_set_current_pen(TVterm *p, bool flag)
{
	TPen *pen;
	TPen **base;

	base = flag ? &(p->savedPen) : &(p->savedPenSL);
	pen = *base;
	if (pen == NULL)
		return;
	pen_copy(&(p->pen), pen);
	if (p->pen.y < p->ymin)
		p->pen.y = p->ymin;
	if (p->pen.y > p->ymax - 1)
		p->pen.y = p->ymax - 1;
	*base = pen->prev;
	free(pen);
}

static inline bool IS_GL_AREA(TVterm *p, u_char c)
{
	return (p->tgl.type & FONT_SIGNATURE_96CHAR) ?
	       (0x1f < c && c < 0x80) : (0x20 < c && c < 0x7f);
}

static inline bool IS_GR_AREA(TVterm *p, u_char c)
{
	return (p->tgr.type & FONT_SIGNATURE_96CHAR) ?
	       (0x9f < c) : (0xa0 < c && c < 0xff);
}

static inline void INSERT_N_CHARS_IF_NEEDED(TVterm *p, int n)
{
	if (p->insert)
		vterm_insert_n_chars(p, n);
}

static inline void SET_WARP_FLAG_IF_NEEDED(TVterm *p)
{
	if (p->pen.x == p->xmax - 1)
		p->wrap = true;
}

static int vterm_put_normal_char(TVterm *p, u_char c)
{
	if (p->pen.x == p->xmax) {
		p->wrap = true;
		p->pen.x--;
	}
	if (p->wrap) {
		p->pen.x -= p->xmax - 1;
		if (p->pen.y == p->ymax - 1)
			p->scroll++;
		else
			p->pen.y++;
		p->wrap = false;
		return -1;
	}
	if (p->dbcsLeadByte != '\0') {
		INSERT_N_CHARS_IF_NEEDED(p, 2);
		vterm_wput(p, p->dbcsIdx,
			   p->dbcsHalf == FONT_HALF_RIGHT ?
			   p->dbcsLeadByte | 0x80 : p->dbcsLeadByte & 0x7f,
			   p->dbcsHalf == FONT_HALF_RIGHT ?
			   c | 0x80 : c & 0x7f,
			   p->dbcsLeadByte, c);
		p->pen.x += 2;
		p->dbcsLeadByte = '\0';
		p->tgl = p->gl; p->tgr = p->gr;
	} else if (IS_GL_AREA(p, c)) {
		if (p->tgl.type & FONT_SIGNATURE_DOUBLE) {
			SET_WARP_FLAG_IF_NEEDED(p);
			p->dbcsLeadByte = c;
			p->dbcsHalf = p->tgl.half;
			p->dbcsIdx = p->tgl.idx;
		} else {
			INSERT_N_CHARS_IF_NEEDED(p, 1);
			vterm_sput(p, p->tgl.idx,
				   p->tgl.half == FONT_HALF_RIGHT ?
				   c | 0x80 : c, c);
			p->pen.x++;
			p->tgl = p->gl; p->tgr = p->gr;
		}
	} else if (IS_GR_AREA(p, c)) {
		if (p->tgr.type & FONT_SIGNATURE_DOUBLE) {
			SET_WARP_FLAG_IF_NEEDED(p);
			p->dbcsLeadByte = c;
			p->dbcsHalf = p->tgr.half;
			p->dbcsIdx = p->tgr.idx;
		} else {
			INSERT_N_CHARS_IF_NEEDED(p, 1);
			vterm_sput(p, p->tgr.idx,
				   p->tgr.half == FONT_HALF_LEFT ?
				   c & 0x7f : c, c);
			p->pen.x++;
			p->tgl = p->gl; p->tgr = p->gr;
		}
	} else if (c == 0x20) {
		INSERT_N_CHARS_IF_NEEDED(p, 1);
		vterm_sput(p, 0, 0x20, 0x20);
		p->pen.x++;
		p->tgl = p->gl; p->tgr = p->gr;
	}
	return 0;
}

#ifdef ENABLE_UTF8
static int vterm_put_uchar(TVterm *p, uint16_t code)
{
	TFont *font;
	u_short glyphWidth;

	font = &gFonts[p->utf8Idx];
	font->getGlyph(font, code, &glyphWidth);
	if (font->width != glyphWidth) {
		if (p->pen.x == p->xmax - 1) {
			p->wrap = true;
			p->pen.x -= 2;
		}
		if (p->wrap) {
			p->pen.x -= p->xmax - 1;
			if (p->pen.y == p->ymax - 1)
				p->scroll++;
			else
				p->pen.y++;
			p->wrap = false;
			return -1;
		}
	}
	if (p->pen.x == p->xmax) {
		p->wrap = true;
		p->pen.x--;
	}
	if (p->wrap) {
		p->pen.x -= p->xmax - 1;
		if (p->pen.y == p->ymax - 1)
			p->scroll++;
		else
			p->pen.y++;
		p->wrap = false;
		return -1;
	}
	if (font->width == glyphWidth) {
		INSERT_N_CHARS_IF_NEEDED(p, 1);
		vterm_uput1(p, p->utf8Idx, code, code);
		p->pen.x++;
	} else {
		INSERT_N_CHARS_IF_NEEDED(p, 2);
		vterm_uput2(p, p->utf8Idx, code, code);
		p->pen.x += 2;
	}
	p->utf8remain = 0;
	p->ucs2 = 0;
	return 0;
}
#endif

static bool vterm_iso_C0_set(TVterm *p, u_char c)
{
	switch (c) {
	case ISO_BEL:
		bell_execute(p);
		break;
	case ISO_BS:
		if (p->pen.x)
			p->pen.x--;
		p->wrap = false;
		break;
	case ISO_HT:
		p->pen.x += p->tab - (p->pen.x % p->tab);
		p->wrap = false;
		if (p->pen.x < p->xmax)
			break;
		p->pen.x -= p->xmax;
		/* FALLTHROUGH */
	case ISO_VT:
		/* FALLTHROUGH */
	case ISO_FF:
#ifdef ENABLE_OTHER_CODING_SYSTEM
		if (!vterm_is_otherCS(p))
			vterm_set_default_invoke_and_designate(p);
#else
		vterm_set_default_invoke_and_designate(p);
#endif
		/* FALLTHROUGH */
	case ISO_LF:
		p->wrap = false;
		if (p->pen.y == p->ymax - 1)
			p->scroll++;
		else
			p->pen.y++;
		break;
	case ISO_CR:
		p->pen.x = 0;
		p->wrap = false;
		break;
	case UNIVERSAL_ESC:
		p->esc = vterm_esc_start;
		p->escSignature = 0;
		p->escGn = 0;
		return true;
	case ISO_LS1:
		if (!vterm_is_ISO2022(p))
			break;
		/* GL <== G1 */
		vterm_invoke_gx(p, &(p->gl), 1);
		p->tgl = p->gl;
		return true;
	case ISO_LS0:
		if (!vterm_is_ISO2022(p))
			break;
		/* GL <== G0 */
		vterm_invoke_gx(p, &(p->gl), 0);
		p->tgl = p->gl;
		return true;
	default:
		break;
	}
	return false;
}

static bool vterm_iso_C1_set(TVterm *p, u_char c)
{
	switch (c) {
	case ISO_SS2: /* single shift 2 */
		/* GR <== G2 */
		vterm_invoke_gx(p, &(p->tgr), 2);
		return true;
	case ISO_SS3: /* single shift 3 */
		/* GR <== G3 */
		vterm_invoke_gx(p, &(p->tgr), 3);
		return true;
	default:
		break;
	}
	return false;
}

#ifdef ENABLE_UTF8
#define UTF8_CHECK_START(p) { \
		if ((p)->utf8remain != 0) { \
			(p)->ucs2 = 0; \
			(p)->utf8remain = 0; \
		} \
}

static int vterm_put_utf8_char(TVterm *p, u_char c)
{
	int rev;

	if (c < 0x7f) {
		UTF8_CHECK_START(p);
		p->ucs2 = c;
	} else if ((c & 0xc0) == 0x80) {
		if (p->utf8remain == 0)
			/* illegal UTF-8 sequence? */
			p->ucs2 = c;
		else {
			p->ucs2 <<= 6;
			p->ucs2 |= (c & 0x3f);
			p->utf8remain--;
			if (p->utf8remain > 0)
				return p->utf8remain;
		}
	} else if ((c & 0xe0) == 0xc0) {
		UTF8_CHECK_START(p);
		p->utf8remain = 1;
		p->ucs2 = (c & 0x1f);
		return p->utf8remain;
	} else if ((c & 0xf0) == 0xe0) {
		UTF8_CHECK_START(p);
		p->utf8remain = 2;
		p->ucs2 = (c & 0x0f);
		return p->utf8remain;
	} else {
		/* XXX: support UCS2 only */
	}
	if (p->altCs && p->ucs2 < 127 &&
	    IS_GL_AREA(p, (u_char)(p->ucs2 & 0xff)))
		rev = vterm_put_normal_char(p, (p->ucs2 & 0xff));
	else
		rev = vterm_put_uchar(p, p->ucs2);
	if (rev < 0) {
		p->ucs2 >>= 6;
		if (p->ucs2)
			p->utf8remain++;
	}
	return rev;
}
#endif

#ifdef ENABLE_OTHER_CODING_SYSTEM
static int vterm_put_otherCS_char(TVterm *p, u_char c)
{
	int rev;
#if defined (__GLIBC__)
	char *inbuf;
#else
	const char *inbuf;
#endif
	size_t inbytesleft;
	char *outbuf;
	size_t outbytesleft;
	size_t nbytes;
	int i;

	if (p->otherCS->inbuflen >= sizeof(p->otherCS->inbuf) - 1)
		/* XXX: too long? */
		p->otherCS->inbuflen = 0;
	p->otherCS->inbuf[p->otherCS->inbuflen] = c;
	p->otherCS->inbuflen++;

	inbuf = p->otherCS->inbuf;
	inbytesleft = p->otherCS->inbuflen;
	outbuf = p->otherCS->outbuf;
	outbytesleft = sizeof(p->otherCS->outbuf);

	if (iconv(p->otherCS->cd, &inbuf, &inbytesleft, &outbuf,
		  &outbytesleft) == -1) {
		switch (errno) {
		case E2BIG:
			p->otherCS->inbuflen = 0;
			return 0;
		case EILSEQ: /* illegal sequence */
			p->otherCS->inbuflen = 0;
			return 0;
		case EINVAL: /* incomplete multibyte */
			return 0;
		default:
			p->otherCS->inbuflen = 0;
			return 0;
		}
	}

	nbytes = sizeof(p->otherCS->outbuf) - outbytesleft;

	if (strcasecmp(p->otherCS->tocode, "UTF-8") == 0 && !p->altCs) {
		p->ucs2 = 0;
		for (i = 0; i < nbytes; i++) {
			p->ucs2 <<= 8;
			p->ucs2 |= (p->otherCS->outbuf[i] & 0xff);
		}
		rev = vterm_put_uchar(p, p->ucs2);
		if (rev < 0) {
			p->otherCS->inbuflen--;
			return rev;
		}
		p->otherCS->inbuflen = 0;
		return rev;
	} else {
		for (i = 0; i < nbytes; i++) {
			rev = vterm_put_normal_char(p, p->otherCS->outbuf[i]);
			if (rev < 0) {
				p->otherCS->inbuflen--;
				return rev;
			}
		}
		p->otherCS->inbuflen = 0;
		return 0;
	}
}
#endif

void vterm_emulate(TVterm *p, const u_char *buf, size_t nbytes)
{
	u_char c;
	int rev;

	while (nbytes-- > 0) {
		c = *(buf++);
		if (!c)
			continue;
		else if (p->esc)
			p->esc(p, c);
		else if (c < 0x20) {
			if (vterm_iso_C0_set(p, c))
				continue;
#ifdef ENABLE_OTHER_CODING_SYSTEM
		} else if (vterm_is_otherCS(p)) {
			rev = vterm_put_otherCS_char(p, c);
			if (rev >= 0)
				continue;
			else if (rev < 0) {
				nbytes -= rev;
				buf += rev;
			}
#endif
#ifdef ENABLE_UTF8
		} else if (vterm_is_UTF8(p)) {
			rev = vterm_put_utf8_char(p, c);
			if (rev >= 0)
				continue;
			else if (rev < 0) {
				nbytes -= rev;
				buf += rev;
			}
#endif
		} else if ((0x7f < c) && (c < 0xa0)) {
			if (vterm_iso_C1_set(p, c))
				continue;
		} else if (c == ISO_DEL) {
			/* nothing to do. */
		} else {
			rev = vterm_put_normal_char(p, c);
			if (rev == 0)
				continue;
			else if (rev < 0) {
				nbytes -= rev;
				buf += rev;
			}
		}
		if (p->scroll > 0)
			vterm_text_scroll_up(p, p->scroll);
		else if (p->scroll < 0)
			vterm_text_scroll_down(p, -(p->scroll));
		p->scroll = 0;
	}
}

#define ESC_ISO_GnDx(n, x) \
	{ \
		p->escSignature |= (x); p->escGn = (n);	\
		p->esc = vterm_esc_designate_font; \
	}

#define Fe(x) ((x) - 0x40)

static void vterm_esc_start(TVterm *p, u_char c)
{
	p->esc = NULL;
	switch (c) {
	case Fe(ISO_CSI):    /* 5/11 [ */
		p->esc = vterm_esc_bracket;
		break;
	case Fe(ISO_OSC):    /* 5/13 ] */
		p->esc = vterm_esc_rbracket;
		break;
	case ISO__MBS:       /* 2/4 $ */
		p->esc = vterm_esc_traditional_multibyte_fix;
		p->escSignature = FONT_SIGNATURE_DOUBLE;
		break;
	case ISO_DOCS:       /* 2/5 % */
		p->esc = vterm_esc_designate_otherCS;
		break;
	case ISO_GZD4:      /* 2/8 ( */
		ESC_ISO_GnDx(0, FONT_SIGNATURE_94CHAR);
		break;
	case MULE__GZD6:    /* 2/12 , */
		ESC_ISO_GnDx(0, FONT_SIGNATURE_96CHAR);
		break;
	case ISO_G1D4:      /* 2/9 ) */
		ESC_ISO_GnDx(1, FONT_SIGNATURE_94CHAR);
		break;
	case ISO_G1D6:      /* 2/13 - */
		ESC_ISO_GnDx(1, FONT_SIGNATURE_96CHAR);
		break;
	case ISO_G2D4:      /* 2/10 * */
		ESC_ISO_GnDx(2, FONT_SIGNATURE_94CHAR);
		break;
	case ISO_G2D6:      /* 2/14 . */
		ESC_ISO_GnDx(2, FONT_SIGNATURE_96CHAR);
		break;
	case ISO_G3D4:      /* 2/11 + */
		ESC_ISO_GnDx(3, FONT_SIGNATURE_94CHAR);
		break;
	case ISO_G3D6:      /* 2/15 / */
		ESC_ISO_GnDx(3, FONT_SIGNATURE_96CHAR);
		break;
	case Fe(ISO_NEL):   /* 4/5 E */
		p->pen.x = 0;
		p->wrap = false;
	case Fe(TERM_IND):  /* 4/4 D */
		if (p->pen.y == p->ymax - 1)
			p->scroll++;
		else
			p->pen.y++;
		break;
	case Fe(ISO_RI):    /* 4/13 M */
		if (p->pen.y == p->ymin)
			p->scroll--;
		else
			p->pen.y--;
		break;
	case Fe(ISO_SS2):   /* 4/14 N: 7bit single shift 2 */
		/* GL <== G2 */
		vterm_invoke_gx(p, &(p->tgl), 2);
		break;
	case Fe(ISO_SS3):   /* 4/15 O: 7bit single shift 3 */
		/* GL <== G3 */
		vterm_invoke_gx(p, &(p->tgl), 3);
		break;
	case ISO_RIS:       /* 6/3 c */
		pen_resetAttribute(&(p->pen));
		p->wrap = false;
		vterm_set_default_invoke_and_designate(p);
		/* fail into next case */
		/* TERM_CAH: - conflicts with ISO_G2D4, no terminfo */
		p->pen.x = 0;
		p->pen.y = 0;
		p->wrap = false;
		vterm_text_clear_all(p);
		break;
	case DEC_SC:        /* 3/7 7 */
		vterm_push_current_pen(p, true);
		break;
	case DEC_RC:        /* 3/8 8 */
		vterm_pop_pen_and_set_current_pen(p, true);
		p->wrap = false;
		break;
	case ISO_LS2:       /* 6/14 n */
		if (!vterm_is_ISO2022(p))
			break;
		/* GL <= G2 */
		vterm_invoke_gx(p, &(p->gl), 2);
		p->tgl = p->gl;
		break;
	case ISO_LS3:       /* 6/15 o */
		if (!vterm_is_ISO2022(p))
			break;
		/* GL <= G3 */
		vterm_invoke_gx(p, &(p->gl), 3);
		p->tgl = p->gl;
		break;
	case ISO_LS3R:      /* 7/12 | */
		if (!vterm_is_ISO2022(p))
			break;
		/* GR <= G3 */
		vterm_invoke_gx(p, &(p->gr), 3);
		p->tgr = p->gr;
		break;
	case ISO_LS2R:      /* 7/13 } */
		if (!vterm_is_ISO2022(p))
			break;
		/* GR <= G2 */
		vterm_invoke_gx(p, &(p->gr), 2);
		p->tgr = p->gr;
		break;
	case ISO_LS1R:      /* 7/14 ~ */
		if (!vterm_is_ISO2022(p))
			break;
		/* GR <= G1 */
		vterm_invoke_gx(p, &(p->gr), 1);
		p->tgr = p->gr;
		break;
	default:
		break;
	}
}

static void vterm_esc_set_attr(TVterm *p, int value)
{
	static int acsIdx;

	switch (value) {
	case 0:
		pen_resetAttribute(&(p->pen));
		break;
	case 1:
		pen_setHighlight(&(p->pen), true);
		break;
	case 21:
		pen_setHighlight(&(p->pen), false);
		break;
	case 4:
		pen_setUnderline(&(p->pen), true);
		break;
	case 24:
		pen_setUnderline(&(p->pen), false);
		break;
	case 7:
		pen_setReverse(&(p->pen), true);
		break;
	case 27:
		pen_setReverse(&(p->pen), false);
		break;
	case 10: /* rmacs, rmpch */
		p->gIdx[0] = 0;
		vterm_re_invoke_gx(p, &(p->gl));
		vterm_re_invoke_gx(p, &(p->gr));
		p->tgl = p->gl; p->tgr = p->gr;
		p->altCs = false;
		break;
	case 11: /* smacs, smpch */
		if (acsIdx == 0)
			acsIdx = font_getIndexBySignature(0x30 |
						FONT_SIGNATURE_94CHAR);
		if (acsIdx > 0) {
			p->gIdx[0] = acsIdx;
			vterm_re_invoke_gx(p, &(p->gl));
			vterm_re_invoke_gx(p, &(p->gr));
			p->tgl = p->gl; p->tgr = p->gr;
			p->altCs = true;
		}
		break;
	default:
		pen_setColor(&(p->pen), value);
		break;
	}
}

static void vterm_set_mode(TVterm *p, int value, bool question, bool flag)
{
	switch (value) {
	case 1:
		if (question)
			p->cursor = flag;
		break;
	case 4:
		if (!question)
			p->insert = flag;
		break;
	case 5:
		if (question) {
			palette_reverse(flag);
			if (!palette_hasColorMap()) {
				vterm_unclean(p);
				vterm_refresh(p);
			}
			usleep(50000); /* 0.05 sec. */
		}
		break;
	case 9:
		if (question)
			p->mouseTracking = flag ?
				VTERM_MOUSE_TRACKING_X10 :
				VTERM_MOUSE_TRACKING_NONE;
		break;
	case 25:
		if (question) {
			cursor.on = flag;
			cursor_show(p, &cursor, flag);
		}
		break;
	case 1000:
		if (question)
			p->mouseTracking = flag ?
				VTERM_MOUSE_TRACKING_VT200 :
				VTERM_MOUSE_TRACKING_NONE;
		break;
	case 1002:
		if (question)
			p->mouseTracking = flag ?
				VTERM_MOUSE_TRACKING_BTN_EVENT :
				VTERM_MOUSE_TRACKING_NONE;
		break;
	case 1003:
		if (question)
			p->mouseTracking = flag ?
				VTERM_MOUSE_TRACKING_ANY_EVENT :
				VTERM_MOUSE_TRACKING_NONE;
		break;
	default:
		break;
	}
}

#define REPORTLEN (64)

static void vterm_esc_report(TVterm *p, u_char c, int value)
{
	char report[REPORTLEN];
	int x, y;

	report[0] = '\0';
	switch (c) {
	case 'n':
		switch (value) {
		case 5:
			strncpy(report, "\033[0n", REPORTLEN);
			report[REPORTLEN - 1] = '\0';
			break;
		case 6:
			x = (p->pen.x < p->xmax - 1) ? p->pen.x : p->xmax - 1;
			y = (p->pen.y < p->ymax - 1) ? p->pen.y : p->ymax - 1;
			snprintf(report, REPORTLEN, "\033[%d;%dR",
				 y + 1, x + 1);
			report[REPORTLEN - 1] = '\0';
			break;
		default:
			break;
		}
		break;
	case 'c':
		switch (value) {
		case 0:
			strncpy(report, "\033[?6c", REPORTLEN);
			report[REPORTLEN - 1] = '\0';
		default:
			break;
		}
		break;
	default:
		break;
	}
	if (strlen(report) > 0)
		write_wrapper(p->term->masterPty, report, strlen(report));
}

static void vterm_set_region(TVterm *p, int ymin, int ymax)
{
	if (ymin < 0 || ymin >= p->rows || ymin > ymax || ymax > p->rows)
		return;
	p->ymin = ymin;
	p->ymax = ymax;
	p->pen.x = 0;
	if (p->pen.y < p->ymin || p->pen.y > p->ymax - 1)
		p->pen.y = p->ymin;
	p->wrap = false;
}

static void vterm_set_window_size(TVterm *p)
{
	struct winsize winsize;

	winsize.ws_row = p->ymax;
	winsize.ws_col = p->xmax;
	winsize.ws_xpixel = 0;
	winsize.ws_ypixel = 0;
	if (ioctl(p->term->slavePty, TIOCSWINSZ, &winsize) == -1)
		warn("ioctl(TIOCSWINSZ)");
}

static void vterm_esc_status_line(TVterm *p, u_char c)
{
	switch (c) {
	case 'T': /* To */
		if (p->statusLine == VTERM_STATUS_LINE_ENTER)
			break;
		if (p->savedPenSL == NULL)
			vterm_push_current_pen(p, false);
		/* FALLTHROUGH */
	case 'S': /* Show */
		if (p->statusLine == VTERM_STATUS_LINE_NONE) {
			p->ymax = p->rows - 1;
			vterm_set_window_size(p);
		}
		if (c == 'T') {
			p->statusLine = VTERM_STATUS_LINE_ENTER;
			vterm_set_region(p, p->rows - 1, p->rows);
		}
		break;
	case 'F': /* From */
		if (p->statusLine == VTERM_STATUS_LINE_ENTER) {
			p->statusLine = VTERM_STATUS_LINE_LEAVE;
			vterm_set_region(p, 0, p->rows - 1);
			if (p->savedPenSL)
				vterm_pop_pen_and_set_current_pen(p, false);
			p->savedPenSL = NULL;
		}
		break;
	case 'H': /* Hide */
		/* FALLTHROUGH */
	case 'E': /* Erase */
		if (p->statusLine == VTERM_STATUS_LINE_NONE)
			break;
		vterm_set_region(p, 0, p->rows);
		vterm_set_window_size(p);
		p->statusLine = VTERM_STATUS_LINE_NONE;
		break;
	default:
		break;
	}
	p->wrap = false;
	p->esc = NULL;
}

#define MAX_NARG (16)

static void vterm_esc_bracket(TVterm *p, u_char c)
{
	static int values[MAX_NARG];
	static bool questions[MAX_NARG];
	static int narg;
	int n;

	if (c >= '0' && c <= '9')
		values[narg] = (values[narg] * 10) + (c - '0');
	else if (c == '?')
		questions[narg] = true;
	else if (c == ';') {
		if (narg < MAX_NARG) {
			narg++;
			values[narg] = 0;
			questions[narg] = false;
		} else
			p->esc = NULL;
	} else {
		p->esc = NULL;
		switch (c) {
		case ISO_CS_NO_ICH:
			if (narg < 1 && !questions[0]) {
				n = values[0] != 0 ? values[0] : 1;
				vterm_insert_n_chars(p, n);
			}
			break;
		case ISO_CS_NO_CUU:
			if (narg < 1 && !questions[0]) {
				n = values[0] != 0 ? values[0] : 1;
				p->pen.y -= n;
				if (p->pen.y < p->ymin) {
					p->scroll -= p->pen.y - p->ymin;
					p->pen.y = p->ymin;
				}
			}
			break;
		case ISO_CS_NO_CUD:
			if (narg < 1 && !questions[0]) {
				n = values[0] != 0 ? values[0] : 1;
				p->pen.y += n;
				if (p->pen.y >= p->ymax) {
					p->scroll += p->pen.y - p->ymin;
					p->pen.y = p->ymax - 1;
				}
			}
			break;
		case ISO_CS_NO_CUF:
			if (narg < 1 && !questions[0]) {
				n = values[0] != 0 ? values[0] : 1;
				p->pen.x += n;
				p->wrap = false;
			}
			break;
		case ISO_CS_NO_CUB:
			if (narg < 1 && !questions[0]) {
				n = values[0] != 0 ? values[0] : 1;
				p->pen.x -= n;
				p->wrap = false;
			}
			break;
		case 'G':
			if (narg < 1 && !questions[0]) {
				n = values[0] != 0 ? values[0] - 1 : 0;
				p->pen.x = n;
				p->wrap = false;
			}
			break;
		case 'J':
			if (narg < 1 && !questions[0])
				vterm_text_clear_eos(p, values[0]);
			break;
		case 'K':
			if (narg < 1 && !questions[0])
				vterm_text_clear_eol(p, values[0]);
			break;
		case 'L':
			if (narg < 1 && !questions[0]) {
				n = values[0] != 0 ? values[0] : 1;
				vterm_text_move_down(p, p->pen.y, p->ymax, n);
			}
			break;
		case 'M':
			if (narg < 1 && !questions[0]) {
				n = values[0] != 0 ? values[0] : 1;
				vterm_text_move_up(p, p->pen.y, p->ymax, n);
			}
			break;
		case 'P':
			if (narg < 1 && !questions[0]) {
				n = values[0] != 0 ? values[0] : 1;
				vterm_delete_n_chars(p, n);
			}
			break;
		case 'S':
			/* FALLTHROUGH */
		case 'F':
			/* FALLTHROUGH */
		case 'E':
			if (narg < 1 && questions[0])
				vterm_esc_status_line(p, c);
			break;
		case 'H':
			if (narg < 1 && questions[0]) {
				vterm_esc_status_line(p, c);
				break;
			}
			/* FALLTHROUGH */
		case 'f':
			if (narg < 2 && !questions[0] && !questions[1]) {
				n = values[1] != 0 ? values[1] - 1 : 0;
				p->pen.x = n;
				p->wrap = false;
				n = values[0] != 0 ? values[0] - 1 : 0;
				p->pen.y = n;
			}
			break;
		case 'd':
			/* XXX: resize(1) specify large x,y */
			if (narg < 1 && !questions[0]) {
				n = values[0] != 0 ? values[0] - 1 : 0;
				p->pen.y = n;
			}
			break;
		case 'm':
			for (n = 0; n <= narg; n++) {
				if (questions[n])
					continue;
#ifdef ENABLE_256_COLOR
				if (values[n] == 38 || values[n] == 48) {
					if (n + 2 <= narg && values[n + 1] == 5) {
						pen_set256Color(&(p->pen),
								values[n],
								values[n + 2]);
						n += 2;
					}
				} else
					vterm_esc_set_attr(p, values[n]);
#else
				vterm_esc_set_attr(p, values[n]);
#endif
			}
			break;
		case 'r':
			if (narg == 1 && !questions[0] && !questions[1]) {
				n = values[1];
				if (n == 0)
					n = p->rows;
				if (p->statusLine != VTERM_STATUS_LINE_NONE)
					if (n == p->rows)
						n--;
				vterm_set_region(p, values[0] != 0 ? (values[0] - 1) : 0, n);
			}
			break;
		case 'l':
			for (n = 0; n <= narg; n++)
				vterm_set_mode(p, values[n], questions[n], false);
			break;
		case 'h':
			for (n = 0; n <= narg; n++)
				vterm_set_mode(p, values[n], questions[n], true);
			break;
		case 's':
			if (narg < 1 && !questions[0])
				vterm_push_current_pen(p, true);
			break;
		case 'u':
			if (narg < 1 && !questions[0])
				vterm_pop_pen_and_set_current_pen(p, true);
			break;
		case 'n':
			/* FALLTHROUGH */
		case 'c':
			if (narg < 1 && !questions[0])
				vterm_esc_report(p, c, values[0]);
			break;
		case 'R':
			break;
		case ']':
			if (narg > 0) {
				if (values[0] == 10)
					bell_setFreq(values[1]);
				else if (values[0] == 11)
					bell_setDuration(values[1]);
			}
			break;
		default:
			break;
		}
	}
	if (p->esc == NULL) {
		for (n = 0; n <= narg; n++) {
			values[n] = 0;
			questions[n] = false;
		}
		narg = 0;
	}
}

#define MAX_VALUE_LENGTH (48)

/*
 * ESC  ]    p... F
 * 1/11 5/13 p... F
 *
 * p: 2/0 - 7/E
 * F: 0/1 - 1/15
 *
 * 0/5	designate private coding system
 */
static void vterm_esc_rbracket(TVterm *p, u_char c)
{
	static char value[MAX_VALUE_LENGTH + 1];
	static int i;
	static char prev;

	if (c == '\033') {
	} else if (prev == '\033' && c == '\\') {
#ifdef ENABLE_256_COLOR
		int ansiColor;
		char *cp;
		if (strlen(value) >= 2 && strncmp(value, "4;", 2) == 0) {
			if ((cp = strchr(&value[2], ';')) != NULL) {
				*cp = '\0';
				ansiColor = atoi(&value[2]);
				cp++;
				if (palette_update(ansiColor, cp))
					if (!palette_hasColorMap()) {
						vterm_unclean(p);
						vterm_refresh(p);
					}
			}
		}
#endif
		p->esc = NULL;
	} else if (prev == '\033' && c != '\\')
		p->esc = NULL;
	else if (c >= 0x20 && c <= 0x7e) {
		if (i < MAX_VALUE_LENGTH - 1) {
			value[i] = c;
			i++;
			value[i] = '\0';
		} else
			p->esc = NULL;
	} else {
#ifdef ENABLE_OTHER_CODING_SYSTEM
		const char *config;
		char *encoding, *other;

		switch (c) {
		case 0x05: /* 0/5 designate private coding system */
			encoding = strdup(value);
			if (strchr(encoding, ',') == NULL) {
				config = caps_findEntry(p->caps, "encoding.",
							encoding);
				if (config != NULL) {
					free(encoding);
					encoding = strdup(config);
					if (encoding == NULL)
						err(1, "strdup()");
				} else {
					other = vterm_format_otherCS(encoding);
					free(encoding);
					encoding = other;
				}
				vterm_set_default_encoding(p, encoding);
			}
			free(encoding);
			break;
		default:
			break;
		}
#endif
		p->esc = NULL;
	}
	if (p->esc == NULL) {
		i = 0;
		prev = '\0';
		memset(value, '\0', sizeof(value));
		return;
	}
	prev = c;
}

static void vterm_invoke_gx(TVterm *p, TFontSpec *fs, u_int n)
{
	fs->invokedGn = n;
	fs->idx = p->gIdx[fs->invokedGn];
	fs->type = gFonts[fs->idx].signature;
	fs->half = gFonts[fs->idx].half;
}

static void vterm_re_invoke_gx(TVterm *p, TFontSpec *fs)
{
	fs->idx = p->gIdx[fs->invokedGn];
	fs->type = gFonts[fs->idx].signature;
	fs->half = gFonts[fs->idx].half;
}

static void vterm_esc_designate_font(TVterm *p, u_char c)
{
	int i;

	if (vterm_is_ISO2022(p)) {
		i = font_getIndexBySignature(c | p->escSignature);
		if (i >= 0) {
			p->gIdx[p->escGn] = i;
			vterm_re_invoke_gx(p, &(p->gl));
			vterm_re_invoke_gx(p, &(p->gr));
			p->tgl = p->gl; p->tgr = p->gr;
		}
	}
	p->esc = NULL;
}

static void vterm_esc_designate_otherCS(TVterm *p, u_char c)
{
	switch (c) {
	case 0x40:
		/* reset ISO-2022 */
		vterm_switch_to_ISO2022(p);
		break;
#ifdef ENABLE_UTF8
	case 0x47: /* XXX: designate and invoke UTF-8 coding */
		vterm_switch_to_UTF8(p);
		break;
#endif
	default:
		break;
	}
	p->esc = NULL;
}

static void vterm_esc_traditional_multibyte_fix(TVterm *p, u_char c)
{
	if (c == 0x40 || c == 0x41 || c == 0x42)
		vterm_esc_designate_font(p, c);
	else
		vterm_esc_start(p, c);
}

void vterm_show_sequence(FILE *stream, const char *encoding)
{
	static const char *invokeL[] = { "\017", "\016", "\033n", "\033o" };
	static const char *invokeR[] = { "", "\033~", "\033}", "\033|" };
	static const char csr4[] = { ISO_GZD4, ISO_G1D4, ISO_G2D4, ISO_G3D4 };
	static const char csr6[] = { MULE__GZD6, ISO_G1D6, ISO_G2D6, ISO_G3D6 };
	TCsv csv;
	int i, idx[6];
	const char *token;

	assert(stream != NULL);
	assert(encoding != NULL);

	csv_initialize(&csv, encoding);
	/* get first token */
	token = csv_getToken(&csv);
#ifdef ENABLE_OTHER_CODING_SYSTEM
	if (csv.tokens == 4 && strcasecmp(token, "other") == 0) {
		/* other,<coding-system>,iconv,<std-coding-system> */
		TCodingSystem *codingSystem;
		codingSystem = malloc(sizeof(TCodingSystem));
		if (codingSystem == NULL)
			err(1, "malloc()");
		codingSystem_initialize(codingSystem);
		if (parse_otherCS(encoding, codingSystem))
			fprintf(stream, "\033]%s\005", codingSystem->fromcode);
		else
			warnx("ENCODING : BAD FORMAT : %s", encoding);
		codingSystem_finalize(codingSystem);
		free(codingSystem);
		csv_finalize(&csv);
		return;
	}
#endif
#ifdef ENABLE_UTF8
	if (csv.tokens == 2 && strcasecmp(token, "UTF-8") == 0) {
		/* UTF-8,<fontsetname> */
		fputs("\033%G", stream);
		csv_finalize(&csv);
		return;
	}
#endif
	csv_finalize(&csv);
	/* ISO-2022 */
	if (!parse_encoding(encoding, idx)) {
		warnx("ENCODING : BAD FORMAT : %s", encoding);
		return;
	}
	fputs("\033%@", stream);
	/* GL */
	fputs(invokeL[idx[0]], stream);
	/* GR */
	fputs(invokeR[idx[1]], stream);
	/* G0 .. G3 */
	for (i = 0; i < 4; i++) {
		TFont *font;
		u_int signature;
		font = &gFonts[idx[2 + i]];
		signature = font->signature;
		fprintf(stream, "\033%s%c%c",
			(signature & FONT_SIGNATURE_DOUBLE ? "$" : ""),
			(signature & FONT_SIGNATURE_96CHAR ? csr6[i] : csr4[i]),
			signature & 0xff);
	}
}

