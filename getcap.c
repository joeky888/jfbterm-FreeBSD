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

#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "getcap.h"
#include "utilities.h"

#define BUF_SIZE (1024)
#define MAX_COLS (256)

static void capValue_initialize(TCapValue *p);
static void capValue_finalize(TCapValue *p);
static void capability_initialize(TCapability *p);
static void capability_finalize(TCapability *p);
static void capability_deleteAllValues(TCapability *p);
static void capability_setName(TCapability *p, const char *name);
static void capability_addValue(TCapability *p, const char *value);
static void caps_register_nv(TCaps *p, const char *name, const char *value, char f);
static char *trim_left(char *cp);
static char *trim_left_right(char *cp);
static bool caps_register(TCaps *p, const char *cp);

static void capValue_initialize(TCapValue *p)
{
	assert(p != NULL);

	p->next = NULL;
	p->value = NULL;
}

static void capValue_finalize(TCapValue *p)
{
	assert(p != NULL);

	if (p->value != NULL) {
		free(p->value);
		p->value = NULL;
	}
}

static void capability_initialize(TCapability *p)
{
	assert(p != NULL);

	p->next = NULL;
	p->name = NULL;
	p->values = NULL;
}

static void capability_finalize(TCapability *p)
{
	assert(p != NULL);

	capability_deleteAllValues(p);
	if (p->name != NULL) {
		free(p->name);
		p->name = NULL;
	}
}

static void capability_deleteAllValues(TCapability *p)
{
	TCapValue *capValue, *next;

	assert(p != NULL);

	capValue = p->values;
	while (capValue != NULL) {
		next = capValue->next;
		capValue_finalize(capValue);
		free(capValue);
		capValue = next;
	}
	p->values = NULL;
}

static void capability_setName(TCapability *p, const char *name)
{
	assert(p != NULL);
	assert(name != NULL);

	free(p->name);
	p->name = NULL;
	if (name != NULL) {
		p->name = strdup(name);
		if (p->name == NULL)
			err(1, "strdup()");
	}
}

static void capability_addValue(TCapability *p, const char *value)
{
	TCapValue *cp;
	TCapValue *en;

	assert(p != NULL);
	assert(value != NULL);

	cp = malloc(sizeof(TCapValue));
	if (cp == NULL)
		err(1, "malloc()");
	capValue_initialize(cp);
	if (p->values != NULL) {
		en = p->values;
		while (en->next != NULL)
			en = en->next;
		en->next = cp;
	} else
		p->values = cp;
	cp->value = strdup(value);
	if (cp->value == NULL)
		err(1, "strdup()");
}

void caps_initialize(TCaps *p)
{
	assert(p != NULL);

	p->next = NULL;
}

void caps_finalize(TCaps *p)
{
	TCapability *capability, *next;

	assert(p != NULL);

	capability = p->next;
	while (capability != NULL) {
		next = capability->next;
		capability_finalize(capability);
		free(capability);
		capability = next;
	}
	p->next = NULL;
}

TCapability *caps_find(TCaps *p, const char *name)
{
	TCapability *capability;

	assert(p != NULL);
	assert(name != NULL);

	for (capability = p->next; capability != NULL;
	     capability = capability->next) {
		if (capability->name != NULL &&
		    strcasecmp(name, capability->name) == 0)
			break;
	}
	return capability;
}

const char *caps_findFirst(TCaps *p, const char *name)
{
	TCapability *capability;

	assert(p != NULL);
	assert(name != NULL);

	capability = caps_find(p, name);
	if (capability == NULL || capability->values == NULL)
		return NULL;
	return capability->values->value;
}

const char *caps_findEntry(TCaps *p, const char *prefix, const char *name)
{
	char *key;
	const char *value;
	int len;

	assert(p != NULL);
	assert(prefix != NULL);
	assert(name != NULL);

	len = strlen(prefix) + strlen(name) + 1;
	key = malloc(len);
	if (key == NULL)
		err(1, "malloc()");
	strncpy(key, prefix, len);
	key[len - 1] = '\0';
	strncat(key, name, len - strlen(key));
	key[len - 1] = '\0';
	value = caps_findFirst(p, key);
	free(key);
	return value;
}

static void caps_register_nv(TCaps *p, const char *name, const char *value, char f)
{
	TCapability *capability;

	assert(p != NULL);
	assert(name != NULL);
	assert(value != NULL);

	capability = caps_find(p, name);
	if (capability == NULL) {
		capability = malloc(sizeof(TCapability));
		if (capability == NULL)
			err(1, "malloc()");
		capability_initialize(capability);
		capability->next = p->next;
		p->next = capability;
		capability_setName(capability, name);
	}
	if (f == '=')
		capability_deleteAllValues(capability);
	capability_addValue(capability, value);
}

static char *trim_left(char *cp)
{
	assert(cp != NULL);

	while (*cp != '\0') {
		if (*cp != ' ' && *cp != '\t')
			break;
		cp++;
	}
	return cp;
}

static char *trim_left_right(char *cp)
{
	char *q;
	char c;

	assert(cp != NULL);

	cp = trim_left(cp);
	q = cp + strlen(cp);
	while (cp < q) {
		c = *(--q);
		if (c != ' ' && c != '\t') {
			*(++q) = '\0';
			break;
		}
	}
	return cp;
}

static bool caps_register(TCaps *p, const char *cp)
{
	char line[MAX_COLS];
	char *n, *v, *q;

	assert(p != NULL);
	assert(cp != NULL);

	strncpy(line, cp, MAX_COLS);
	line[MAX_COLS - 1] = '\0';
	if ((q = strchr(line, ':')) == NULL)
		return false;
	*q = '\0';
	n = trim_left_right(line);
	v = trim_left_right(q + 1);
	if (*n == '\0')
		return false;
	if (*n == '+')
		caps_register_nv(p, n + 1, v, '+');
	else
		caps_register_nv(p, n, v, '=');
	return true;
}

void caps_readFile(TCaps *p, const char *path)
{
	FILE *stream;
	char *q;
	char line[MAX_COLS];
	bool b;
	int nl;
	int len;

	assert(p != NULL);
	assert(path != NULL);

	if ((stream = fopen(path, "r")) == NULL)
		err(1, "%s", path);
#if DEBUG
	fprintf(stderr, "(**) : Configuration file `%s'\n", path);
#else
	fprintf(stderr, "Configuration file `%s'\n", path);
#endif
	for (nl = 1; fgets(line, MAX_COLS, stream) != NULL; nl++) {
		len = strlen(line);
		if (len > 0)
			line[len - 1] = '\0';
		if ((q = strchr(line, '#')) != NULL)
			*q = '\0';
		q = trim_left(line);
		if (*q == '\0')
			continue;
		if (strchr(line, ':') == NULL) {
#if DEBUG
			fprintf(stderr, "(--) : line %d, `%s'\n", nl, line);
#endif
			continue;
		}
		b = caps_register(p, line);
#if DEBUG
		if (!b)
			fprintf(stderr, "(--) : line %d, `%s'\n", nl, line);
		else
			fprintf(stderr, "(**) : line %d, `%s'\n", nl, line);
#endif
	}
#if DEBUG
	fprintf(stderr, "(**) : total %d lines read.\n", nl);
#endif
	fclose(stream);
}

