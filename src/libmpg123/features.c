#include "config.h"
#include "mpg123.h"

int mpg123_features(const enum mpg123_feature_set key)
{
	switch(key)
	{
		case mpg123_features_win32_wopen: return WANT_WIN32_UNICODE+0; break; /* Do we need to make sure its 1 or 0? */
		case mpg123_features_64bit_files: return !(!(_FILE_OFFSET_BITS+0)); break;
		default: return 0;
	}
}
