/* 
 * Mpeg Layer-1,2,3 audio decoder 
 * ------------------------------
 * copyright (c) 1995 by Michael Hipp, All rights reserved. See also 'README'
 * slighlty optimized for machines without autoincrement/decrement
 * uses integer arithmetic for final stage
 *
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mpg123.h"

/*
 * currently we need at least 17+1 bits for unreduced window values 
 * on some machines going to 16bit values may increase performance
 * then - of course - you must reduce the window values to 15+1
 */
#define integer long

static void tr(integer *here,real *samples);

static integer win[1024];

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
  integer *stable;
  real *table,*costab;
  static int tablen[3] = { 3 , 5 , 9 };
  static int *itable,*tables[3] = { grp_3tab , grp_5tab , grp_9tab };

  for(i=0;i<5;i++)
  {
	double scale;
	if(i == 0)
		scale = 4096.0;
	else
		scale = 1.0;
    kr=0x10>>i; divv=0x40>>i;
    costab = pnts[i];
    for(k=0;k<kr;k++)
      costab[k] = scale / (2.0 * cos(M_PI * ((double) k * 2.0 + 1.0) / (double) divv));
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

  scaleval = 1;

  stable = win;
  scaleval = -scaleval;
  for(i=0,j=0;i<256;i++,j++,stable+=32)
  {
    stable[16] = stable[0] = (integer) (intwinbase[j]) * scaleval;
    if(i % 32 == 31)
      stable -= 1023;
    if(i % 64 == 63)
      scaleval = - scaleval;
  }

  for( /* i=256 */ ;i<512;i++,j--,stable+=32)  
  {
    stable[16] = stable[0] = (integer) (intwinbase[j]) * scaleval;
    if(i % 32 == 31)
      stable -= 1023;
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
int wrt_sample(short *samples,long sum)
{
      if (sum > 32767*4096*4 )
      {
        *samples = 0x7fff;
        return 1;
      }
      else if(sum < -32767*4096*4)
      {
        *samples = -0x8000;
        return 1;
      }
      else
        *samples = sum>>14;
      return 0;
}

int SubBandSynthesis (real *bandPtr,int channel,short *samples)
{
  static integer buf1[0x200],buf0[0x200];
  static int boc[2]={1,1};
  static integer *buffs[2] = { buf0,buf1 };

  int bo;
  int clip = 0; 
  integer *buf = buffs[channel];

  samples += channel;

  bo = boc[channel];
  bo--;
  bo &= 0xf;
  boc[channel] = bo;

  tr(buf+(bo<<5),bandPtr); /* writes values from buf[0] to buf[3f] */

  if(bo & 0x1) {
    register int j;
    register integer *window,*b0,*b1;
    long sum;
    window = win + 16 - bo;

    b0 = buf + 15;
    b1 = buf + 15;
    for (j=16;j;j--,b0++,b1--,window+=32,samples+=2)
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

      clip += wrt_sample(samples,sum);
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
      b0--,b1++,window+=32,samples+=2;
    }

    for (j=15;j;j--,b0--,b1++,window+=32,samples+=2)
    {
      sum  = window[0] * b0[0x000];
      sum += window[1] * b1[0x020];
      sum += window[2] * b0[0x040];
      sum += window[3] * b1[0x060];
      sum += window[4] * b0[0x080];
      sum += window[5] * b1[0x0a0];
      sum += window[6] * b0[0x0c0];
      sum += window[7] * b1[0x0e0];
      sum += window[8] * b0[0x100];
      sum += window[9] * b1[0x120];
      sum += window[10] * b0[0x140];
      sum += window[11] * b1[0x160];
      sum += window[12] * b0[0x180];
      sum += window[13] * b1[0x1a0];
      sum += window[14] * b0[0x1c0];
      sum += window[15] * b1[0x1e0];

      clip += wrt_sample(samples,sum);
    }
  }
  else {
    register int j;
    register integer *window,*b0,*b1;
    long sum;
    window = win + 16 - bo;

    b0 = buf + 15;
    b1 = buf + 15;
    for (j=16;j;j--,b0--,b1++,window+=32,samples+=2) 
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

      clip += wrt_sample(samples,sum);
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
      b0++,b1--,window+=32,samples+=2;
    }
    for (j=15;j;j--,b0++,b1--,window+=32,samples+=2)
    {
      sum  = window[0] * b0[0x000];
      sum += window[1] * b1[0x020];
      sum += window[2] * b0[0x040];
      sum += window[3] * b1[0x060];
      sum += window[4] * b0[0x080];
      sum += window[5] * b1[0x0a0];
      sum += window[6] * b0[0x0c0];
      sum += window[7] * b1[0x0e0];
      sum += window[8] * b0[0x100];
      sum += window[9] * b1[0x120];
      sum += window[10] * b0[0x140];
      sum += window[11] * b1[0x160];
      sum += window[12] * b0[0x180];
      sum += window[13] * b1[0x1a0];
      sum += window[14] * b0[0x1c0];
      sum += window[15] * b1[0x1e0];

      clip += wrt_sample(samples,sum);
    }
  }

  return(clip);
}

/*
 * -funroll-loops (for gcc) will remove the loops for better performance
 * using loops in the source-code enhances readabillity
 */

static void tr(integer *here,real *samples)
{
  real bufs[64];

 {
  register int i,j;
  register real *b1,*b2,*bs,*costab;

  b1 = samples;
  bs = bufs;
  costab = pnts[0]+16;
  b2 = b1 + 32;

/*
 * for easier implementation we do the scale by 2^12 here 
 * the better solution would be to integrate it into
 * another multiplication in one of the earlier stages
 */

  for(i=15;i>=0;i--)
    *bs++ = 4096.0 * (*b1++ + *--b2); 
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

  here[31] = bufs[0]; here[15] = bufs[1]; 
  here[23] = bufs[2]; here[ 7] = bufs[3];
  here[27] = bufs[4]; here[11] = bufs[5];
  here[19] = bufs[6]; here[ 3] = bufs[7];
  here[29] = bufs[8]; here[13] = bufs[9];
  here[21] = bufs[10]; here[ 5] = bufs[11];
  here[25] = bufs[12]; here[ 9] = bufs[13];
  here[17] = bufs[14]; here[ 1] = bufs[15];
  here[30] = bufs[16]; here[14] = bufs[17];
  here[22] = bufs[18]; here[ 6] = bufs[19];
  here[26] = bufs[20]; here[10] = bufs[21];
  here[18] = bufs[22]; here[ 2] = bufs[23];
  here[28] = bufs[24]; here[12] = bufs[25];
  here[20] = bufs[26]; here[ 4] = bufs[27];
  here[24] = bufs[28]; here[ 8] = bufs[29];
  here[16] = bufs[30]; here[ 0] = bufs[31];

}


