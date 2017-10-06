/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
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

#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "clipboard.h"

static struct {
	char *text;
	size_t length;
} clipboard = {
	NULL,
	0
};

static bool initialized;

static void finalize(void);
static void finalizer(void);

void clipboard_initialize(void)
{
	assert(!initialized);

	atexit(finalizer);
	initialized = true;
	clipboard.length = 1;
	clipboard.text = malloc(clipboard.length);
	if (clipboard.text == NULL)
		err(1, "malloc()");
	clipboard.text[clipboard.length - 1] = '\0';
}

static void finalize(void)
{
	assert(initialized);

	if (clipboard.text != NULL) {
		free(clipboard.text);
		clipboard.text = NULL;
	}
	clipboard.length = 0;
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

void clipboard_clear(void)
{
	assert(initialized);

	if (clipboard.text != NULL)
		free(clipboard.text);
	clipboard.length = 1;
	clipboard.text = malloc(clipboard.length);
	if (clipboard.text == NULL)
		err(1, "malloc()");
	clipboard.text[clipboard.length - 1] = '\0';
}

void clipboard_appendText(const char *text, const size_t length)
{
	int newlength;

	assert(initialized);

	if (text != NULL && length > 0) {
		newlength = clipboard.length + length;
		clipboard.text = realloc(clipboard.text, newlength);
		if (clipboard.text == NULL)
			err(1, "realloc()");
		memcpy(clipboard.text + clipboard.length - 1, text, length);
		clipboard.length = newlength;
		clipboard.text[clipboard.length - 1] = '\0';
	}
}

const char *clipboard_getText(void)
{
	assert(initialized);

	return clipboard.text;
}

size_t clipboard_getLength(void)
{
	assert(initialized);

	return clipboard.length;
}


