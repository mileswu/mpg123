#include "config.h"
#if (WIN32 && WANT_WIN32_UNICODE)
#define WIN32_LEAN_AND_MEAN 1
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <winnls.h>

#include "mpg123.h"
#include "debug.h"
#include "win32conv.h"

int
win32_uni2mbc (const wchar_t * const wptr, const char **const mbptr,
	       size_t * const buflen)
{
  size_t len;
  char *buf;
  int ret;
  len = WideCharToMultiByte (CP_UTF8, 0, wptr, -1, NULL, 0, NULL, NULL);
  buf = calloc (len, sizeof (char));	/* Can we assume sizeof char always = 1 */
  debug2("win32_uni2mbc allocated %u bytes at %p", len, buf);
  if (buf)
    {
      ret =
	WideCharToMultiByte (CP_UTF8, 0, wptr, -1, buf, len,
			     NULL, NULL);
      *mbptr = buf;
      if (buflen)
	*buflen = len * sizeof (char);
      return ret;
    }
  else
    {
      if (buflen)
	*buflen = 0;
      return 0;
    }
}

int
win32_mbc2uni (const char *const mbptr, const wchar_t ** const wptr,
	       size_t * const buflen)
{
  size_t len;
  wchar_t *buf;
  int ret;
  len = MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS, mbptr, -1, NULL, 0);
  buf = calloc (len, sizeof (wchar_t));
  debug2("win32_uni2mbc allocated %u bytes at %p", len, buf);
  if (buf)
    {
      ret = MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS, mbptr, -1, buf, len);
      *wptr = buf;
      if (buflen)
	*buflen = len * sizeof (wchar_t);
      return ret;
    }
  else
    {
      if (buflen)
	*buflen = 0;
      return 0;
    }
}

#endif /* WIN32 && WANT_WIN32_UNICODE */

