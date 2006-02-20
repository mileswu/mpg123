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

extern int outburst;

void audio_info_struct_init(struct audio_info_struct *ai)
{
#ifdef AUDIO_USES_FD
  ai->fn = -1; 
#endif
#ifdef SGI
#if 0
  ALconfig config;
  ALport port;
#endif
#endif
  ai->rate = -1;
  ai->gain = -1;
  ai->output = -1;
  ai->device = NULL;
  ai->channels = -1;
  ai->format = -1;
}


#ifdef OS2
#include <stdlib.h>
#include <os2.h>
#define  INCL_OS2MM
#include <os2me.h>
#define BUFNUM     20
#define BUFSIZE 16384
#endif

#ifdef NAS
#include <stdlib.h>
#include <audio/audiolib.h>
#include <audio/soundlib.h>

typedef struct
{
    AuServer            *aud;
    AuFlowID            flow;
    AuDeviceAttributes  *da;
    int                 numDevices;
    char                *buf;
    int                 buf_size;
    int                 buf_cnt;
    AuBool              data_sent;
    AuBool              finished;
} InfoRec, *InfoPtr;

#define NAS_SOUND_PORT_DURATION 5 /* seconds */
#define NAS_SOUND_LOW_WATER_MARK 25 /* percent */
#define NAS_MAX_FORMAT 10 /* currently, there are 7 supported formats */

static InfoRec info;

static void
sendData(AuServer *aud, InfoPtr i, AuUint32 numBytes)
{
    if (numBytes < i->buf_cnt) {
        AuWriteElement(aud, i->flow, 0, numBytes, i->buf, AuFalse, NULL);
        memmove(i->buf, i->buf + numBytes, i->buf_cnt - numBytes);
        i->buf_cnt = i->buf_cnt - numBytes;
    }
    else {
         AuWriteElement(aud, i->flow, 0, i->buf_cnt, i->buf,
                        (numBytes > i->buf_cnt), NULL);
         i->buf_cnt = 0;
    }
    i->data_sent = AuTrue;
}

static AuBool
eventHandler(AuServer *aud, AuEvent *ev, AuEventHandlerRec *handler)
{
    InfoPtr         i = (InfoPtr) handler->data;

    switch (ev->type)
    {
        case AuEventTypeMonitorNotify:
            i->finished = AuTrue;
            break;
       case AuEventTypeElementNotify:
           {
               AuElementNotifyEvent *event = (AuElementNotifyEvent *) ev;

               switch (event->kind)
               {
                   case AuElementNotifyKindLowWater:
                       sendData(aud, i, event->num_bytes);
                       break;
                   case AuElementNotifyKindState:
                       switch (event->cur_state)
                       {
                           case AuStatePause:
                               if (event->reason != AuReasonUser)
                                   sendData(aud, i, event->num_bytes);
                               break;
                            case AuStateStop:
                                i->finished = AuTrue;
                                break;
                       }
               }
           }
    }
    return AuTrue;
}

int audio_open(struct audio_info_struct *ai)
{
    AuDeviceID      device = AuNone;
    AuElement       elements[3];
    unsigned char   format;
    int             buf_samples;
    int i, j, k;
    int maxRate, minRate;
    AuBool supportedFormats[NAS_MAX_FORMAT];
    
    if(!ai)
        return -1;

    if (!(info.aud = AuOpenServer(ai->device, 0, NULL, 0, NULL, NULL))) {
        if (ai->device==NULL)
            fprintf(stderr,"could not open default NAS server\n");
        else
            fprintf(stderr,"could not open NAS server %s\n",
                    ai->device);
        exit(1);
    }

    /* get server info */
    maxRate =  AuServerMaxSampleRate(info.aud);
    minRate =  AuServerMinSampleRate(info.aud);
    for (i=0; i<NAS_MAX_FORMAT; i++)
        supportedFormats[i]=AuFalse;
    
    j = AuServerNumFormats(info.aud);
    for (i=0; i<j; i++) {
        k=AuServerFormat(info.aud,i);
        if ((k>=0) && (k<NAS_MAX_FORMAT))
            supportedFormats[k] = AuTrue;
    }
    
    /* adjust sample rate */
    if (ai->rate == -1) ai->rate = maxRate;
    if (ai->rate > maxRate) {
        fprintf(stderr,"unable to set audio rate to %ld\n", ai->rate);
        exit(1);
    }
    if (ai->rate < minRate) {
        fprintf(stderr,"unable to set audio rate to %ld\n", ai->rate);
        exit(1);
    }
    
    buf_samples = ai->rate * NAS_SOUND_PORT_DURATION;
    
    format = AuFormatLinearSigned16LSB;
    if (!supportedFormats[AuFormatLinearSigned16LSB]) {
        fprintf(stderr,"LinearUnsigned16LSB not supported\n");
        exit(1);
    }

    /* look for an output device */
    for (i = 0; i < AuServerNumDevices(info.aud); i++)
       if (((AuDeviceKind(AuServerDevice(info.aud, i)) ==
              AuComponentKindPhysicalOutput) &&
             AuDeviceNumTracks(AuServerDevice(info.aud, i))
             ==  ai->channels )) {
            device = AuDeviceIdentifier(AuServerDevice(info.aud, i));
            break;
       }
    if (device == AuNone) {
       fprintf(stderr,
                "Couldn't find an output device providing %d channels\n",
                ai->channels);
        exit(1);
    }

    if(ai->gain >= 0) {
        info.da = AuGetDeviceAttributes(info.aud, device, NULL);
        if ((info.da)!=NULL) {
            AuDeviceGain(info.da) = AuFixedPointFromSum(ai->gain, 0);
            AuSetDeviceAttributes(info.aud, AuDeviceIdentifier(info.da),
                                  AuCompDeviceGainMask, info.da, NULL);
        }
        else
            fprintf(stderr,"audio/gain: setable Volume/PCM-Level not supported");
    }
        
    if (!(info.flow = AuCreateFlow(info.aud, NULL))) {
        fprintf(stderr, "Couldn't create flow\n");
        exit(1);
    }

    AuMakeElementImportClient(&elements[0],        /* element */
                              (unsigned short) ai->rate,
                                                   /* rate */
                              format,              /* format */
                             ai->channels,        /* channels */
                             AuTrue,              /* ??? */
                              (unsigned short) buf_samples,
                                                   /* max samples */
                              (unsigned short) (ai->rate *
                                  NAS_SOUND_LOW_WATER_MARK / 100),
                                                   /* low water mark */
                              0,                   /* num actions */
                              NULL);               /* actions */
    AuMakeElementExportDevice(&elements[1],        /* element */
                              0,                   /* input */
                              device,              /* device */
                              (unsigned short) ai->rate,
                                                   /* rate */
                             AuUnlimitedSamples,  /* num samples */
                              0,                   /* num actions */
                              NULL);               /* actions */
    AuSetElements(info.aud,                        /* Au server */
                  info.flow,                       /* flow ID */
                  AuTrue,                          /* clocked */
                  2,                               /* num elements */
                  elements,                        /* elements */
                  NULL);                           /* return status */

    AuRegisterEventHandler(info.aud,               /* Au server */
                           AuEventHandlerIDMask,   /* value mask */
                           0,                      /* type */
                           info.flow,              /* id */
                          eventHandler,           /* callback */
                           (AuPointer) &info);     /* data */

    info.buf_size = buf_samples * ai->channels * AuSizeofFormat(format);
    info.buf = (char *) malloc(info.buf_size);
    if (info.buf == NULL) {
        fprintf(stderr, "Unable to allocate input/output buffer of size %d\n",
             info.buf_size);
        exit(1);
    }
    info.buf_cnt = 0;
    info.data_sent = AuFalse;
    info.finished = AuFalse;
    
    AuStartFlow(info.aud,                          /* Au server */
                info.flow,                         /* id */
                NULL);                             /* status */

    return 0;
}
    
int audio_set_rate(struct audio_info_struct *ai)
{
    /* changing the sample rate is not supported */
    return 0;
}

int audio_set_channels(struct audio_info_struct *ai)
{
    /* changing the channel number is not supported */
    return 0;
}

void nas_flush()
{
    AuEvent         ev;
    
    while ((!info.data_sent) && (!info.finished)) {
        AuNextEvent(info.aud, AuTrue, &ev);
        AuDispatchEvent(info.aud, &ev);
    }
    info.data_sent = AuFalse;
}

int audio_play_samples(struct audio_info_struct *ai,short *buf,int len)
{
    int buf_cnt = 0;
    int len_bytes = len * 2;

    while ((info.buf_cnt + (len_bytes - buf_cnt)) >  info.buf_size) {
        memcpy(info.buf + info.buf_cnt,
               ((char *) buf) + buf_cnt,
               (info.buf_size - info.buf_cnt));
        buf_cnt += (info.buf_size - info.buf_cnt);
        info.buf_cnt += (info.buf_size - info.buf_cnt);
        nas_flush();
    }
    memcpy(info.buf + info.buf_cnt,
           ((char *) buf) + buf_cnt,
           (len_bytes - buf_cnt));
    info.buf_cnt += (len_bytes - buf_cnt);
    
    return len*2;
}

int audio_close(struct audio_info_struct *ai)
{
    while (!info.finished) {
        nas_flush();
    }
    AuCloseServer(info.aud);
    free(info.buf);
    
    return 0;
}

#elif defined(VOXWARE)
#include <sys/ioctl.h>
#ifdef LINUX
#include <linux/soundcard.h>
#else
#include <machine/soundcard.h>
#endif

int audio_open(struct audio_info_struct *ai)
{
  if(!ai)
    return -1;

  if(!ai->device)
    ai->device = "/dev/dsp";

  ai->fn = open(ai->device,O_WRONLY);  

  if(ai->fn < 0)
  {
    fprintf(stderr,"Can't open %s!\n",ai->device);
    exit(1);
  }

  ioctl(ai->fn, SNDCTL_DSP_GETBLKSIZE, &outburst);
  if(outburst > MAXOUTBURST)
    outburst = MAXOUTBURST;

  if(audio_reset_parameters(ai) < 0) {
    close(ai->fn);
    return -1;
  }

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

int audio_reset_parameters(struct audio_info_struct *ai)
{
  int ret;
  ret = ioctl(ai->fn,SNDCTL_DSP_RESET,NULL);
  if(ret >= 0)
    ret = audio_set_format(ai);
  if(ret >= 0)
    ret = audio_set_channels(ai);
  if(ret >= 0)
    ret = audio_set_rate(ai);
  return ret;
}

int audio_rate_best_match(struct audio_info_struct *ai)
{
  int ret,dsp_rate;

  if(!ai || ai->fn < 0 || ai->rate < 0)
    return -1;
  dsp_rate = ai->rate;
  ret = ioctl(ai->fn, SNDCTL_DSP_SPEED,&dsp_rate);
  if(ret < 0)
    return ret;
  ai->rate = dsp_rate;
  return 0;
}

int audio_set_rate(struct audio_info_struct *ai)
{
  int dsp_rate;

  if(ai->rate >= 0) {
    dsp_rate = ai->rate;
    return ioctl(ai->fn, SNDCTL_DSP_SPEED,&dsp_rate);
  }

  return 0;
}

int audio_set_channels(struct audio_info_struct *ai)
{
  int chan = ai->channels - 1;

  if(ai->channels < 0)
    return 0;

  return ioctl(ai->fn, SNDCTL_DSP_STEREO, &chan);
}

int audio_set_format(struct audio_info_struct *ai)
{
  int sample_size,fmts;

  if(ai->format == -1)
    return 0;

  switch(ai->format) {
    case AUDIO_FORMAT_SIGNED_16:
    default:
      fmts = AFMT_S16_LE;
      sample_size = 16;
      break;
    case AUDIO_FORMAT_UNSIGNED_8:
      fmts = AFMT_U8;
      sample_size = 8;
      break;
    case AUDIO_FORMAT_SIGNED_8:
      fmts = AFMT_S8;
      sample_size = 8;
      break;
    case AUDIO_FORMAT_ULAW_8:
      fmts = AFMT_MU_LAW;
      sample_size = 8;
      break;
  }
  if(ioctl(ai->fn, SNDCTL_DSP_SAMPLESIZE,&sample_size) < 0)
    return -1;
  return ioctl(ai->fn, SNDCTL_DSP_SETFMT, &fmts);
}

int audio_get_formats(struct audio_info_struct *ai)
{
  int ret = 0;
  int fmts;

  if(ioctl(ai->fn,SNDCTL_DSP_GETFMTS,&fmts) < 0)
    return -1;

  if(fmts & AFMT_MU_LAW)
    ret |= AUDIO_FORMAT_ULAW_8;
  if(fmts & AFMT_S16_LE)
    ret |= AUDIO_FORMAT_SIGNED_16;
  if(fmts & AFMT_U8)
    ret |= AUDIO_FORMAT_UNSIGNED_8;
  if(fmts & AFMT_S8)
    ret |= AUDIO_FORMAT_SIGNED_8;

  return ret;
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

  if(audio_reset_parameters(ai) < 0) {
    return -1;
  }

  if(ioctl(ai->fn, AUDIO_GETINFO, &ainfo) == -1)
    return -1;

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

int audio_reset_parameters(struct audio_info_struct *ai)
{
  int ret;
  ret = audio_set_format(ai);
  if(ret >= 0)
    ret = audio_set_channels(ai);
  if(ret >= 0)
    ret = audio_set_rate(ai);
  return ret;
}

int audio_rate_best_match(struct audio_info_struct *ai)
{
  return 0;
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

int audio_set_format(struct audio_info_struct *ai)
{
  audio_info_t ainfo;

  if(ioctl(ai->fn, AUDIO_GETINFO, &ainfo) == -1)
    return -1;

  switch(ai->format) {
    case -1:
    case AUDIO_FORMAT_SIGNED_16:
    default:
      ainfo.play.encoding = AUDIO_ENCODING_LINEAR;
      ainfo.play.precision = 16;
      break;
    case AUDIO_FORMAT_UNSIGNED_8:
#ifndef SUNOS
      ainfo.play.encoding = AUDIO_ENCODING_LINEAR8;
      ainfo.play.precision = 8;
      break;
#endif
    case AUDIO_FORMAT_SIGNED_8:
      fprintf(stderr,"Linear signed 8 bit not supported!\n");
      return -1;
    case AUDIO_FORMAT_ULAW_8:
      ainfo.play.encoding = AUDIO_ENCODING_ULAW;
      ainfo.play.precision = 8;
      break;
    case AUDIO_FORMAT_ALAW_8:
      ainfo.play.encoding = AUDIO_ENCODING_ALAW;
      ainfo.play.precision = 8;
      break;
  }

  if(ioctl(ai->fn, AUDIO_SETINFO, &ainfo) == -1)
    return -1;

  return 0;
}

int audio_get_formats(struct audio_info_struct *ai)
{
  return AUDIO_FORMAT_SIGNED_16|AUDIO_FORMAT_ULAW_8;
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

  if(audio_reset_parameters(ai) < 0)
    return -1;
 
  return ai->fn;
}

int audio_reset_parameters(struct audio_info_struct *ai)
{
  int ret;
  ret = audio_set_format(ai);
  if(ret >= 0)
    ret = audio_set_channels(ai);
  if(ret >= 0)
    ret = audio_set_rate(ai);
  return ret;
}

int audio_rate_best_match(struct audio_info_struct *ai)
{
  return 0;
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

int audio_set_format(struct audio_info_struct *ai)
{
  int fmt;

  switch(ai->format) {
    case -1:
    case AUDIO_FORMAT_SIGNED_16:
    default: 
      fmt = AUDIO_FORMAT_LINEAR16BIT;
      break;
    case AUDIO_FORMAT_UNSIGNED_8:
      fprintf(stderr,"unsigned 8 bit linear not supported\n");
      return -1;
    case AUDIO_FORMAT_SIGNED_8:
      fprintf(stderr,"signed 8 bit linear not supported\n");
      return -1;
    case AUDIO_FORMAT_ALAW_8:
      fmt = AUDIO_FORMAT_ALAW;
      break;
    case AUDIO_FORMAT_ULAW_8:
      fmt = AUDIO_FORMAT_ULAW;
      break;
  }
  return ioctl(ai->fn,AUDIO_SET_DATA_FORMAT,fmt);
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

  audio_reset_parameters(ai);
    
  return 1;
}

int audio_reset_parameters(struct audio_info_struct *ai)
{
  int ret;
  ret = audio_set_format(ai);
  if(ret >= 0)
    ret = audio_set_channels(ai);
  if(ret >= 0)
    ret = audio_set_rate(ai);

/* todo: Set new parameters here */

  return ret;
}

int audio_rate_best_match(struct audio_info_struct *ai)
{
  return 0;
}

int audio_set_rate(struct audio_info_struct *ai)
{
    long params[2] = {AL_OUTPUT_RATE, 44100};

    params[1] = ai->rate;
    ALsetparams(AL_DEFAULT_DEVICE, params, 2);
    return 0;
}

int audio_set_channels(struct audio_info_struct *ai)
{
    if(ai->channels == 2)
      ALsetchannels(ai->config, AL_STEREO);
    else
      ALsetchannels(ai->config, AL_MONO);
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
  if(ai->format == AUDIO_FORMAT_SIGNED_8)
    ALwritesamps(ai->port, buf, len);
  else
    ALwritesamps(ai->port, buf, len>>1);
  return len;
}

int audio_close(struct audio_info_struct *ai)
{
    while(ALgetfilled(ai->port) > 0)
	sginap(1);  
    ALcloseport(ai->port);
    ALfreeconfig(ai->config);
    return 0;
}

#elif defined(OS2)

typedef struct {
    ULONG operation;
    ULONG operand1;
    ULONG operand2;
    ULONG operand3;
} ple;
MCI_WAVE_SET_PARMS msp;
MCI_PLAY_PARMS mpp;
int id, pos=0;
ple pl[BUFNUM+2];

int audio_open(struct audio_info_struct *ai)
{
  char *buf[BUFNUM];
  int i;
  ULONG rc;
  MCI_OPEN_PARMS mop;

  pl[0].operation = (ULONG)NOP_OPERATION;
  pl[0].operand1 = 0;
  pl[0].operand2 = 0;
  pl[0].operand3 = 0;
  for(i = 0; i < BUFNUM; i++) {
    buf[i] = (char*)malloc(BUFSIZE);
    memset(buf[i], 0, BUFSIZE);
    pl[i+1].operation = (ULONG)DATA_OPERATION;
    pl[i+1].operand1 = (ULONG)buf[i];
    pl[i+1].operand2 = BUFSIZE/2;
    pl[i+1].operand3 = 0;
  }
  pl[BUFNUM+1].operation = (ULONG)BRANCH_OPERATION;
  pl[BUFNUM+1].operand1 = 0;
  pl[BUFNUM+1].operand2 = 1;
  pl[BUFNUM+1].operand3 = 0;

  mop.pszDeviceType = (PSZ)MCI_DEVTYPE_WAVEFORM_AUDIO_NAME;
  mop.pszElementName = (PSZ)&pl;

  rc = mciSendCommand(0, MCI_OPEN, MCI_WAIT
                                 | MCI_OPEN_PLAYLIST, (PVOID)&mop, 0);
  if (rc) {
    puts("open audio device failed!");
    exit(1);
  }
  id = mop.usDeviceID;

  msp.usBitsPerSample = 16;
  rc = mciSendCommand(id, MCI_SET, MCI_WAIT | MCI_WAVE_SET_BITSPERSAMPLE, (PVOID)&msp, 0);
  return 0;
}

int audio_reset_parameters(struct audio_info_struct *ai)
{
  static int audio_initialized = FALSE, i;

  if (audio_initialized) {
    while (pl[pos].operand3 < pl[pos].operand2) _sleep2(125);
    for(i = 1; i <= BUFNUM; i++) {
      memset((char*)pl[i].operand1, 0, BUFSIZE);
      pl[i].operand2 = BUFSIZE;
      pl[i].operand3 = 0;
    }
    _sleep2(2000);
    mciSendCommand(id, MCI_STOP, MCI_WAIT, (PVOID)&mpp, 0);
    for(i = 1; i <= BUFNUM; i++) {
      memset((char*)pl[i].operand1, 0, BUFSIZE);
      pl[i].operand2 = BUFSIZE/2;
      pl[i].operand3 = 0;
    }
    pos = 0;
  }
  msp.ulSamplesPerSec = ai->rate;
  msp.usChannels = ai->channels;
  mciSendCommand(id, MCI_SET, MCI_WAIT 
                            | MCI_WAVE_SET_SAMPLESPERSEC 
                            | MCI_WAVE_SET_CHANNELS, (PVOID)&msp, 0);

  mciSendCommand(id, MCI_PLAY, 0, (PVOID)&mpp, 0);

  if(!audio_initialized) audio_initialized = TRUE;
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

int audio_play_samples(struct audio_info_struct *ai,unsigned char *buf,int len)
{
  pos = pos + 1;

  if (pos > BUFNUM) pos = 1;

  while (pl[pos].operand3 < pl[pos].operand2) _sleep2(125);

  memcpy((char*)pl[pos].operand1, buf, len);
  pl[pos].operand2 = len;
  pl[pos].operand3 = 0;

  return len;
}

int audio_close(struct audio_info_struct *ai)
{
  ULONG rc;

  pl[pos].operation = (ULONG)EXIT_OPERATION;
  pl[pos].operand2 = 0;
  pl[pos].operand3 = 0;

  pos = pos - 1;
  if(pos == 0) pos = BUFNUM;

  while (pl[pos].operand3 < pl[pos].operand2) _sleep2(250);
  _sleep2(2000);

  rc = mciSendCommand(id, MCI_CLOSE, MCI_WAIT, (PVOID)NULL, 0);
  return 0;
}

#else

int audio_open(struct audio_info_struct *ai)
{
  fprintf(stderr,"No audio device support compiled into this binary (use -s).\n");
  return -1;
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
  return len;
}

int audio_close(struct audio_info_struct *ai)
{
  return 0;
}
#endif
