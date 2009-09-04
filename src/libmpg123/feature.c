#include "config.h"
#include "mpg123.h"

int mpg123_feature(const enum mpg123_feature_set key)
{
	switch(key)
	{
		case mpg123_feature_utf8open: return WANT_WIN32_UNICODE+0; /* Do we need to make sure its 1 or 0? */
		default: return 0;
	}
}
