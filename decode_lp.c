/* 
 * Mpeg Layer-1,2,3 audio decoder 
 * ------------------------------
 * copyright (c) 1995 by Michael Hipp, All rights reserved. See also 'README'
 * version for slower machines .. decodes only every second sample 
 * sounds like 22050 kHz .. (reducing to 11025 kHz sounds annoying)
 *
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mpg123.h"

static void tr(real *here,real *samples);

static real win[1024];

static real cos64[16],cos32[8],cos16[4],cos8[2],cos4[1];
static real *pnts[] = { cos64,cos32,cos16,cos8,cos4 };

long intwinbase[] = {
     0,    -1,    -1,    -1,    -1,    -1,    -1,    -2,    -2,    -2,
    -2,    -3,    -3,    -4,    -4,    -5,    -5,    -6,    -7,    -7,
    -8,    -9,   -10,   -11,   -13,   -14,   -16,   -17,   -19,   -21,
   -24,   -26,   -29,   -31,   -35,   -38,   -41,   -45,   -49,   -53,
   -58,   -63,   -68,   -73,   -79,   -85,   -91,   -97,  -104,  -111,
  -117,  -125,  -132,  -139,  -147,  -154,  -161,  -169,  -176,  -183,
  -190,  -196,  -202,  -208,  -213,  -218,  -222,  -225,  -227,  -228,
  -228,  -227,  -224,  -221,  -215,  -208,  -200,  -189,  -177,  -163,
  -146,  -127,  -106,   -83,   -57,   -29,     2,    36,    72,   111,
   153,   197,   244,   294,   347,   401,   459,   519,   581,   645,
   711,   779,   848,   919,   991,  1064,  1137,  1210,  1283,  1356,
  1428,  1498,  1567,  1634,  1698,  1759,  1817,  1870,  1919,  1962,
  2001,  2032,  2057,  2075,  2085,  2087,  2080,  2063,  2037,  2000,
  1952,  1893,  1822,  1739,  1644,  1535,  1414,  1280,  1131,   970,
   794,   605,   402,   185,   -45,  -288,  -545,  -814, -1095, -1388,
 -1692, -2006, -2330, -2663, -3004, -3351, -3705, -4063, -4425, -4788,
 -5153, -5517, -5879, -6237, -6589, -6935, -7271, -7597, -7910, -8209,
 -8491, -8755, -8998, -9219, -9416, -9585, -9727, -9838, -9916, -9959,
 -9966, -9935, -9863, -9750, -9592, -9389, -9139, -8840, -8492, -8092,
 -7640, -7134, -6574, -5959, -5288, -4561, -3776, -2935, -2037, -1082,
   -70,   998,  2122,  3300,  4533,  5818,  7154,  8540,  9975, 11455,
 12980, 14548, 16155, 17799, 19478, 21189, 22929, 24694, 26482, 28289,
 30112, 31947, 33791, 35640, 37489, 39336, 41176, 43006, 44821, 46617,
 48390, 50137, 51853, 53534, 55178, 56778, 58333, 59838, 61289, 62684,
 64019, 65290, 66494, 67629, 68692, 69679, 70590, 71420, 72169, 72835,
 73415, 73908, 74313, 74630, 74856, 74992, 75038 };

int grp_3tab[32 * 3] = { 0, };   /* used: 27 */
int grp_5tab[128 * 3] = { 0, };  /* used: 125 */
int grp_9tab[1024 * 3] = { 0, }; /* used: 729 */

real muls[27][64];

void make_decode_tables(long scaleval)
{
  static double mulmul[27] = { 
    0.0 , -2.0/3.0 , 2.0/3.0 ,
    2.0/7.0 , 2.0/15.0 , 2.0/31.0, 2.0/63.0 , 2.0/127.0 , 2.0/255.0 ,
    2.0/511.0 , 2.0/1023.0 , 2.0/2047.0 , 2.0/4095.0 , 2.0/8191.0 ,
    2.0/16383.0 , 2.0/32767.0 , 2.0/65535.0 , 
    -4.0/5.0 , -2.0/5.0 , 2.0/5.0, 4.0/5.0 ,
    -8.0/9.0 , -4.0/9.0 , -2.0/9.0 , 2.0/9.0 , 4.0/9.0 , 8.0/9.0 };
  static int base[3][9] = { 
     { 1 , 0, 2 , } ,
     { 17, 18, 0 , 19, 20 , } ,
     { 21, 1, 22, 23, 0, 24, 25, 2, 26 } };  
  int i,j,k,l,len,kr,divv;
  real *table,*costab;
  static int tablen[3] = { 3 , 5 , 9 };
  static int *itable,*tables[3] = { grp_3tab , grp_5tab , grp_9tab };

  for(i=0;i<5;i++)
  {
    kr=0x10>>i; divv=0x40>>i;
    costab = pnts[i];
    for(k=0;k<kr;k++)
      costab[k] = 1.0 / (2.0 * cos(M_PI * ((double) k * 2.0 + 1.0) / (double) divv));
  }

  for(i=0;i<3;i++)
  {
    itable = tables[i];
    len = tablen[i];
    for(j=0;j<len;j++)
      for(k=0;k<len;k++)
        for(l=0;l<len;l++)
        {
          *itable++ = base[i][l];
          *itable++ = base[i][k];
          *itable++ = base[i][j];
        }
  }

  table = win;
  scaleval = -scaleval;
  for(i=0,j=0;i<256;i++,j++,table+=32)
  {
    table[16] = table[0] = (double) intwinbase[j] / 65536.0 * (double) scaleval;
    if(i % 32 == 31)
      table -= 1023;
    if(i % 64 == 63)
      scaleval = - scaleval;
  }

  for( /* i=256 */ ;i<512;i++,j--,table+=32)  
  {
    table[16] = table[0] = (double) intwinbase[j] / 65536.0 * (double) scaleval;
    if(i % 32 == 31)
      table -= 1023;
    if(i % 64 == 63)
      scaleval = - scaleval;
  }

  for(k=0;k<27;k++)
  {
    double m=mulmul[k];
    table = muls[k];
    for(j=3,i=0;i<63;i++,j--)
      *table++ = m * pow(2.0,(double) j / 3.0);
    *table++ = 0.0;
  }

}

#ifdef _gcc_
static inline 
#else
static
#endif
int wrt_sample(short *samples,real sum)
{
      if (sum > 32767.0)
      {
        *samples = 0x7fff;
        return 1;
      }
      else if(sum < -32768.0)
      {
        *samples = -0x8000;
        return 1;
      }
      else
        *samples = sum;
      return 0;
}

int SubBandSynthesis (real *bandPtr,int channel,short *samples)
{
  static real buf1[0x200],buf0[0x200];
  static int boc[2]={1,1};
  static real *buffs[2] = { buf0,buf1 };

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
    window = win + 16 - bo;

    b0 = buf + 15;
    b1 = buf + 15;
    for (j=8;j;j--,b0+=2,b1-=2,window+=48,samples+=4)
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

      clip += wrt_sample(samples,sum);
      clip += wrt_sample(samples+2,sum);
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
      clip += wrt_sample(samples,sum);
      clip += wrt_sample(samples+2,sum);
      b0-=2,b1+=2,window+=64,samples+=4;
    }

    for (j=7;j;j--,b0-=2,b1+=2,window+=48,samples+=4)
    {
      sum  = *window++ * b0[0x000];
      sum += *window++ * b1[0x020];
      sum += *window++ * b0[0x040];
      sum += *window++ * b1[0x060];
      sum += *window++ * b0[0x080];
      sum += *window++ * b1[0x0a0];
      sum += *window++ * b0[0x0c0];
      sum += *window++ * b1[0x0e0];
      sum += *window++ * b0[0x100];
      sum += *window++ * b1[0x120];
      sum += *window++ * b0[0x140];
      sum += *window++ * b1[0x160];
      sum += *window++ * b0[0x180];
      sum += *window++ * b1[0x1a0];
      sum += *window++ * b0[0x1c0];
      sum += *window++ * b1[0x1e0];

      clip += wrt_sample(samples,sum);
      clip += wrt_sample(samples+2,sum);
    }
  }
  else {
    register int j;
    register real *window,*b0,*b1,sum;
    window = win + 16 - bo;

    b0 = buf + 15;
    b1 = buf + 15;
    for (j=8;j;j--,b0-=2,b1+=2,window+=48,samples+=4) 
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

      clip += wrt_sample(samples,sum);
      clip += wrt_sample(samples+2,sum);
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
      clip += wrt_sample(samples,sum);
      clip += wrt_sample(samples+2,sum);
      b0+=2,b1-=2,window+=64,samples+=4;
    }
    for (j=7;j;j--,b0+=2,b1-=2,window+=48,samples+=4)
    {
      sum  = *window++ * b0[0x000];
      sum += *window++ * b1[0x020];
      sum += *window++ * b0[0x040];
      sum += *window++ * b1[0x060];
      sum += *window++ * b0[0x080];
      sum += *window++ * b1[0x0a0];
      sum += *window++ * b0[0x0c0];
      sum += *window++ * b1[0x0e0];
      sum += *window++ * b0[0x100];
      sum += *window++ * b1[0x120];
      sum += *window++ * b0[0x140];
      sum += *window++ * b1[0x160];
      sum += *window++ * b0[0x180];
      sum += *window++ * b1[0x1a0];
      sum += *window++ * b0[0x1c0];
      sum += *window++ * b1[0x1e0];

      clip += wrt_sample(samples,sum);
      clip += wrt_sample(samples+2,sum);
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


