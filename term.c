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
 * KON2 for FreeBSD
 * Copyright (C) Takashi OGURA
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

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#ifdef HAVE_UTMP_H
#include <utmp.h>
#else
#include <utmpx.h>
#endif

#if defined (__linux__)
#include <sys/vt.h>
#include <pty.h>
#elif defined (__FreeBSD__)
#include <osreldate.h>
#if __FreeBSD_version >= 410000
#include <sys/consio.h>
#else
#include <machine/console.h>
#endif
#include <sys/ttycom.h>
#include <libutil.h>
#elif defined (__NetBSD__) || defined (__OpenBSD__)
#include <sys/ttycom.h>
#include <util.h>
#ifndef _DEV_WSCONS_WSDISPLAY_USL_IO_H_
#include <dev/wscons/wsdisplay_usl_io.h>
#define _DEV_WSCONS_WSDISPLAY_USL_IO_H_
#endif
#endif

#include "console.h"
#include "font.h"
#include "framebuffer.h"
#include "getcap.h"
#include "keyboard.h"
#include "main.h"
#include "mouse.h"
#include "palette.h"
#include "privilege.h"
#include "screensaver.h"
#include "term.h"
#include "utilities.h"
#include "vterm.h"
#include "vtermlow.h"

static TTerm term;
static TTerm *const self = &term;
static pid_t child;
static fd_set orgReadFds;
static int numFds;
static bool initialized;
static struct termios* termios = NULL;

static void finalize(void);
static void finalizer(void);
static void acquireVirtualTerminal(int signum);
static void releaseVirtualTerminal(int signum);
static void childHandler(int signum);
static bool openPseudoTerminal(void);
static void enableMouse(void);
static void disableMouse(void);
static void executeShell(const char *shell, char *const args[],
			 const char *name);
static void consoleHandler(void);
static void writeLoginRecord(void);
static void eraseLoginRecord(void);
size_t strrep(u_char *p, size_t len, size_t maxlen, size_t oldlen,
	      const char *newstr);
static size_t applicationCursor(u_char *buf, size_t len, size_t maxlen);
#if defined (__FreeBSD__)
static size_t sysconsTolinux(u_char *buf, size_t len, size_t maxlen);
#endif
#if defined (__NetBSD__) || defined (__OpenBSD__)
static size_t wsconsTolinux(u_char *buf, size_t len, size_t maxlen);
#endif

void term_initialize(TCaps *caps, u_int history, const char *encoding,
		     int ambiguous)
{
	struct sigaction act;

	assert(!initialized);
	assert(caps != NULL);
	assert(encoding != NULL);

	atexit(finalizer);
	initialized = true;
	self->masterPty = -1;
	self->slavePty  = -1;
	self->device[0] = '\0';
	termios = malloc(sizeof(struct termios));
	if (termios == NULL)
		err(1, "malloc()");
	if (tcgetattr(STDIN_FILENO, termios) == -1) {
		free(termios);
		termios = NULL;
		err(1, "tcgetattr()");
	}
	if (!openPseudoTerminal())
		errx(1, "Could not open pseudo-terminal.");
	vterm_initialize(&(self->vterm), self, caps, history,
			 gFramebuffer.width / gFontsWidth,
			 gFramebuffer.height / gFontsHeight,
			 encoding, ambiguous);
	bzero(&act, sizeof(act));
	act.sa_handler = childHandler;
	act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
	sigaction(SIGCHLD, &act, NULL);
	console_setAcquireHandler(acquireVirtualTerminal);
	console_setReleaseHandler(releaseVirtualTerminal);
}

static void finalize(void)
{
	struct sigaction act;

	assert(initialized);

	eraseLoginRecord();
	vterm_finalize(&(self->vterm));
	if (self->slavePty != -1) {
		close(self->slavePty);
		self->slavePty = -1;
	}
	if (self->masterPty != -1) {
		close(self->masterPty);
		self->masterPty = -1;
	}
	bzero(&act, sizeof(act));
	act.sa_handler = SIG_DFL;
	sigaction(SIGCHLD, &act, NULL);
	initialized = false;
}

static void finalizer(void)
{
	finalize();
}

static void acquireVirtualTerminal(int signum)
{
	UNUSED_VARIABLE(signum);
	if (!((&self->vterm))->active) {
		framebuffer_reset();
		((&self->vterm))->active = true;
		vterm_unclean(&(self->vterm));
		((&self->vterm))->textClear = true;
		vterm_refresh(&(self->vterm));
		if (mouse_isEnable()) {
			mouse_start();
			enableMouse();
		}
		screensaver_execute(false);
		palette_reset();
#if defined (__linux__)
		keyboard_enableScrollBack(false);
#endif
	}
}

static void releaseVirtualTerminal(int signum)
{
	UNUSED_VARIABLE(signum);
	((&self->vterm))->active = false;
	if (mouse_isEnable()) {
		disableMouse();
		mouse_stop();
	}
#if defined (__NetBSD__) || defined (__OpenBSD__)
	palette_restore();
#endif
	vterm_scroll_reset(&(self->vterm));
#if defined (__linux__)
	keyboard_enableScrollBack(true);
#endif
}

static void childHandler(int signum)
{
	int status, errsv;
	pid_t pid;

	UNUSED_VARIABLE(signum);
	errsv = errno;
	pid = waitpid(child, &status, WNOHANG);
	if (pid == (pid_t)-1 && errno != ECHILD)
		err(1, "waitpid()");
	if (pid == child || (pid == (pid_t)-1 && errno == ECHILD)) {
		if (WIFSIGNALED(status))
			errx(1, "Child process died with signal %s",
			     sys_siglist[WTERMSIG(status)]);
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
			errx(1, "Child process exited with status %d",
			     WEXITSTATUS(status));
		fputs("Finished.\n", stderr);
		exit(EXIT_SUCCESS);
	}
	errno = errsv;
}

static bool openPseudoTerminal(void)
{
	int status;
	char *device;

	assert(initialized);

	/* openpty(3) don't use thrid argument. */
	privilege_on();
	status = openpty(&self->masterPty, &self->slavePty, NULL, NULL, NULL);
	privilege_off();
	if (status == -1)
		return false;
	device = ttyname(self->slavePty);
	if (device == NULL)
		return false;
	strncpy(self->device, device, MAXPATHLEN);
	self->device[MAXPATHLEN - 1] = '\0';
	return true;
}

static void enableMouse(void)
{
	int mouseFd;

	assert(initialized);

	mouseFd = mouse_getFd();
	if (mouseFd != -1)
		FD_SET(mouseFd, &orgReadFds);
	if (mouseFd > self->masterPty)
		numFds = mouseFd + 1;
	else
		numFds = self->masterPty + 1;
}

static void disableMouse(void)
{
	int mouseFd;

	assert(initialized);

	mouseFd = mouse_getFd();
	if (mouseFd != -1)
		FD_CLR(mouseFd, &orgReadFds);
	numFds = self->masterPty + 1;
}

void term_execute(const char *shell, char *const args[], const char *name)
{
	struct termios *new_termios;
	int on;

	assert(initialized);
	assert(shell != NULL);
	assert(args != NULL);
	assert(name != NULL);

	fflush(stdout);
	fflush(stderr);
	child = fork();
	if (child == (pid_t)-1)
		err(1, "fork()");
	else if (child == (pid_t)0) {
		/* Child process */
		executeShell(shell, args, name);
	} else {
		/* Parent process */
		new_termios = malloc(sizeof(struct termios));
		if (new_termios == NULL)
			err(1, "malloc()");
		memcpy(new_termios, termios, sizeof(struct termios));
		cfmakeraw(new_termios);
		new_termios->c_oflag = (OPOST | ONLCR);
#if defined (__linux__)
		new_termios->c_line = 0;
#endif
		if (tcsetattr(STDIN_FILENO, TCSANOW, new_termios) == -1)
			err(1, "tcsetattr()");
		free(new_termios);
		on = 1;
		if (ioctl(self->masterPty, TIOCPKT, &on) == -1)
			err(1, "ioctl(TIOCPKT)");
		consoleHandler();
	}
	/* NOTREACHED */
}

static void executeShell(const char *shell, char *const args[],
			 const char *name)
{
	int i;

	assert(initialized);
	assert(shell != NULL);
	assert(args != NULL);
	assert(name != NULL);

	setenv("TERM", name, 1);
	close(self->masterPty);
	self->masterPty = -1;
	if (login_tty(self->slavePty) == -1)
		err(1, "login_tty()");
	if (tcsetattr(STDIN_FILENO, TCSANOW, termios) == -1)
		err(1, "tcsetattr()");
	writeLoginRecord();
	privilege_drop();
	for (i = getdtablesize(); i > STDERR_FILENO; i--)
		close(i);
	execvp(shell, args);
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}

#define BUF_SIZE (1024)

static void consoleHandler(void)
{
	int r;
	ssize_t nbytes;
	struct timeval timeout;
	u_char buf[BUF_SIZE + 1];
	fd_set readFds;

	assert(initialized);

#if defined (__linux__)
	keyboard_enableScrollBack(false);
#endif
	FD_ZERO(&orgReadFds);
	FD_SET(STDIN_FILENO, &orgReadFds);
	FD_SET(self->masterPty, &orgReadFds);
	if (mouse_isEnable())
		mouse_start();
	if (mouse_getFd() != -1)
		enableMouse();
	else
		disableMouse();
	for (;;) {
		do {
			/* Idle loop. */
			vterm_pollCursor(&(self->vterm), false);
			readFds = orgReadFds;
			timeout.tv_sec  = 0;
			timeout.tv_usec = 100000; /* 0.1 sec. */
			r = select(numFds, &readFds, NULL, NULL, &timeout);
		} while (r == 0 || (r == -1 && (errno == EINTR || mouse_getFd() == -1)));
		if (r == -1)
			err(1, "select()");
		if (FD_ISSET(STDIN_FILENO, &readFds)) {
			nbytes = read(STDIN_FILENO, buf, BUF_SIZE);
#if defined (__FreeBSD__)
			nbytes = sysconsTolinux(buf, nbytes, BUF_SIZE);
#elif defined (__NetBSD__) || defined (__OpenBSD__)
			nbytes = wsconsTolinux(buf, nbytes, BUF_SIZE);
#endif
			if (((&self->vterm))->cursor)
				nbytes = applicationCursor(buf, nbytes,
							   BUF_SIZE);
			if (nbytes > 0) {
				/* Scroll Lock + Page Up */
				bool scrollLocked, shiftPressed;
				scrollLocked = keyboard_isScrollLocked();
				shiftPressed = keyboard_isShiftPressed();
				if ((scrollLocked || shiftPressed) &&
				    (buf[0] == '\033' && buf[1] == '[' &&
				     buf[2] == '5' && buf[3] == '~')) {
					vterm_scroll_backward_page(
							&(self->vterm));
					continue;
				}
				/* Scroll Lock + Page Down */
				else if ((scrollLocked || shiftPressed) &&
					 (buf[0] == '\033' && buf[1] == '[' &&
					  buf[2] == '6' && buf[3] == '~')) {
					vterm_scroll_forward_page(
							&(self->vterm));
					continue;
				}
				/* Scroll Lock + Up */
				else if ((scrollLocked || shiftPressed) &&
					 ((buf[0] == '\033' && buf[1] == '[' &&
					   buf[2] == 'A') ||
					  (buf[0] == '\033' && buf[1] == 'O' &&
					   buf[2] == 'A'))) {
					vterm_scroll_backward_line(
							&(self->vterm));
					continue;
				}
				/* Scroll Lock + Down */
				else if ((scrollLocked || shiftPressed) &&
					 ((buf[0] == '\033' && buf[1] == '[' &&
					   buf[2] == 'B') ||
					  (buf[0] == '\033' && buf[1] == 'O' &&
					   buf[2] == 'B'))) {
					vterm_scroll_forward_line(
							&(self->vterm));
					continue;
				}
				write_wrapper(self->masterPty, buf, nbytes);
			}
			vterm_pollCursor(&(self->vterm), true);
		}
		if (FD_ISSET(self->masterPty, &readFds)) {
			bool scrollLocked;
			scrollLocked = keyboard_isScrollLocked();
			if (scrollLocked)
				usleep(10000); /* 0.01 sec. */
			else {
				nbytes = read(self->masterPty, buf, BUF_SIZE);
				if (nbytes > 0 && buf[0] == TIOCPKT_DATA) {
					vterm_scroll_reset(&(self->vterm));
					vterm_emulate(&(self->vterm),
						      &buf[1], nbytes - 1);
					vterm_refresh(&(self->vterm));
				}
			}
		}
		if (mouse_isEnable() && mouse_getFd() != -1) {
			if (FD_ISSET(mouse_getFd(), &readFds) &&
			    ((&self->vterm))->active) {
				mouse_getPackets(&(self->vterm));
				vterm_pollCursor(&(self->vterm), true);
			}
		}
	}
	/* NOTREACHED */
}

static void writeLoginRecord(void)
{
#if defined (__linux__)
	struct utmp utmp;
	struct passwd *pw;
	char *tn;

	assert(initialized);

	bzero(&utmp, sizeof(utmp));
	pw = getpwuid(privilege_getUID());
	if (pw == NULL) {
		warnx("who are you?");
		return; /* give up */
	}
	if (strncmp(self->device, "/dev/pts/", 9) == 0) {
		/* Unix98 style: "/dev/pts/\*" */
		tn = self->device + strlen("/dev/pts/");
		if (strlen(tn) < 1)
			return; /* bad format */
		snprintf(utmp.ut_id, sizeof(utmp.ut_id), "p%s", tn);
	} else if (strncmp(self->device, "/dev/tty", 8) == 0) {
		/* BSD style: "/dev/tty[p-za-e][0-9a-f]" */
		tn = self->device + strlen("/dev/tty");
		if (strlen(tn) < 2)
			return; /* bad format */
		strncpy(utmp.ut_id, tn, sizeof(utmp.ut_id));
	} else
		return; /* bad format */
	utmp.ut_type = DEAD_PROCESS;
	privilege_on();
	setutent();
	getutid(&utmp);
	utmp.ut_type = USER_PROCESS;
	utmp.ut_pid = getpid();
	tn = self->device + strlen("/dev/");
	strncpy(utmp.ut_line, tn, sizeof(utmp.ut_line));
	strncpy(utmp.ut_user, pw->pw_name, sizeof(utmp.ut_user));
	time(&(utmp.ut_time));
	pututline(&utmp);
	endutent();
	privilege_off();
#elif defined (__FreeBSD__) && (__FreeBSD_version >= 900007)
	struct utmpx utmp;
	struct passwd *pw;
	char *tn;

	assert(initialized);

	bzero(&utmp, sizeof(utmp));
	pw = getpwuid(privilege_getUID());
	if (pw == NULL) {
		warnx("who are you?");
		return; /* give up */
	}
	if (strncmp(self->device, "/dev/pts/", 9) == 0 ||
	    strncmp(self->device, "/dev/tty", 8) == 0) {
		/* Unix98 style: "/dev/pts/\*" */
		/* BSD style: "/dev/tty[l-sL-S][0-9a-v]" */
		tn = self->device + strlen("/dev/");
		if (strlen(tn) < 5)
			return; /* bad format */
		strncpy(utmp.ut_id, tn, sizeof(utmp.ut_id));
	} else
		return; /* bad format */
	utmp.ut_type = DEAD_PROCESS;
	privilege_on();
	setutxent();
	getutxid(&utmp);
	utmp.ut_type = USER_PROCESS;
	utmp.ut_pid = getpid();
	strncpy(utmp.ut_line, tn, sizeof(utmp.ut_line));
	strncpy(utmp.ut_user, pw->pw_name, sizeof(utmp.ut_user));
	gettimeofday(&(utmp.ut_tv), NULL);
	pututxline(&utmp);
	endutxent();
	privilege_off();
#elif defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__)
	struct utmp utmp;
	struct passwd *pw;
	char *tn;
	int fd, ts;

	assert(initialized);

	bzero(&utmp, sizeof(utmp));
	pw = getpwuid(privilege_getUID());
	if (pw == NULL) {
		warnx("who are you?");
		return; /* give up */
	}
	if (strncmp(self->device, "/dev/tty", 8) == 0) {
		/* BSD style: "/dev/tty[p-sP-S][0-9a-v]" */
		tn = self->device + strlen("/dev/");
		if (strlen(tn) < 5)
			return; /* bad format */
	} else
		return; /* bad format */
	strncpy(utmp.ut_line, tn, sizeof(utmp.ut_line));
	strncpy(utmp.ut_name, pw->pw_name, sizeof(utmp.ut_name));
	time(&(utmp.ut_time));
	if ((ts = ttyslot()) == 0)
		return; /* give up */
	privilege_on();
	fd = open(_PATH_UTMP, O_WRONLY | O_CREAT, 0644);
	privilege_off();
	if (fd != -1) {
		lseek(fd, (ts * sizeof(struct utmp)), L_SET);
		write(fd, &utmp, sizeof(struct utmp));
		close(fd);
	}
#else
	#error not implement
#endif
}

static void eraseLoginRecord(void)
{
#if defined (__linux__)
	struct utmp utmp, *utp;
	char *tn;

	assert(initialized);

	bzero(&utmp, sizeof(utmp));
	if (strncmp(self->device, "/dev/pts/", 9) == 0) {
		/* Unix98 style: "/dev/pts/\*" */
		tn = self->device + strlen("/dev/pts/");
		if (strlen(tn) < 1)
			return; /* bad format */
		snprintf(utmp.ut_id, sizeof(utmp.ut_id), "p%s", tn);
	} else if (strncmp(self->device, "/dev/tty", 8) == 0) {
		/* BSD style: "/dev/tty[p-za-e][0-9a-f]" */
		tn = self->device + strlen("/dev/tty");
		if (strlen(tn) < 2)
			return; /* bad format */
		strncpy(utmp.ut_id, tn, sizeof(utmp.ut_id));
	} else
		return; /* bad format */
	utmp.ut_type = USER_PROCESS;
	privilege_on();
	setutent();
	utp = getutid(&utmp);
	if (utp == NULL) {
		endutent();
		privilege_off();
		return; /* give up */
	}
	utp->ut_type = DEAD_PROCESS;
	bzero(utp->ut_user, sizeof(utmp.ut_user));
	time(&(utp->ut_time));
	pututline(utp);
	endutent();
	privilege_off();
#elif defined (__FreeBSD__) && (__FreeBSD_version >= 900007)
	struct utmpx utmp, *utp;
	char *tn;

	assert(initialized);

	bzero(&utmp, sizeof(utmp));
	if (strncmp(self->device, "/dev/pts/", 9) == 0 ||
	    strncmp(self->device, "/dev/tty", 8) == 0) {
		/* Unix98 style: "/dev/pts/\*" */
		/* BSD style: "/dev/tty[l-sL-S][0-9a-v]" */
		tn = self->device + strlen("/dev/");
		if (strlen(tn) < 5)
			return; /* bad format */
		strncpy(utmp.ut_id, tn, sizeof(utmp.ut_id));
	} else
		return; /* bad format */
	utmp.ut_type = USER_PROCESS;
	privilege_on();
	setutxent();
	utp = getutxid(&utmp);
	if (utp == NULL) {
		endutxent();
		privilege_off();
		return; /* give up */
	}
	utp->ut_type = DEAD_PROCESS;
	bzero(utp->ut_user, sizeof(utmp.ut_user));
	gettimeofday(&(utp->ut_tv), NULL);
	pututxline(utp);
	endutxent();
	privilege_off();
#elif defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__)
	struct utmp utmp;
	char *tn;
	int fd, ts;

	assert(initialized);

	if (strncmp(self->device, "/dev/tty", 8) == 0) {
		/* BSD style: "/dev/tty[p-sP-S][0-9a-v]" */
		tn = self->device + strlen("/dev/");
		if (strlen(tn) < 5)
			return; /* bad format */
	} else
		return; /* bad format */
	privilege_on();
	fd = open(_PATH_UTMP, O_RDWR);
	privilege_off();
	if (fd != -1) {
		for (ts = 0; read(fd, &utmp, sizeof(struct utmp)) ==
		     sizeof(struct utmp); ts++) {
			if (strncmp(utmp.ut_line, tn, strlen(tn)) == 0) {
				bzero(utmp.ut_name, sizeof(utmp.ut_name));
				bzero(utmp.ut_host, sizeof(utmp.ut_host));
				time(&utmp.ut_time);
				lseek(fd, (ts * sizeof(struct utmp)), L_SET);
				write(fd, &utmp, sizeof(struct utmp));
				break;
			}
		}
		close(fd);
	}
#else
	#error not implement
#endif
}

size_t strrep(u_char *p, size_t len, size_t maxlen, size_t oldlen,
	      const char *newstr)
{
	size_t newlen;
	ssize_t copylen, diff;

	assert(p != NULL);
	assert(newstr != NULL);

	newlen = strlen(newstr);
	diff = newlen - oldlen;
	copylen = len - oldlen;
	if (len + diff > maxlen) {
		diff -= (len + diff - maxlen);
		copylen = maxlen - newlen;
	}
	if (copylen > 0)
		memmove(p + newlen, p + oldlen, copylen);
	copylen = newlen;
	if (newlen > maxlen) {
		copylen = maxlen;
		diff = maxlen - len;
	}
	if (copylen > 0)
		memcpy(p, newstr, copylen);
	return diff;
}

static size_t applicationCursor(u_char *buf, size_t len, size_t maxlen)
{
	u_char *p;
	size_t i, newlen;

	assert(initialized);
	assert(buf != NULL);

	newlen = len;
	for (i = 0, p = buf; i < newlen; i++, p++) {
		switch (*p) {
		case '\033':      /* ESC */
			if (i + 2 <= newlen && *(p + 1) == '[') {
				switch (*(p + 2)) {
				case 'A':        /* Up */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033OA");
					i += 2; p += 2;
					break;
				case 'B':        /* Down */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033OB");
					i += 2; p += 2;
					break;
				case 'C':        /* Right */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033OC");
					i += 2; p += 2;
					break;
				case 'D':        /* Left */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033OD");
					i += 2; p += 2;
					break;
				default:
					i += 2; p += 2;
					break;
				}
			}
		default:
			break;
		}
	}
	return newlen;
}

#if defined (__FreeBSD__)
static size_t sysconsTolinux(u_char *buf, size_t len, size_t maxlen)
{
	u_char *p;
	size_t i, newlen;

	assert(initialized);
	assert(buf != NULL);

	newlen = len;
	for (i = 0, p = buf; i < newlen; i++, p++) {
		switch (*p) {
		case '\177':      /* Delete */
			newlen += strrep(p, len - i, maxlen - i, 1, "\033[3~");
			i += 3; p += 3;
			break;
		case '\033':      /* ESC */
			if (i + 2 <= newlen && *(p + 1) == '[') {
				switch (*(p + 2)) {
				case 'E':        /* Center (Num Lock + 5) */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[G");
					i += 2; p += 2;
					break;
				case 'H':        /* Home */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[1~");
					i += 3; p += 3;
					break;
				case 'L':        /* Insert */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[2~");
					i += 3; p += 3;
					break;
				case 'F':        /* End */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[4~");
					i += 3; p += 3;
					break;
				case 'I':        /* Page Up */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[5~");
					i += 3; p += 3;
					break;
				case 'G':        /* Page Down */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[6~");
					i += 3; p += 3;
					break;
				case 'M':        /* F1 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[[A");
					i += 3; p += 3;
					break;
				case 'N':        /* F2 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[[B");
					i += 3; p += 3;
					break;
				case 'O':        /* F3 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[[C");
					i += 3; p += 3;
					break;
				case 'P':        /* F4 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[[D");
					i += 3; p += 3;
					break;
				case 'Q':        /* F5 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[[E");
					i += 3; p += 3;
					break;
				case 'R':        /* F6 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[17~");
					i += 4; p += 4;
					break;
				case 'S':        /* F7 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[18~");
					i += 4; p += 4;
					break;
				case 'T':        /* F8 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[19~");
					i += 4; p += 4;
					break;
				case 'U':        /* F9 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[20~");
					i += 4; p += 4;
					break;
				case 'V':        /* F10 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[21~");
					i += 4; p += 4;
					break;
				case 'W':        /* F11 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[23~");
					i += 4; p += 4;
					break;
				case 'X':        /* F12 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[24~");
					i += 4; p += 4;
					break;
				case 'Y':        /* F13 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[25~");
					i += 4; p += 4;
					break;
				case 'Z':        /* F14 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[26~");
					i += 4; p += 4;
					break;
				case 'a':        /* F15 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[28~");
					i += 4; p += 4;
					break;
				case 'b':        /* F16 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[29~");
					i += 4; p += 4;
					break;
				case 'c':        /* F17 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[31~");
					i += 4; p += 4;
					break;
				case 'd':        /* F18 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[32~");
					i += 4; p += 4;
					break;
				case 'e':        /* F19 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[33~");
					i += 4; p += 4;
					break;
				case 'f':        /* F20 */
					newlen += strrep(p, len - i, maxlen - i,
							 3, "\033[34~");
					i += 4; p += 4;
					break;
				default:
					i += 2; p += 2;
					break;
				}
			}
		default:
			break;
		}
	}
	return newlen;
}
#endif

#if defined (__NetBSD__) || defined (__OpenBSD__)
static size_t wsconsTolinux(u_char *buf, size_t len, size_t maxlen)
{
	u_char *p;
	size_t i, newlen;
	int n;

	assert(initialized);
	assert(buf != NULL);

	newlen = len;
	for (i = 0, p = buf; i < newlen; i++, p++) {
		switch (*p) {
#if defined (__NetBSD__)
		case '\177':      /* Delete */
			newlen += strrep(p, len - i, maxlen - i, 1, "\033[3~");
			i += 3; p += 3;
			break;
#endif
		case '\033':      /* ESC */
			if (i + 3 <= newlen && *(p + 1) == '[' &&
			    *(p + 3) == '~') {
				switch (*(p + 2)) {
				case '7':        /* Home */
					newlen += strrep(p, len - i, maxlen - i,
							 4, "\033[1~");
					i += 3; p += 3;
					break;
				case '8':        /* End */
					newlen += strrep(p, len - i, maxlen - i,
							 4, "\033[4~");
					i += 3; p += 3;
					break;
				default:
					i += 3; p += 3;
					break;
				}
			} else if (i + 4 <= newlen && *(p + 1) == '[' &&
				   isdigit(*(p + 2)) && isdigit(*(p + 3)) &&
				   *(p + 4) == '~') {
				n = hex2int(*(p + 2)) * 10 + hex2int(*(p + 3));
				switch (n) {
				case 11:         /* F1 */
					newlen += strrep(p, len - i, maxlen - i,
							 5, "\033[[A");
					i += 3; p += 3;
					break;
				case 12:         /* F2 */
					newlen += strrep(p, len - i, maxlen - i,
							 5, "\033[[B");
					i += 3; p += 3;
					break;
				case 13:         /* F3 */
					newlen += strrep(p, len - i, maxlen - i,
							 5, "\033[[C");
					i += 3; p += 3;
					break;
				case 14:         /* F4 */
					newlen += strrep(p, len - i, maxlen - i,
							 5, "\033[[D");
					i += 3; p += 3;
					break;
				case 15:         /* F5 */
					newlen += strrep(p, len - i, maxlen - i,
							 5, "\033[[E");
					i += 3; p += 3;
					break;
				default:
					i += 4; p += 4;
					break;
				}
			}
		default:
			break;
		}
	}
	return newlen;
}
#endif

