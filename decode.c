/* 
 * Mpeg Layer-1,2,3 audio decoder 
 * ------------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp, All rights reserved.
 * See also 'README'
 *
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mpg123.h"

static void tr(real *here,real *samples);

#define WRITE_SAMPLE(samples,sum,clip) \
  if( (sum) > 32767.0) { *(samples) = 0x7fff; (clip)++; } \
  else if( (sum) < -32768.0) { *(samples) = 0x8000; (clip)++; } \
  else { *(samples) = sum; }

int synth_1to1(real *bandPtr,int channel,short *samples)
{
  static real buf1[0x200],buf0[0x200];
  static int boc[2]={1,1};
  static real *buffs[2] = { buf0,buf1 };
  static const int step = 2;

  int bo;
  int clip = 0; 
  real *buf = buffs[channel];

  samples += channel;

  bo = boc[channel];
  bo--;
  bo &= 0xf;
  boc[channel] = bo;

  tr(buf+(bo<<5),bandPtr); /* writes values from buf[0] to buf[3f] */

  if(bo & 0x1) {
    register int j;
    register real *window,*b0,*b1,sum;
    window = decwin + 16 - bo;

    b0 = buf + 15;
    b1 = buf + 15;
    for (j=16;j;j--,b0++,b1--,window+=16,samples+=step)
    {
      sum  = *window++ * b0[0x000];
      sum -= *window++ * b1[0x020];
      sum += *window++ * b0[0x040];
      sum -= *window++ * b1[0x060];
      sum += *window++ * b0[0x080];
      sum -= *window++ * b1[0x0a0];
      sum += *window++ * b0[0x0c0];
      sum -= *window++ * b1[0x0e0];
      sum += *window++ * b0[0x100];
      sum -= *window++ * b1[0x120];
      sum += *window++ * b0[0x140];
      sum -= *window++ * b1[0x160];
      sum += *window++ * b0[0x180];
      sum -= *window++ * b1[0x1a0];
      sum += *window++ * b0[0x1c0];
      sum -= *window++ * b1[0x1e0];

      WRITE_SAMPLE(samples,sum,clip);
    }

    {
      sum  = window[0] * b0[0x000];
      sum += window[2] * b0[0x040];
      sum += window[4] * b0[0x080];
      sum += window[6] * b0[0x0c0];
      sum += window[8] * b0[0x100];
      sum += window[10] * b0[0x140];
      sum += window[12] * b0[0x180];
      sum += window[14] * b0[0x1c0];
      WRITE_SAMPLE(samples,sum,clip);
      b0--,b1++,window-=32,samples+=step;
    }
    window += bo<<1;

    for (j=15;j;j--,b0--,b1++,window-=16,samples+=step)
    {
      sum = -*(--window) * b0[0x000];
      sum -= *(--window) * b1[0x020];
      sum -= *(--window) * b0[0x040];
      sum -= *(--window) * b1[0x060];
      sum -= *(--window) * b0[0x080];
      sum -= *(--window) * b1[0x0a0];
      sum -= *(--window) * b0[0x0c0];
      sum -= *(--window) * b1[0x0e0];
      sum -= *(--window) * b0[0x100];
      sum -= *(--window) * b1[0x120];
      sum -= *(--window) * b0[0x140];
      sum -= *(--window) * b1[0x160];
      sum -= *(--window) * b0[0x180];
      sum -= *(--window) * b1[0x1a0];
      sum -= *(--window) * b0[0x1c0];
      sum -= *(--window) * b1[0x1e0];


      WRITE_SAMPLE(samples,sum,clip);
    }
  }
  else {
    register int j;
    register real *window,*b0,*b1,sum;
    window = decwin + 16 - bo;

    b0 = buf + 15;
    b1 = buf + 15;
    for (j=16;j;j--,b0--,b1++,window+=16,samples+=step) 
    {
      sum = -*window++ * b0[0x000];
      sum += *window++ * b1[0x020];
      sum -= *window++ * b0[0x040];
      sum += *window++ * b1[0x060];
      sum -= *window++ * b0[0x080];
      sum += *window++ * b1[0x0a0];
      sum -= *window++ * b0[0x0c0];
      sum += *window++ * b1[0x0e0];
      sum -= *window++ * b0[0x100];
      sum += *window++ * b1[0x120];
      sum -= *window++ * b0[0x140];
      sum += *window++ * b1[0x160];
      sum -= *window++ * b0[0x180];
      sum += *window++ * b1[0x1a0];
      sum -= *window++ * b0[0x1c0];
      sum += *window++ * b1[0x1e0];

      WRITE_SAMPLE(samples,sum,clip);
    }
    {
      sum  = window[1] * b1[0x020];
      sum += window[3] * b1[0x060];
      sum += window[5] * b1[0x0a0];
      sum += window[7] * b1[0x0e0];
      sum += window[9] * b1[0x120];
      sum += window[11] * b1[0x160];
      sum += window[13] * b1[0x1a0];
      sum += window[15] * b1[0x1e0];
      WRITE_SAMPLE(samples,sum,clip);
      b0++,b1--,window-=32,samples+=step;
    }
    window += bo<<1;

    for (j=15;j;j--,b0++,b1--,window-=16,samples+=step)
    {
      sum = -*(--window) * b0[0x000];
      sum -= *(--window) * b1[0x020];
      sum -= *(--window) * b0[0x040];
      sum -= *(--window) * b1[0x060];
      sum -= *(--window) * b0[0x080];
      sum -= *(--window) * b1[0x0a0];
      sum -= *(--window) * b0[0x0c0];
      sum -= *(--window) * b1[0x0e0];
      sum -= *(--window) * b0[0x100];
      sum -= *(--window) * b1[0x120];
      sum -= *(--window) * b0[0x140];
      sum -= *(--window) * b1[0x160];
      sum -= *(--window) * b0[0x180];
      sum -= *(--window) * b1[0x1a0];
      sum -= *(--window) * b0[0x1c0];
      sum -= *(--window) * b1[0x1e0];


      WRITE_SAMPLE(samples,sum,clip);
    }
  }

  return(clip);
}

/*
 * -funroll-loops (for gcc) will remove the loops for better performance
 * using loops in the source-code enhances readabillity
 */

static void tr(real *here,real *samples)
{
  real bufs[64];

 {
  register int i,j;
  register real *b1,*b2,*bs,*costab;

  b1 = samples;
  bs = bufs;
  costab = pnts[0]+16;
  b2 = b1 + 32;

  for(i=15;i>=0;i--)
    *bs++ = (*b1++ + *--b2); 
  for(i=15;i>=0;i--)
    *bs++ = (*--b2 - *b1++) * *--costab;

  b1 = bufs;
  costab = pnts[1]+8;
  b2 = b1 + 16;

  {
    for(i=7;i>=0;i--)
      *bs++ = (*b1++ + *--b2); 
    for(i=7;i>=0;i--)
      *bs++ = (*--b2 - *b1++) * *--costab; 
    b2 += 32;
    costab += 8;
    for(i=7;i>=0;i--)
      *bs++ = (*b1++ + *--b2); 
    for(i=7;i>=0;i--)
      *bs++ = (*b1++ - *--b2) * *--costab; 
    b2 += 32;
  }

  bs = bufs;
  costab = pnts[2];
  b2 = b1 + 8;

  for(j=2;j;j--)
  {
    for(i=3;i>=0;i--)
      *bs++ = (*b1++ + *--b2); 
    for(i=3;i>=0;i--)
      *bs++ = (*--b2 - *b1++) * costab[i]; 
    b2 += 16;
    for(i=3;i>=0;i--)
      *bs++ = (*b1++ + *--b2); 
    for(i=3;i>=0;i--)
      *bs++ = (*b1++ - *--b2) * costab[i]; 
    b2 += 16;
  }

  b1 = bufs;
  costab = pnts[3];
  b2 = b1 + 4;

  for(j=4;j;j--)
  {
    *bs++ = (*b1++ + *--b2); 
    *bs++ = (*b1++ + *--b2);
    *bs++ = (*--b2 - *b1++) * costab[1]; 
    *bs++ = (*--b2 - *b1++) * costab[0];
    b2 += 8;
    *bs++ = (*b1++ + *--b2); 
    *bs++ = (*b1++ + *--b2);
    *bs++ = (*b1++ - *--b2) * costab[1]; 
    *bs++ = (*b1++ - *--b2) * costab[0];
    b2 += 8;
  }
  bs = bufs;
  costab = pnts[4];
  b2 = b1 + 2;

  for(j=8;j;j--)
  {
    *bs++ = (*b1++ + *--b2);
    *bs++ = (*--b2 - *b1++) * (*costab);
    b2 += 4;
    *bs++ = (*b1++ + *--b2);
    *bs++ = (*b1++ - *--b2) * (*costab);
    b2 += 4;
  }

 }

/* maybe it's possible to do the reorder now so we can
   program the follwing code with auto increment */

 {
  register real *b1;
  register int i;

  for(b1=bufs,i=8;i;i--,b1+=4)
    b1[2] += b1[3];

  for(b1=bufs,i=4;i;i--,b1+=8)
  {
    b1[4] += b1[6];
    b1[6] += b1[5];
    b1[5] += b1[7];
  }

  for(b1=bufs,i=2;i;i--,b1+=16)
  {
    b1[8]  += b1[12];
    b1[12] += b1[10];
    b1[10] += b1[14];
    b1[14] += b1[9];
    b1[9]  += b1[13];
    b1[13] += b1[11];
    b1[11] += b1[15];
  }
  {
    b1 = bufs+16;
    b1[0]  += b1[8];
    b1[8]  += b1[4];
    b1[4]  += b1[12];
    b1[12] += b1[2];
    b1[2]  += b1[10];
    b1[10] += b1[6];
    b1[6]  += b1[14];
    b1[14] += b1[1];
    b1[1]  += b1[9];
    b1[9]  += b1[5];
    b1[5]  += b1[13];
    b1[13] += b1[3];
    b1[3]  += b1[11];
    b1[11] += b1[7];
    b1[7]  += b1[15];
  }
 }

#if 1
 {
  register real *a1=bufs;
  here[31] = *a1++; here[15] = *a1++; here[23] = *a1++; here[ 7] = *a1++;
  here[27] = *a1++; here[11] = *a1++; here[19] = *a1++; here[ 3] = *a1++;
  here[29] = *a1++; here[13] = *a1++; here[21] = *a1++; here[ 5] = *a1++;
  here[25] = *a1++; here[ 9] = *a1++; here[17] = *a1++; here[ 1] = *a1++;
  here[30] = *a1++; here[14] = *a1++; here[22] = *a1++; here[ 6] = *a1++;
  here[26] = *a1++; here[10] = *a1++; here[18] = *a1++; here[ 2] = *a1++;
  here[28] = *a1++; here[12] = *a1++; here[20] = *a1++; here[ 4] = *a1++;
  here[24] = *a1++; here[ 8] = *a1++; here[16] = *a1++; here[ 0] = *a1;
 }
#else
  {
    static int pos[32] = {
      31,15,23,7 , 27,11,19,3 , 29,13,21,5 , 25,9,17,1 ,
      30,14,22,6 , 26,10,18,2 , 28,12,20,4 , 24,8,16,0 };
    register int i,*p = pos;
    register real *a1;

    a1 = bufs;
    for(i=31;i>=0;i--)
      here[*p++] = *a1++;
  }
#endif

}


