/*
 * JFBTERM for FreeBSD
 * Copyright (C) 2009 Yusuke BABA <babayaga1@y8.dion.ne.jp>
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

#ifndef INCLUDE_MOUSE_H
#define INCLUDE_MOUSE_H

#include <sys/types.h>
#include <stdbool.h>

#include "getcap.h"
#include "vterm.h"

void mouse_initialize(void);
void mouse_configure(TCaps *caps);
int mouse_getFd(void);
bool mouse_isEnable(void);
bool mouse_isTimeout(void);
int mouse_getX(void);
int mouse_getY(void);
void mouse_start(void);
void mouse_stop(void);
void mouse_getPackets(TVterm *p);

#endif /* INCLUDE_MOUSE_H */

