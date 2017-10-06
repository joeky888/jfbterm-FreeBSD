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

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "csv.h"

void csv_initialize(TCsv *p, const char *s)
{
	char *cp, *cq;

	assert(p != NULL);
	assert(s != NULL);

	p->buffer = strdup(s);
	if (p->buffer == NULL)
		err(1, "strdup()");
	p->next = p->buffer;
	p->count = 0;
	p->tokens = 1;
	for (cp = cq = p->buffer; *cp != '\0'; cp++) {
		switch (*cp) {
		case ',':
			*cq++ = '\0';
			p->tokens++;
			break;
		default:
			*cq++ = *cp;
			break;
		}
	}
	*cq = '\0';
}

void csv_finalize(TCsv *p)
{
	assert(p != NULL);

	if (p->buffer != NULL) {
		free(p->buffer);
		p->buffer = NULL;
	}
}

const char *csv_getToken(TCsv *p)
{
	char *token;

	assert(p != NULL);

	if (p->count >= p->tokens)
		return NULL;
	for (token = p->next; *(p->next) != '\0'; p->next++) {
		/* do nothing. */
	}
	p->next++;
	p->count++;
	return token;
}

