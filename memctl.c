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

#include <sys/types.h>
#include <assert.h>
#include <stdbool.h>

#if defined (__linux__) && (defined (__i386__) || defined (__x86_64__))
#include <asm/mtrr.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#elif (defined (__FreeBSD__) || defined (__OpenBSD__)) && (defined (__i386__) || defined (__x86_64__))
#include <sys/ioctl.h>
#include <sys/memrange.h>
#include <fcntl.h>
#include <paths.h>
#include <string.h>
#elif defined (__NetBSD__) && (defined (__i386__) || defined (__x86_64__))
#include <machine/mtrr.h>
#include <machine/sysarch.h>
#endif

#include "privilege.h"
#include "utilities.h"

bool memctl_setWriteCombine(unsigned long base, unsigned long size)
{
#if defined (__linux__) && (defined (__i386__) || defined (__x86_64__))
	bool result;
	struct mtrr_sentry mtrr_sentry;
	int fd;

	assert(base != 0);
	assert(size != 0);

	result = false;
	privilege_on();
	fd = open("/proc/mtrr", O_WRONLY);
	privilege_off();
	if (fd != -1) {
		mtrr_sentry.base = base;
		mtrr_sentry.size = size;
		mtrr_sentry.type = MTRR_TYPE_WRCOMB;
		if (ioctl(fd, MTRRIOC_ADD_ENTRY, &mtrr_sentry) != -1)
			result = true;
		close(fd);
	}
	return result;
#elif (defined (__FreeBSD__) || defined (__OpenBSD__)) && (defined (__i386__) || defined (__x86_64__))
	bool result;
	struct mem_range_desc mem_range_desc;
	struct mem_range_op mem_range_op;
	int fd;

	assert(base != 0);
	assert(size != 0);

	result = false;
	privilege_on();
	fd = open(_PATH_MEM, O_RDONLY);
	privilege_off();
	if (fd != -1) {
		mem_range_desc.mr_base  = base;
		mem_range_desc.mr_len   = size;
		mem_range_desc.mr_flags = MDF_WRITECOMBINE;
		strncpy(mem_range_desc.mr_owner, "vesa",
			sizeof(mem_range_desc.mr_owner));
		mem_range_desc.mr_owner[sizeof(mem_range_desc.mr_owner) - 1] =
			'\0';
		mem_range_op.mo_desc   = &mem_range_desc;
		mem_range_op.mo_arg[0] = 0;
		if (ioctl(fd, MEMRANGE_SET, &mem_range_op) != -1)
			result = true;
		close(fd);
	}
	return result;
#elif defined (__NetBSD__) && (defined (__i386__) || defined (__x86_64__))
	bool result;
	struct mtrr mtrr;
	int n;

	assert(base != 0);
	assert(size != 0);

	result = false;
	mtrr.base  = base;
	mtrr.len   = size;
	mtrr.type  = MTRR_TYPE_WC;
	mtrr.flags = MTRR_VALID;
	mtrr.owner = 0;
	n = 1;
	privilege_on();
#if defined (__i386__)
	if (i386_set_mtrr(&mtrr, &n) != -1)
		result = true;
#elif defined (__x86_64__)
	if (x86_64_set_mtrr(&mtrr, &n) != -1)
		result = true;
#endif
	privilege_off();
	return result;
#else
	UNUSED_VARIABLE(base);
	UNUSED_VARIABLE(size);
	return false;
#endif
}

