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

#ifndef INCLUDE_VTERMLOW_H
#define INCLUDE_VTERMLOW_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#include "vterm.h"

void vterm_unclean(TVterm *p);
void vterm_delete_n_chars(TVterm *p, int n);
void vterm_insert_n_chars(TVterm *p, int n);
void vterm_refresh(TVterm *p);
void vterm_sput(TVterm *p, u_int fontIndex, u_char c, u_char raw);
void vterm_wput(TVterm *p, u_int fontIndex, u_char c1, u_char c2, u_char raw1, u_char raw2);
#ifdef ENABLE_UTF8
void vterm_uput1(TVterm *p, u_int fontIndex, uint16_t ucs2, uint16_t raw);
void vterm_uput2(TVterm *p, u_int fontIndex, uint16_t ucs2, uint16_t raw);
#endif
void vterm_text_clear_all(TVterm *p);
void vterm_text_clear_eol(TVterm *p, int mode);
void vterm_text_clear_eos(TVterm *p, int mode);
void vterm_text_scroll_down(TVterm *p, int line);
void vterm_text_move_down(TVterm *p, u_int top, u_int bottom, int line);
void vterm_text_scroll_up(TVterm *p, int line);
void vterm_text_move_up(TVterm *p, u_int top, u_int bottom, int line);
void vterm_scroll_reset(TVterm *p);
void vterm_scroll_forward_line(TVterm *p);
void vterm_scroll_backward_line(TVterm *p);
void vterm_scroll_forward_page(TVterm *p);
void vterm_scroll_backward_page(TVterm *p);
void vterm_reverseText(TVterm *p, u_int sx, u_int sy, u_int ex, u_int ey);
void vterm_copyText(TVterm *p, u_int sx, u_int sy, u_int ex, u_int ey);
void vterm_pasteText(TVterm *p);
void vterm_pollCursor(TVterm *p, bool wakeup);

#endif /* INCLUDE_VTERMLOW_H */

