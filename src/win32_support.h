/*
	Win32 support helper file

	This file is only for use with the mpg123 frontend.
	win32 support helpers for libmpg123 are in src/libmpg123/compat.h
*/
#ifndef MPG123_WIN32_SUPPORT_H
#define MPG123_WIN32_SUPPORT_H

#include "config.h"
#include "mpg123.h"
#ifdef HAVE_WINDOWS_H

#define WIN32_LEAN_AND_MEAN 1
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <winnls.h>
#include <shellapi.h>
#include <mmsystem.h>

#if defined (WANT_WIN32_SOCKETS) /*conflict with gethostname and select in select.h and unistd.h */
#include <ws2tcpip.h>
#define EINPROGRESS WSAEINPROGRESS
SOCKET win32_net_open_connection(mpg123_string *host, mpg123_string *port);
ssize_t win32_net_read (int fildes, void *buf, size_t nbyte);
ssize_t win32_net_write (int fildes, const void *buf, size_t nbyte);
void win32_net_init (void);
void win32_net_deinit (void);
void win32_net_replace (mpg123_handle *fr);
#endif

#ifdef WANT_WIN32_UNICODE
/**
 * Put the windows command line into argv / argc, encoded in UTF-8.
 * You are supposed to free up resources by calling win32_cmdline_free with the values you got from this one.
 * @return 0 on success, -1 on error */
int win32_cmdline_utf8(int * argc, char *** argv);

/**
 * Free up cmdline memory (the argv itself, theoretically hidden resources, too).
 */
void win32_cmdline_free(int argc, char **argv);

#endif /* WIN32_WANT_UNICODE */

/**
 * Set process priority
 * @param arg -2: Idle, -1, bellow normal, 0, normal (ignored), 1 above normal, 2 highest, 3 realtime
 */
void win32_set_priority (const int arg);

#endif /* HAVE_WINDOWS_H */
#endif /* MPG123_WIN32_SUPPORT_H */

