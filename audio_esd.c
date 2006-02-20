#include <esd.h>

#include "mpg123.h"

int audio_open(struct audio_info_struct *ai)
{

    int bits = ESD_BITS16;
    int mode = ESD_STREAM, func = ESD_PLAY ;
    
    esd_format_t format = 0;

    format = (bits | mode | func) & (~ESD_MASK_CHAN);
    if( ai->channels == 1 )
    	format |= ESD_MONO;
    else
    	format |= ESD_STEREO;
    
    printf( "opening socket, format = 0x%08x at %ld Hz\n", 
	    format, ai->rate );
   
    /* sock = esd_play_stream( format, ai->rate ); */
    ai->fn = esd_play_stream_fallback( format, ai->rate, NULL, "mpg123" );
    return (ai->fn);

}

int audio_reset_parameters(struct audio_info_struct *ai)
{
  return 0;
}

int audio_rate_best_match(struct audio_info_struct *ai)
{
  return 0;
}

int audio_set_rate(struct audio_info_struct *ai)
{
  return 0;
}

int audio_set_channels(struct audio_info_struct *ai)
{
  return 0;
}

int audio_set_format(struct audio_info_struct *ai)
{
  return 0;
}

int audio_get_formats(struct audio_info_struct *ai)
{
  return AUDIO_FORMAT_SIGNED_16;
}

int audio_play_samples(struct audio_info_struct *ai,unsigned char *buf,int len)
{
    return write(ai->fn,buf,len);
}

int audio_close(struct audio_info_struct *ai)
{
  close (ai->fn);
  return 0;
}
