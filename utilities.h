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

#ifndef INCLUDE_UTILITIES_H
#define INCLUDE_UTILITIES_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#define UNUSED_VARIABLE(x) (void)(x)

uint32_t UINT32_SWAP_ALWAYS(uint32_t value);
int32_t INT32_SWAP_ALWAYS(int32_t value);
uint16_t UINT16_SWAP_ALWAYS(uint16_t value);
int16_t INT16_SWAP_ALWAYS(int16_t value);
uint32_t UINT32_SWAP_ON_BE(uint32_t value);
int32_t INT32_SWAP_ON_BE(int32_t value);
uint16_t UINT16_SWAP_ON_BE(uint16_t value);
int16_t INT16_SWAP_ON_BE(int16_t value);
uint32_t UINT32_SWAP_ON_LE(uint32_t value);
int32_t INT32_SWAP_ON_LE(int32_t value);
uint16_t UINT16_SWAP_ON_LE(uint16_t value);
int16_t INT16_SWAP_ON_LE(int16_t value);
int lookup(bool ignorecase, const char *s, ...);
int strrcmp(const char *s1, const char *s2);
int hex2int(const char c);
ssize_t write_wrapper(int fd, const void *buf, size_t nbytes);

#endif /* INCLUDE_UTILITIES_H */

