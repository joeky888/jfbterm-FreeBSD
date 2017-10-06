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

#if defined (__linux__) || defined (__NetBSD__)
#define LIBICONV_PLUG
#endif

#include <sys/types.h>
#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <iconv.h>
#ifndef LIBICONV_PLUG
#include <localcharset.h>
#endif
#include <langinfo.h>
#include <locale.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bell.h"
#include "console.h"
#include "csv.h"
#include "cursor.h"
#include "font.h"
#include "framebuffer.h"
#include "getcap.h"
#include "keyboard.h"
#include "main.h"
#include "mouse.h"
#include "palette.h"
#include "pcf.h"
#include "privilege.h"
#include "screensaver.h"
#ifdef ENABLE_SPLASH_SCREEN
#include "splash.h"
#endif
#include "term.h"
#include "utilities.h"
#include "vterm.h"

#define DEFAULT_ENCODING ("G0,G1,ansix3.4-1968,ansix3.4-1968,iso8859.1-1987,ansix3.4-1968")

#ifdef SYSCONFDIR
#define JFBTERM_CONF_PATH (SYSCONFDIR"/jfbterm.conf")
#else
#define JFBTERM_CONF_PATH ("/etc/jfbterm.conf")
#endif

static TApplication application;
static TApplication *const self = &application;
static bool initialized;

static void initialize(void);
static void finalize(void);
static void finalizer(void);
static void fatalErrorHandler(int signum);
static char *getShell(void);
static void getOptions(int argc, char *argv[]);
static void showHelp(FILE *stream);
static char *getEncoding(const char *value);

static void initialize(void)
{
	struct sigaction act;

	assert(!initialized);

	atexit(finalizer);
	initialized = true;
	setlocale(LC_ALL, "");
	privilege_initialize();
	self->help = false;
	self->reset = NULL;
	self->shell = getShell();
	self->args = NULL;
	self->configFile = JFBTERM_CONF_PATH;
	self->encoding = NULL;
	bzero(&act, sizeof(act));
	act.sa_handler = fatalErrorHandler;
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGBUS,  &act, NULL);
	sigaction(SIGFPE,  &act, NULL);
	sigaction(SIGHUP,  &act, NULL);
	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGILL,  &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGSYS,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	caps_initialize(&(self->caps));
}

static void finalize(void)
{
	struct sigaction act;

	assert(initialized);

	caps_finalize(&(self->caps));
	if (self->args != NULL) {
		free(self->args);
		self->args = NULL;
	}
	bzero(&act, sizeof(act));
	act.sa_handler = SIG_DFL;
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGBUS,  &act, NULL);
	sigaction(SIGFPE,  &act, NULL);
	sigaction(SIGHUP,  &act, NULL);
	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGILL,  &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGSYS,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

static void fatalErrorHandler(int signum)
{
	errx(1, "%s", sys_siglist[signum]);
	/* NOTREACHED */
}

static char *getShell(void)
{
	static char result[MAXPATHLEN];
	struct passwd *pw;
	char *shell;

	shell = getenv("SHELL");
	if (shell == NULL) {
		pw = getpwuid(privilege_getUID());
		if (pw != NULL)
			shell = pw->pw_shell;
	}
	if (shell == NULL || strlen(shell) == 0)
		shell = _PATH_BSHELL;
	strncpy(result, shell, MAXPATHLEN);
	result[MAXPATHLEN - 1] = '\0';
	return result;
}

static char *getEncoding(const char *value)
{
#ifdef ENABLE_OTHER_CODING_SYSTEM
	char *other;
	iconv_t cd;
#ifndef LIBICONV_PLUG
	const char *charset;
#endif
#endif
	const char *config, *codeset;
	char *encoding;

	assert(initialized);

	if (value != NULL)
		encoding = strdup(value);
	else
		encoding = strdup("locale");
	if (encoding == NULL)
		err(1, "strdup()");
	if (strcasecmp(encoding, "locale") == 0) {
		codeset = nl_langinfo(CODESET);
		if (codeset == NULL || strlen(codeset) == 0)
			errx(1, "Could not get codeset. "
			        "Please check your locale settings.");
		fprintf(stderr, "ENCODING : Current codeset is %s\n", codeset);
		free(encoding);
		encoding = strdup(codeset);
		if (encoding == NULL)
			err(1, "strdup()");
#ifdef ENABLE_OTHER_CODING_SYSTEM
#ifndef LIBICONV_PLUG
		config = caps_findEntry(&(self->caps), "encoding.", encoding);
		if (config == NULL) {
			charset = locale_charset();
			if (charset != NULL) {
				other = vterm_format_otherCS(charset);
				free(encoding);
				encoding = other;
			}
		}
#endif
#endif
	}
	if (strchr(encoding, ',') == NULL) {
		config = caps_findEntry(&(self->caps), "encoding.", encoding);
		if (config != NULL) {
			free(encoding);
			encoding = strdup(config);
			if (encoding == NULL)
				err(1, "strdup()");
		} else {
#ifdef ENABLE_OTHER_CODING_SYSTEM
			cd = iconv_open("UCS-2BE", encoding);
			if (cd != (iconv_t)-1) {
				iconv_close(cd);
				other = vterm_format_otherCS(encoding);
				free(encoding);
				encoding = other;
			} else
				errx(1, "Could not convert %s to UCS-2BE.",
				     encoding);
#else
			errx(1, "Invalid encoding %s.", encoding);
#endif
		}
	}
	return encoding;
}

static void getOptions(int argc, char *argv[])
{
	static const struct option longopts[] = {
		{ "shell",     required_argument, NULL, 'e' },
		{ "exec",      required_argument, NULL, 'e' },
		{ "config",    required_argument, NULL, 'f' },
		{ "encoding",  required_argument, NULL, 'c' },
		{ "charmap",   required_argument, NULL, 'c' },
		{ "ambiguous", required_argument, NULL, 'a' },
		{ "reset",     optional_argument, NULL, 'r' },
		{ "help",      no_argument,       NULL, 'h' },
		{ NULL,        0,                 NULL, 0   },
	};
	int i, c;

	assert(initialized);
	assert(argc >= 0);
	assert(argv != NULL);

	while ((c = getopt_long(argc, argv, "c:f:e:a:hr::", longopts,
				NULL)) != EOF) {
		switch (c) {
		case 'e':
			self->shell = optarg;
			goto end;
		case 'f':
			/* Same as issetugid(2) */
			if (!privilege_isSetUID() && !privilege_isSetGID())
				self->configFile = optarg;
			else
				warnx("This process is currently running "
				      "setuid or setgid. "
				      "--config or -f option skipped.");
			break;
		case 'c':
			self->encoding = optarg;
			break;
		case 'a':
			if (optarg != NULL) {
				self->ambiguous = lookup(true, optarg,
							 "unchanged",
							 "single",
							 "double",
							 (char *)NULL);
				if (self->ambiguous == -1)
					self->ambiguous = atoi(optarg);
			} else
				self->ambiguous = 0;
			break;
		case 'r':
			if (optarg != NULL)
				self->reset = optarg;
			else
				self->reset = "locale";
			break;
		case 'h':
			self->help = true;
			break;
		default:
			break;
		}
	}
 end:
	self->args = calloc(argc + 2 - optind, sizeof(char *));
	if (self->args == NULL)
		err(1, "calloc()");
	self->args[0] = self->shell;
	for (i = 0; i < argc + 1 - optind; i++)
		self->args[i + 1] = argv[i + optind];
}

static void showHelp(FILE *stream)
{
	assert(initialized);

	fputs("\tCopyright (C) 2007-2009 Yusuke BABA\n"
	      "This program is based on :\n"
	      "JFBTERM/ME - J framebuffer terminal/Multilingual Enhancement -\n"
	      "\tCopyright (C) 2003 Fumitoshi UKAI\n"
	      "\tCopyright (C) 1999-2000 Noritoshi MASUICHI\n"
	      "Unofficial eXtension Patch for JFBTERM\n"
	      "\tCopyright (C) 2006 Young Chol Song\n"
	      "KON2 - Kanji ON Console -\n"
	      "\tCopyright (C) 1992,1993 Atusi MAEDA\n"
	      "\tCopyright (C) 1992-1996 Takashi MANABE\n"
	      "KON2 for FreeBSD\n"
	      "\tCopyright (C) Takashi OGURA\n", stream);
	fputs("[ BPP ]\n"
#ifdef ENABLE_8BPP
		" 8bpp"
#endif
#ifdef ENABLE_15BPP
		" 15bpp"
#endif
#ifdef ENABLE_16BPP
		" 16bpp"
#endif
#ifdef ENABLE_24BPP
		" 24bpp"
#endif
#ifdef ENABLE_32BPP
		" 32bpp"
#endif
		, stream);
	fputs("\n", stream);
	fputs("[ FONTSET ]\n", stream);
	font_showList(stream);
	fputs("[ ENCODING ]\n", stream);
	fputs(" ISO-2022-*", stream);
#ifdef ENABLE_UTF8
	fputs(" UTF-8", stream);
#endif
#ifdef ENABLE_OTHER_CODING_SYSTEM
	fputs(" OTHER-CODING-SYSTEM", stream);
#endif
	fputs("\n", stream);
	fputs("[ MISC ]\n"
#ifdef DEBUG
		" DEBUG"
#endif
		, stream);
	fputs("\n", stream);
}

int main(int argc, char *argv[])
{
	const char *config, *term;
	char *encoding;
	u_int history;
#if DEBUG
	int i;
#endif

	initialize();
	fprintf(stderr, "JFBTERM for FreeBSD Version %s\n", VERSION);
	getOptions(argc, argv);
	if (self->help) {
		showHelp(stderr);
		return EXIT_SUCCESS;
	}
	caps_readFile(&(self->caps), self->configFile);
	if (self->reset != NULL) {
		FILE *stream;
		encoding = getEncoding(self->reset);
		if ((stream = fopen("/dev/tty", "w")) != NULL) {
			vterm_show_sequence(stream, encoding);
			fclose(stream);
		} else
			vterm_show_sequence(stderr, encoding);
		fprintf(stderr, "ENCODING : %s\n", encoding);
		free(encoding);
		return EXIT_SUCCESS;
	}
	font_initialize();
	font_configure(&(self->caps));
	config = caps_findFirst(&(self->caps), "term");
	if (config != NULL)
		term = config;
	else
		term = "jfbterm";
	config = caps_findFirst(&(self->caps), "history");
	if (config != NULL) {
		history = atoi(config);
		if (history >= 10000)
			history = 9999;
	} else
		history = 0;
	config = caps_findFirst(&(self->caps), "encoding");
	if (self->encoding != NULL)
		config = self->encoding;
	encoding = getEncoding(config);
	fprintf(stderr, "ENCODING : %s\n", encoding);
#if DEBUG
	fprintf(stderr, "EXECUTE : %s ", self->shell);
	for (i = 1; self->args[i] != NULL; i++)
		fprintf(stderr, "%s ", self->args[i]);
	fputs("\n", stderr);
#endif
	console_initialize();
	framebuffer_initialize();
	framebuffer_configure(&(self->caps));
	framebuffer_open();
#ifdef ENABLE_SPLASH_SCREEN
	splash_initialize();
	splash_configure(&(self->caps));
	splash_load();
#endif
	mouse_initialize();
	mouse_configure(&(self->caps));
	cursor_initialize();
	cursor_configure(&(self->caps));
	bell_initialize();
	bell_configure(&(self->caps));
	palette_configure(&(self->caps));
	screensaver_initialize();
	screensaver_configure(&(self->caps));
	keyboard_initialize();
	keyboard_configure(&(self->caps));
	term_initialize(&(self->caps), history, encoding, self->ambiguous);
	free(encoding);
	term_execute(self->shell, self->args, term);
	/* NOTREACHED */
	return EXIT_SUCCESS;
}

