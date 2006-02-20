/* 
 * Mpeg Layer-1,2,3 audio decoder 
 * ------------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp, All rights reserved.
 * See also 'README'
 * slighlty optimized for machines without autoincrement/decrement
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

int synth_1to1(real *bandPtr,int channel,short *samples)
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

  dct64(buf[bo&1]+(bo>>1),bandPtr);

  if(bo & 0x1) {
    real *b0 = buf[0] + (15*0x8);
    real *b1 = buf[1] + (15*0x8);
    register int j;
    real *window = decwin + 16 - bo;

    for (j=16;j;j--,b0+=0x8,b1-=0x8,window+=0x20,samples+=step)
    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum -= window[0x1] * b1[0x0];
      sum += window[0x2] * b0[0x1];
      sum -= window[0x3] * b1[0x1];
      sum += window[0x4] * b0[0x2];
      sum -= window[0x5] * b1[0x2];
      sum += window[0x6] * b0[0x3];
      sum -= window[0x7] * b1[0x3];
      sum += window[0x8] * b0[0x4];
      sum -= window[0x9] * b1[0x4];
      sum += window[0xA] * b0[0x5];
      sum -= window[0xB] * b1[0x5];
      sum += window[0xC] * b0[0x6];
      sum -= window[0xD] * b1[0x6];
      sum += window[0xE] * b0[0x7];
      sum -= window[0xF] * b1[0x7];

      WRITE_SAMPLE(samples,sum,clip);
    }

    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x1];
      sum += window[0x4] * b0[0x2];
      sum += window[0x6] * b0[0x3];
      sum += window[0x8] * b0[0x4];
      sum += window[0xA] * b0[0x5];
      sum += window[0xC] * b0[0x6];
      sum += window[0xE] * b0[0x7];
      WRITE_SAMPLE(samples,sum,clip);
      b0-=0x8,b1+=0x8,window-=0x20,samples+=step;
    }
    window += bo<<1;

    for (j=15;j;j--,b0-=0x8,b1+=0x8,window-=0x20,samples+=step)
    {
      real sum;
      sum = -window[-0x1] * b0[0x0];
      sum -= window[-0x2] * b1[0x0];
      sum -= window[-0x3] * b0[0x1];
      sum -= window[-0x4] * b1[0x1];
      sum -= window[-0x5] * b0[0x2];
      sum -= window[-0x6] * b1[0x2];
      sum -= window[-0x7] * b0[0x3];
      sum -= window[-0x8] * b1[0x3];
      sum -= window[-0x9] * b0[0x4];
      sum -= window[-0xA] * b1[0x4];
      sum -= window[-0xB] * b0[0x5];
      sum -= window[-0xC] * b1[0x5];
      sum -= window[-0xD] * b0[0x6];
      sum -= window[-0xE] * b1[0x6];
      sum -= window[-0xF] * b0[0x7];
      sum -= window[-0x0] * b1[0x7];

      WRITE_SAMPLE(samples,sum,clip);
    }
  }
  else {
    real *b0 = buf[0] + (15*0x8);
    real *b1 = buf[1] + (15*0x8);
    register int j;
    real *window = decwin + 16 - bo;

    for (j=16;j;j--,b0-=0x8,b1+=0x8,window+=0x20,samples+=step)
    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum -= window[0x1] * b1[0x0];
      sum += window[0x2] * b0[0x1];
      sum -= window[0x3] * b1[0x1];
      sum += window[0x4] * b0[0x2];
      sum -= window[0x5] * b1[0x2];
      sum += window[0x6] * b0[0x3];
      sum -= window[0x7] * b1[0x3];
      sum += window[0x8] * b0[0x4];
      sum -= window[0x9] * b1[0x4];
      sum += window[0xA] * b0[0x5];
      sum -= window[0xB] * b1[0x5];
      sum += window[0xC] * b0[0x6];
      sum -= window[0xD] * b1[0x6];
      sum += window[0xE] * b0[0x7];
      sum -= window[0xF] * b1[0x7];

      WRITE_SAMPLE(samples,-sum,clip);
    }

    {
      real sum;
      sum  = window[0x1] * b1[0x0];
      sum += window[0x3] * b1[0x1];
      sum += window[0x5] * b1[0x2];
      sum += window[0x7] * b1[0x3];
      sum += window[0x9] * b1[0x4];
      sum += window[0xB] * b1[0x5];
      sum += window[0xD] * b1[0x6];
      sum += window[0xF] * b1[0x7];
      WRITE_SAMPLE(samples,sum,clip);
      b0+=0x8,b1-=0x8,window-=0x20,samples+=step;
    }
    window += bo<<1;

    for (j=15;j;j--,b0+=0x8,b1-=0x8,window-=0x20,samples+=step)
    {
      real sum;
      sum = -window[-0x1] * b0[0x0];
      sum -= window[-0x2] * b1[0x0];
      sum -= window[-0x3] * b0[0x1];
      sum -= window[-0x4] * b1[0x1];
      sum -= window[-0x5] * b0[0x2];
      sum -= window[-0x6] * b1[0x2];
      sum -= window[-0x7] * b0[0x3];
      sum -= window[-0x8] * b1[0x3];
      sum -= window[-0x9] * b0[0x4];
      sum -= window[-0xA] * b1[0x4];
      sum -= window[-0xB] * b0[0x5];
      sum -= window[-0xC] * b1[0x5];
      sum -= window[-0xD] * b0[0x6];
      sum -= window[-0xE] * b1[0x6];
      sum -= window[-0xF] * b0[0x7];
      sum -= window[-0x0] * b1[0x7];

      WRITE_SAMPLE(samples,sum,clip);
    }
  }
  return clip;
}


