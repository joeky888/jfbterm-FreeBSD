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
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#if defined (__linux__)
#include <sys/types.h>
#endif

#include "privilege.h"

static uid_t ruid, euid;
static gid_t rgid, egid;

static bool initialized;

static void finalize(void);
static void finalizer(void);

void privilege_initialize(void)
{
	assert(!initialized);

	atexit(finalizer);
	initialized = true;
	ruid = getuid();
	euid = geteuid();
	rgid = getgid();
	egid = getegid();
	privilege_off();
}

static void finalize(void)
{
	assert(initialized);

	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

void privilege_on(void)
{
	int errsv;

	assert(initialized);

	errsv = errno;
	seteuid(euid);
	setegid(egid);
	errno = errsv;
}

void privilege_off(void)
{
	int errsv;

	assert(initialized);

	errsv = errno;
	setegid(rgid);
	seteuid(ruid);
	errno = errsv;
}

uid_t privilege_getUID(void)
{
	assert(initialized);

	return ruid;
}

gid_t privilege_getGID(void)
{
	assert(initialized);

	return rgid;
}

bool privilege_isSetUID(void)
{
	assert(initialized);

	return ruid != euid;
}

bool privilege_isSetGID(void)
{
	assert(initialized);

	return rgid != egid;
}

void privilege_drop(void)
{
	int errsv;

	assert(initialized);

	errsv = errno;
	setgid(rgid);
	setuid(ruid);
	errno = errsv;
}

