/*
 * Mpeg Layer-1,2,3 audio decoder
 * ------------------------------
 * copyright (c) 1995 by Michael Hipp, All rights reserved. See also 'README'
 * version for slower machines .. decodes only every second sample
 * sounds like 24000,22050 or 16000 kHz .. (depending on original sample freq.)
 *
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mpg123.h"

#define WRITE_SAMPLE(samples,sum,clip) \
  if( (sum) > 32767.0) { *(samples) = 0x7fff; (clip)++; } \
  else if( (sum) < -32768.0) { *(samples) = -0x8000; (clip)++; } \
  else { *(samples) = sum; }

int synth_2to1(real *bandPtr,int channel,short *samples)
{
  static real buffs[2][2][0x100];
  static const int step = 2;
  static int bo = 1;

  real (*buf)[0x100];
  int clip = 0; 

  if(!channel) {
    bo--;
    bo &= 0xf;
    buf = buffs[0];
  }
  else {
    samples++;
    buf = buffs[1];
  }

  {
    int i; /* knowing that band 16 to 31 is zero could simplify the dct64 */
    for(i=16;i<32;i++)
      bandPtr[i] = 0.0;
  }

  dct64(buf[bo&1]+(bo>>1),bandPtr);

  if(bo & 0x1) {
    register int j;
    real *window = decwin + 16 - bo;
    real *b0 = buf[0] + (15*0x8);
    real *b1 = buf[1] + (15*0x8);

    for (j=8;j;j--,b0+=0x8,b1-=0x18,window+=0x30)
    {
      real sum;
      sum  = *window++ * *b0++;
      sum -= *window++ * *b1++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b1++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b1++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b1++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b1++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b1++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b1++;
      sum += *window++ * *b0++;
      sum -= *window++ * *b1++;

      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
    }

    {
      real sum;
      sum  = window[0x0] * *b0++;
      sum += window[0x2] * *b0++;
      sum += window[0x4] * *b0++;
      sum += window[0x6] * *b0++;
      sum += window[0x8] * *b0++;
      sum += window[0xA] * *b0++;
      sum += window[0xC] * *b0++;
      sum += window[0xE] * *b0++;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
      b0-=0x18,b1+=0x10,window-=0x40;
    }
    window += bo<<1;

    for (j=7;j;j--,b0-=0x18,b1+=0x8,window-=0x30)
    {
      real sum;
      sum = -*(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;

      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
    }
  }
  else {
    register int j;
    real *window = decwin + 16 - bo;
    real *b0 = buf[0] + (15*0x8);
    real *b1 = buf[1] + (15*0x8);

    for (j=8;j;j--,b0-=0x18,b1+=0x8,window+=0x30) 
    {
      real sum;
      sum = -*window++ * *b0++;
      sum += *window++ * *b1++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b1++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b1++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b1++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b1++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b1++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b1++;
      sum -= *window++ * *b0++;
      sum += *window++ * *b1++;

      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
    }
    {
      real sum;
      sum  = window[0x1] * *b1++;
      sum += window[0x3] * *b1++;
      sum += window[0x5] * *b1++;
      sum += window[0x7] * *b1++;
      sum += window[0x9] * *b1++;
      sum += window[0xB] * *b1++;
      sum += window[0xD] * *b1++;
      sum += window[0xF] * *b1++;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
      b0+=0x10,b1-=0x18,window-=0x40;
    }
    window += bo<<1;

    for (j=7;j;j--,b1-=0x18,b0+=0x8,window-=0x30)
    {
      real sum;
      sum = -*(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;
      sum -= *(--window) * *b0++;
      sum -= *(--window) * *b1++;

      WRITE_SAMPLE(samples,sum,clip); samples += step;
      WRITE_SAMPLE(samples,sum,clip); samples += step;
    }
  }

  return(clip);
}


