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

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#if defined (__linux__)
#include <endian.h>
#elif (defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__))
#include <machine/endian.h>
#endif

#include "utilities.h"

uint32_t UINT32_SWAP_ALWAYS(uint32_t value)
{
	uint8_t *b;

	b = (uint8_t *)&value;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return b[3] + (b[2] << 8) + (b[1] << 16) + (b[0] << 24);
#elif __BYTE_ORDER == __BIG_ENDIAN
	return b[0] + (b[1] << 8) + (b[2] << 16) + (b[3] << 24);
#else
	#error unknown byte order
#endif
}

int32_t INT32_SWAP_ALWAYS(int32_t value)
{
	return (int32_t)UINT32_SWAP_ALWAYS((uint32_t)value);
}

uint16_t UINT16_SWAP_ALWAYS(uint16_t value)
{
	uint8_t *b;

	b = (uint8_t *)&value;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return b[1] + (b[0] << 8);
#elif __BYTE_ORDER == __BIG_ENDIAN
	return b[0] + (b[1] << 8);
#else
	#error unknown byte order
#endif
}

int16_t INT16_SWAP_ALWAYS(int16_t value)
{
	return (int16_t)UINT16_SWAP_ALWAYS((uint16_t)value);
}

uint32_t UINT32_SWAP_ON_BE(uint32_t value)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return value;
#elif __BYTE_ORDER == __BIG_ENDIAN
	return UINT32_SWAP_ALWAYS(value);
#else
	#error unknown byte order
#endif
}

int32_t INT32_SWAP_ON_BE(int32_t value)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return value;
#elif __BYTE_ORDER == __BIG_ENDIAN
	return INT32_SWAP_ALWAYS(value);
#else
	#error unknown byte order
#endif
}

uint16_t UINT16_SWAP_ON_BE(uint16_t value)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return value;
#elif __BYTE_ORDER == __BIG_ENDIAN
	return UINT16_SWAP_ALWAYS(value);
#else
	#error unknown byte order
#endif
}

int16_t INT16_SWAP_ON_BE(int16_t value)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return value;
#elif __BYTE_ORDER == __BIG_ENDIAN
	return INT16_SWAP_ALWAYS(value);
#else
	#error unknown byte order
#endif
}

uint32_t UINT32_SWAP_ON_LE(uint32_t value)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return UINT32_SWAP_ALWAYS(value);
#elif __BYTE_ORDER == __BIG_ENDIAN
	return value;
#else
	#error unknown byte order
#endif
}

int32_t INT32_SWAP_ON_LE(int32_t value)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return INT32_SWAP_ALWAYS(value);
#elif __BYTE_ORDER == __BIG_ENDIAN
	return value;
#else
	#error unknown byte order
#endif
}

uint16_t UINT16_SWAP_ON_LE(uint16_t value)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return UINT16_SWAP_ALWAYS(value);
#elif __BYTE_ORDER == __BIG_ENDIAN
	return value;
#else
	#error unknown byte order
#endif
}

int16_t INT16_SWAP_ON_LE(int16_t value)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return INT16_SWAP_ALWAYS(value);
#elif __BYTE_ORDER == __BIG_ENDIAN
	return value;
#else
	#error unknown byte order
#endif
}

int lookup(bool ignorecase, const char *s, ...)
{
	va_list va;
	char *p;
	int i, result;
	int (*compare)(const char *s1, const char *s2);
		
	result = -1;
	compare = ignorecase ? strcasecmp : strcmp;
	if (s != NULL) {
		va_start(va, s);
		for (i = 0; ((p = va_arg(va, char *)) != NULL); i++) {
			if (compare(s, p) == 0) {
				result = i;
				break;
			}
		}
		va_end(va);
	}
	return result;
}

int strrcmp(const char *s1, const char *s2)
{
	assert(s1 != NULL);
	assert(s2 != NULL);

	return strcmp(s1 + strlen(s1) - strlen(s2), s2);
}

/* Same as digittoint(3) */
int hex2int(const char c)
{
	int result;

	result = 0;
	if (c >= '0' && c <= '9')
		result = c - '0';
	else if (c >= 'a' && c <= 'f')
		result = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		result = c - 'A' + 10;
	return result;
}

ssize_t write_wrapper(int fd, const void *buf, size_t nbytes)
{
	ssize_t n, total;

	assert(buf != NULL);

	total = 0;
	while (total < nbytes) {
		n = write(fd, buf, nbytes - total);
		if (n == -1) n = 0;
		total += n;
		if (errno != EINTR)
			break;
	}
	return total;
}

