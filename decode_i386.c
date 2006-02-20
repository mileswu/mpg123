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
    for (j=16;j;j--,b0++,b1--,window+=32,samples+=step)
    {
      sum  = window[0] * b0[0x000];
      sum -= window[1] * b1[0x020];
      sum += window[2] * b0[0x040];
      sum -= window[3] * b1[0x060];
      sum += window[4] * b0[0x080];
      sum -= window[5] * b1[0x0a0];
      sum += window[6] * b0[0x0c0];
      sum -= window[7] * b1[0x0e0];
      sum += window[8] * b0[0x100];
      sum -= window[9] * b1[0x120];
      sum += window[10] * b0[0x140];
      sum -= window[11] * b1[0x160];
      sum += window[12] * b0[0x180];
      sum -= window[13] * b1[0x1a0];
      sum += window[14] * b0[0x1c0];
      sum -= window[15] * b1[0x1e0];

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

    for (j=15;j;j--,b0--,b1++,window-=32,samples+=step)
    {
      sum  = -window[-1] * b0[0x000];
      sum -= window[-2] * b1[0x020];
      sum -= window[-3] * b0[0x040];
      sum -= window[-4] * b1[0x060];
      sum -= window[-5] * b0[0x080];
      sum -= window[-6] * b1[0x0a0];
      sum -= window[-7] * b0[0x0c0];
      sum -= window[-8] * b1[0x0e0];
      sum -= window[-9] * b0[0x100];
      sum -= window[-10] * b1[0x120];
      sum -= window[-11] * b0[0x140];
      sum -= window[-12] * b1[0x160];
      sum -= window[-13] * b0[0x180];
      sum -= window[-14] * b1[0x1a0];
      sum -= window[-15] * b0[0x1c0];
      sum -= window[-16] * b1[0x1e0];

      WRITE_SAMPLE(samples,sum,clip);
    }
  }
  else {
    register int j;
    register real *window,*b0,*b1,sum;
    window = decwin + 16 - bo;

    b0 = buf + 15;
    b1 = buf + 15;
    for (j=16;j;j--,b0--,b1++,window+=32,samples+=step) 
    {
      sum = -window[0] * b0[0x000];
      sum += window[1] * b1[0x020];
      sum -= window[2] * b0[0x040];
      sum += window[3] * b1[0x060];
      sum -= window[4] * b0[0x080];
      sum += window[5] * b1[0x0a0];
      sum -= window[6] * b0[0x0c0];
      sum += window[7] * b1[0x0e0];
      sum -= window[8] * b0[0x100];
      sum += window[9] * b1[0x120];
      sum -= window[10] * b0[0x140];
      sum += window[11] * b1[0x160];
      sum -= window[12] * b0[0x180];
      sum += window[13] * b1[0x1a0];
      sum -= window[14] * b0[0x1c0];
      sum += window[15] * b1[0x1e0];

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

    for (j=15;j;j--,b0++,b1--,window-=32,samples+=step)
    {
      sum  = -window[-1] * b0[0x000];
      sum -= window[-2] * b1[0x020];
      sum -= window[-3] * b0[0x040];
      sum -= window[-4] * b1[0x060];
      sum -= window[-5] * b0[0x080];
      sum -= window[-6] * b1[0x0a0];
      sum -= window[-7] * b0[0x0c0];
      sum -= window[-8] * b1[0x0e0];
      sum -= window[-9] * b0[0x100];
      sum -= window[-10] * b1[0x120];
      sum -= window[-11] * b0[0x140];
      sum -= window[-12] * b1[0x160];
      sum -= window[-13] * b0[0x180];
      sum -= window[-14] * b1[0x1a0];
      sum -= window[-15] * b0[0x1c0];
      sum -= window[-16] * b1[0x1e0];

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
#if 0
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
#endif
 }

  here[31] = bufs[0]; here[15] = bufs[1]; here[23] = bufs[2]; here[ 7] = bufs[3];
  here[27] = bufs[4]; here[11] = bufs[5]; here[19] = bufs[6]; here[ 3] = bufs[7];
  here[29] = bufs[8]; here[13] = bufs[9]; here[21] = bufs[10]; here[ 5] = bufs[11];
  here[25] = bufs[12]; here[ 9] = bufs[13]; here[17] = bufs[14]; here[ 1] = bufs[15];
#if 0
  here[30] = bufs[16]; here[14] = bufs[17]; here[22] = bufs[18]; here[ 6] = bufs[19];
  here[26] = bufs[20]; here[10] = bufs[21]; here[18] = bufs[22]; here[ 2] = bufs[23];
  here[28] = bufs[24]; here[12] = bufs[25]; here[20] = bufs[26]; here[ 4] = bufs[27];
  here[24] = bufs[28]; here[ 8] = bufs[29]; here[16] = bufs[30]; here[ 0] = bufs[31];
#else
  here[30] = bufs[16+0]+bufs[16+8];  here[14] = bufs[16+1]+bufs[16+9];
  here[22] = bufs[16+2]+bufs[16+10]; here[ 6] = bufs[16+3]+bufs[16+11];
  here[26] = bufs[16+4]+bufs[16+12]; here[10] = bufs[16+5]+bufs[16+13];
  here[18] = bufs[16+6]+bufs[16+14]; here[ 2] = bufs[16+7]+bufs[16+15];
  here[28] = bufs[16+8]+bufs[16+4];  here[12] = bufs[16+9]+bufs[16+5];
  here[20] = bufs[16+10]+bufs[16+6]; here[ 4] = bufs[16+11]+bufs[16+7];
  here[24] = bufs[16+12]+bufs[16+2]; here[ 8] = bufs[16+13]+bufs[16+3];
  here[16] = bufs[16+14]+bufs[16+1]; here[ 0] = bufs[16+15];
#endif

}


