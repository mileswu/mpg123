
#ifndef _MPG123_WIN32CONV_H_
#define _MPG123_WIN32CONV_H_
/**
 * win32_uni2mbc
 * Converts a null terminated UCS-2 string to a multibyte (UTF-8) equivalent.
 * Caller is supposed to free allocated buffer.
 * @param[in] wptr Pointer to wide string.
 * @param[out] mbptr Pointer to multibyte string.
 * @param[out] buflen Optional parameter for length of allocated buffer.
 * @return status of WideCharToMultiByte conversion.
 *
 * WideCharToMultiByte - http://msdn.microsoft.com/en-us/library/dd374130(VS.85).aspx
 */
int win32_uni2mbc (const wchar_t * const wptr, const char **const mbptr,
		   size_t * const buflen);

/**
 * win32_mbc2uni
 * Converts a null terminated UTF-8 string to a UCS-2 equivalent.
 * Caller is supposed to free allocated buffer.
 * @param[out] mbptr Pointer to multibyte string.
 * @param[in] wptr Pointer to wide string.
 * @param[out] buflen Optional parameter for length of allocated buffer.
 * @return status of WideCharToMultiByte conversion.
 *
 * MultiByteToWideChar - http://msdn.microsoft.com/en-us/library/dd319072(VS.85).aspx
 */

int win32_mbc2uni (const char *const mbptr, const wchar_t ** const wptr,
		   size_t * const buflen);

#endif

