/* 
 *  simple audio Lib .. 
 *  copyrights (c) 1994,1995,1996 by Michael Hipp
 *  SGI audio support copyrights (c) 1995 by Thomas Woerner
 *  copy policy: GPL V2.0
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef SOLARIS
#include <stropts.h>
#include <sys/conf.h>
#endif

#include "mpg123.h"

#ifdef VOXWARE
#include <sys/ioctl.h>
#ifdef LINUX
#include <linux/soundcard.h>
#else
#include <machine/soundcard.h>
#endif

int audio_open(struct audio_info_struct *ai)
{
  int fmts = AFMT_S16_LE;
  int dsp_samplesize = 16;
  int dsp_stereo;
  int dsp_speed;

  if(!ai)
    return -1;

  dsp_stereo = ai->channels - 1;

  if(ai->rate == -1)
    dsp_speed = 44100;
  else
    dsp_speed = ai->rate;

  if(!ai->device)
    ai->device = "/dev/dsp";

  ai->fn = open(ai->device,O_WRONLY);  

  if(ai->fn < 0)
  {
    fprintf(stderr,"Can't open %s!\n",ai->device);
    exit(1);
  }

  ioctl(ai->fn, SNDCTL_DSP_SAMPLESIZE,&dsp_samplesize);
  ioctl(ai->fn, SNDCTL_DSP_STEREO, &dsp_stereo);
  ioctl(ai->fn, SNDCTL_DSP_SPEED, &dsp_speed);
  ioctl(ai->fn, SNDCTL_DSP_SETFMT, &fmts);

  if(ai->gain >= 0) {
    int e,mask;
    e = ioctl(ai->fn , SOUND_MIXER_READ_DEVMASK ,&mask);
    if(e < 0) {
      fprintf(stderr,"audio/gain: Can't get audio device features list.\n");
    }
    else if(mask & SOUND_MASK_PCM) {
      int gain = (ai->gain<<8)|(ai->gain);
      e = ioctl(ai->fn, SOUND_MIXER_WRITE_PCM , &gain);
    }
    else if(!(mask & SOUND_MASK_VOLUME)) {
      fprintf(stderr,"audio/gain: setable Volume/PCM-Level not supported by your audio device: %#04x\n",mask);
    }
    else { 
      int gain = (ai->gain<<8)|(ai->gain);
      e = ioctl(ai->fn, SOUND_MIXER_WRITE_VOLUME , &gain);
    }
  }

  return ai->fn;
}

int audio_set_rate(struct audio_info_struct *ai)
{
  int dsp_samplesize;

  if(ai->rate >= 0)
  {
    dsp_samplesize = ai->rate;
    ioctl(ai->fn, SNDCTL_DSP_SPEED,&dsp_samplesize);
  }

  return 0;
}

int audio_set_channels(struct audio_info_struct *ai)
{
  int chan = ai->channels - 1;
  return ioctl(ai->fn, SNDCTL_DSP_STEREO, &chan);
}

int audio_play_samples(struct audio_info_struct *ai,short *buf,int len)
{
  return write(ai->fn,buf,len*2);
}

int audio_close(struct audio_info_struct *ai)
{
  close (ai->fn);
  return 0;
}

#elif defined(SOLARIS) || defined(SUNOS)

#include <sys/filio.h>
#ifdef SUNOS
#include <sun/audioio.h>
#else
#include <sys/audioio.h>
#endif

int audio_open(struct audio_info_struct *ai)
{
  audio_info_t ainfo;

  ai->fn = open("/dev/audio",O_WRONLY);
  if(ai->fn < 0)
     return ai->fn;

#ifdef SUNOS
  {
    int type;
    if(ioctl(ai->fn, AUDIO_GETDEV, &type) == -1)
      return -1;
    if(type == AUDIO_DEV_UNKNOWN || type == AUDIO_DEV_AMD)
      return -1;
  }
#else
#if 0
  {
    struct audio_device ad;
    if(ioctl(ai->fn, AUDIO_GETDEV, &ad) == -1)
      return -1;
fprintf(stderr,"%s\n",ad.name);
    if(strstr(ad.name,"CS4231"))
      fprintf(stderr,"Warning: Your machine can't play full 44.1Khz stereo.\n");
    if(!strstr(ad.name,"dbri") && !strstr(ad.name,"CS4231"))
      fprintf(stderr,"Warning: Unknown sound system %s. But we try it.\n",ad.name);
  }
#endif
#endif
  if(ioctl(ai->fn, AUDIO_GETINFO, &ainfo) == -1)
    return -1;

  if(ai->rate != -1)
    ainfo.play.sample_rate = ai->rate;
  ainfo.play.channels = ai->channels;
  ainfo.play.encoding = AUDIO_ENCODING_LINEAR;
  ainfo.play.precision = 16;

  switch(ai->output)
  {
    case AUDIO_OUT_INTERNAL_SPEAKER:
      ainfo.play.port = AUDIO_SPEAKER;
      break;
    case AUDIO_OUT_HEADPHONES:
      ainfo.play.port = AUDIO_HEADPHONE;
      break;
    case AUDIO_OUT_LINE_OUT:
      ainfo.play.port = AUDIO_LINE_OUT;
      break;
  }

  if(ai->gain != -1)
    ainfo.play.gain = ai->gain;

  if(ioctl(ai->fn, AUDIO_SETINFO, &ainfo) == -1)
    return -1;

  return ai->fn;
}

int audio_set_rate(struct audio_info_struct *ai)
{
  audio_info_t ainfo;

  if(ai->rate != -1)
  {
    if(ioctl(ai->fn, AUDIO_GETINFO, &ainfo) == -1)
      return -1;
    ainfo.play.sample_rate = ai->rate;
    if(ioctl(ai->fn, AUDIO_SETINFO, &ainfo) == -1)
      return -1;
    return 0;
  }
  return -1;
}

int audio_set_channels(struct audio_info_struct *ai)
{
  audio_info_t ainfo;

  if(ioctl(ai->fn, AUDIO_GETINFO, &ainfo) == -1)
    return -1;
  ainfo.play.channels = ai->channels;
  if(ioctl(ai->fn, AUDIO_SETINFO, &ainfo) == -1)
    return -1;
  return 0;
}

int audio_play_samples(struct audio_info_struct *ai,short *buf,int len)
{
  return write(ai->fn,buf,len*2);
}

int audio_close(struct audio_info_struct *ai)
{
  close (ai->fn);
  return 0;
}

#ifdef SOLARIS
void audio_queueflush (struct audio_info_struct *ai)
{
	ioctl (ai->fn, I_FLUSH, FLUSHRW);
}
#endif


#elif defined(HPUX)
#include <sys/audio.h>

int audio_open(struct audio_info_struct *ai)
{
  struct audio_describe ades;
  struct audio_gain again;
  int i,audio;

  ai->fn = open("/dev/audio",O_RDWR);

  if(ai->fn < 0)
    return -1;

  ioctl(ai->fn,AUDIO_SET_DATA_FORMAT,AUDIO_FORMAT_LINEAR16BIT);
  ioctl(ai->fn,AUDIO_SET_CHANNELS,ai->channels);

  ioctl(ai->fn,AUDIO_DESCRIBE,&ades);

  if(ai->gain != -1)
  {
     if(ai->gain > ades.max_transmit_gain)
     {
       fprintf(stderr,"your gainvalue was to high -> set to maximum.\n");
       ai->gain = ades.max_transmit_gain;
     }
     if(ai->gain < ades.min_transmit_gain)
     {
       fprintf(stderr,"your gainvalue was to low -> set to minimum.\n");
       ai->gain = ades.min_transmit_gain;
     }
     again.channel_mask = AUDIO_CHANNEL_0 | AUDIO_CHANNEL_1;
     ioctl(ai->fn,AUDIO_GET_GAINS,&again);
     again.cgain[0].transmit_gain = ai->gain;
     again.cgain[1].transmit_gain = ai->gain;
     again.channel_mask = AUDIO_CHANNEL_0 | AUDIO_CHANNEL_1;
     ioctl(ai->fn,AUDIO_SET_GAINS,&again);
  }
  
  if(ai->output != -1)
  {
     if(ai->output == AUDIO_OUT_INTERNAL_SPEAKER)
       ioctl(ai->fn,AUDIO_SET_OUTPUT,AUDIO_OUT_SPEAKER);
     else if(ai->output == AUDIO_OUT_HEADPHONES)
       ioctl(ai->fn,AUDIO_SET_OUTPUT,AUDIO_OUT_HEADPHONE);
     else if(ai->output == AUDIO_OUT_LINE_OUT)
       ioctl(ai->fn,AUDIO_SET_OUTPUT,AUDIO_OUT_LINE);
  }
  
  if(ai->rate == -1)
    ai->rate = 44100;

  for(i=0;i<ades.nrates;i++)
  {
    if(ai->rate == ades.sample_rate[i])
      break;
  }
  if(i == ades.nrates)
  {
    fprintf(stderr,"Can't set sample-rate to %ld.\n",ai->rate);
    i = 0;
  }
  ioctl(ai->fn,AUDIO_SET_SAMPLE_RATE,ades.sample_rate[i]);
 
  return ai->fn;
}

int audio_set_rate(struct audio_info_struct *ai)
{
  if(ai->rate >= 0)
    return ioctl(ai->fn,AUDIO_SET_SAMPLE_RATE,ai->rate);
  return 0;
}

int audio_set_channels(struct audio_info_struct *ai)
{
  return ioctl(ai->fn,AUDIO_SET_CHANNELS,ai->channels);
}

int audio_play_samples(struct audio_info_struct *ai,short *buf,int len)
{
  return write(ai->fn,buf,len*2);
}

int audio_close(struct audio_info_struct *ai)
{
  close (ai->fn);
  return 0;
}

#elif defined(SGI)

int audio_open(struct audio_info_struct *ai)
{
    ai->config = ALnewconfig();

    if(ai->channels == 2)
      ALsetchannels(ai->config, AL_STEREO);
    else
      ALsetchannels(ai->config, AL_MONO);
    ALsetwidth(ai->config, AL_SAMPLE_16);
    ALsetqueuesize(ai->config, 131069);
    
    ai->port = ALopenport("mpg132", "w", ai->config);
    if(ai->port == NULL){
	fprintf(stderr, "Unable to open audio channel.");
	exit(-1);
    }
    
    return 1;
}

int audio_set_rate(struct audio_info_struct *ai)
{
    long params[2] = {AL_OUTPUT_RATE, 44100};

    params[1] = ai->rate;
    ALsetparams(AL_DEFAULT_DEVICE, params, 2);    
}

int audio_set_channels(struct audio_info_struct *ai)
{
    if(ai->channels == 2)
      ALsetchannels(ai->config, AL_STEREO);
    else
      ALsetchannels(ai->config, AL_MONO);
    return 0;
}

int audio_play_samples(struct audio_info_struct *ai,short *buf,int len)
{
    return ALwritesamps(ai->port, buf, len)*2;
}

int audio_close(struct audio_info_struct *ai)
{
    while(ALgetfilled(ai->port) > 0)
	sginap(1);  
    ALcloseport(ai->port);
    ALfreeconfig(ai->config);
}

#else
int audio_open(struct audio_info_struct *ai)
{
  fprintf(stderr,"No audio device support compiled into this binary (use -s).\n");
  return -1;
}

int audio_set_rate(struct audio_info_struct *ai)
{
  return 0;
}

int audio_set_channels(struct audio_info_struct *ai)
{
  return 0;
}

int audio_play_samples(struct audio_info_struct *ai,short *buf,int len)
{
  return len*2;
}

int audio_close(struct audio_info_struct *ai)
{
  return 0;
}
#endif


