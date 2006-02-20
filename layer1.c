/* 
 * Mpeg Layer-1 audio decoder 
 * --------------------------
 * copyright (c) 1995 by Michael Hipp, All rights reserved. See also 'README'
 * near unoptimzed ...
 *
 * may have a few bugs after last optimization ... 
 *
 */

#include "mpg123.h"

extern int  SubBandSynthesis(real *,int,short *);
extern real muls[27][64];

void I_step_one(unsigned int balloc[], unsigned int scale_index[2][SBLIMIT],struct frame *fr)
{
  unsigned int *ba=balloc;
  unsigned int *sca = (unsigned int *) scale_index;

  if(fr->stereo) {
    int i;
    int jsbound = fr->jsbound;
    for (i=0;i<jsbound;i++) { 
      *ba++ = getbits(4);
      *ba++ = getbits(4);
    }
    for (i=jsbound;i<SBLIMIT;i++)
      *ba++ = getbits(4);

    ba = balloc;

    for (i=0;i<jsbound;i++) {
      if ((*ba++))
        *sca++ = getbits(6);
      if ((*ba++))
        *sca++ = getbits(6);
    }
    for (i=jsbound;i<SBLIMIT;i++)
      if ((*ba++)) {
        *sca++ =  getbits(6);
        *sca++ =  getbits(6);
      }
  }
  else {
    int i;
    for (i=0;i<SBLIMIT;i++)
      *ba++ = getbits(4);
    ba = balloc;
    for (i=0;i<SBLIMIT;i++)
      if ((*ba++))
        *sca++ = getbits(6);
  }
}

void I_step_two(real fraction[2][SBLIMIT],unsigned int balloc[2*SBLIMIT],
	unsigned int scale_index[2][SBLIMIT],struct frame *fr)
{
  int i,n;
  int smpb[2*SBLIMIT]; /* values: 0-65535 */
  int *sample;
  register unsigned int *ba;
  register unsigned int *sca = (unsigned int *) scale_index;

  if(fr->stereo) {
    int jsbound = fr->jsbound;
    register real *f0 = fraction[0];
    register real *f1 = fraction[1];
    ba = balloc;
    for (sample=smpb,i=0;i<jsbound;i++)  {
      if ((n = *ba++))
        *sample++ = getbits(n+1);
      if ((n = *ba++))
        *sample++ = getbits(n+1);
    }
    for (i=jsbound;i<SBLIMIT;i++) 
      if ((n = *ba++))
        *sample++ = getbits(n+1);

    ba = balloc;
    for (sample=smpb,i=0;i<jsbound;i++) {
      if((n=*ba++))
        *f0++ = (real) ( ((-1)<<n) + (*sample++) + 1) * muls[n+1][*sca++];
      else
        *f0++ = 0.0;
      if((n=*ba++))
        *f1++ = (real) ( ((-1)<<n) + (*sample++) + 1) * muls[n+1][*sca++];
      else
        *f1++ = 0.0;
    }
    for (sample=smpb,i=jsbound;i<SBLIMIT;i++) {
      if ((n=*ba++)) {
        real samp = ( ((-1)<<n) + (*sample++) + 1);
        *f0++ = samp * muls[n+1][*sca++];
        *f1++ = samp * muls[n+1][*sca++];
      }
      else
        *f0++ = *f1++ = 0.0;
    }
  }
  else {
    register real *f0 = fraction[0];
    ba = balloc;
    for (sample=smpb,i=0;i<SBLIMIT;i++)
      if ((n = *ba++))
        *sample++ = getbits(n+1);
    ba = balloc;
    for (sample=smpb,i=0;i<SBLIMIT;i++) {
      if((n=*ba++))
        *f0++ = (real) ( ((-1)<<n) + (*sample++) + 1) * muls[n+1][*sca++];
      else
        *f0++ = 0.0;
    }
  }
}

int do_layer1(struct frame *fr,int outmode,struct audio_info_struct *ai)
{
  int clip=0;
  int i,stereo = fr->stereo;
  unsigned int balloc[2*SBLIMIT];
  unsigned int scale_index[2][SBLIMIT];
  real fraction[2][SBLIMIT];
  int single = fr->single;

  if(stereo == 1 || single == 3)
    single = 0;

  I_step_one(balloc,scale_index,fr);

  for (i=0;i<SCALE_BLOCK;i++)
  {
    I_step_two(fraction,balloc,scale_index,fr);

    if(single >= 0)
    {
      int k;
      clip += SubBandSynthesis ( (real *) fraction[single],0,pcm_sample+pcm_point);
      for(k=0;k<64;k+=2)
        pcm_sample[pcm_point+k+1] = pcm_sample[pcm_point+k];
    }
    else {
        clip += SubBandSynthesis ( (real *) fraction[0],0,pcm_sample+pcm_point);
        clip += SubBandSynthesis ( (real *) fraction[1],1,pcm_sample+pcm_point);
    }

    pcm_point+=64;
    if(pcm_point == audiobufsize)
    {
      switch(outmode)
      {
	case DECODE_STDOUT:
          write(1,pcm_sample,audiobufsize*2);
          break;
        case DECODE_AUDIO:
          audio_play_samples(ai,pcm_sample,audiobufsize);
          break;
      }
      pcm_point = 0;
    }
  }

  return clip;
}


