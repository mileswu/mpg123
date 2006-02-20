/* 
 * Mpeg Layer-3 audio decoder 
 * --------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp.
 * All rights reserved. See also 'README'
 *
 * - I'm currently working on that .. needs a few more optimizations,
 *   though the code is now fast enough to run in realtime on a 133Mhz 486
 * - a few personal notes are in german .. 
 *
 * used source: 
 *   mpeg1_iis package
 */ 

#include "mpg123.h"

#define MAP

static real ispow[8207];
static real aa_ca[8],aa_cs[8];
static real COS1[12][6];
static real win[4][36];
static real win1[4][36];
static real gainpow2[256+118];
static real COS9[9];
static real tfcos36[9];

static int slen[2][16] = {
  {0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4},
  {0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3}
};

struct bandInfoStruct {
  int longIdx[23];
  int longDiff[22];
  int shortIdx[14];
  int shortDiff[13];
};

struct bandInfoStruct bandInfo[3] = { 
 { {0,4,8,12,16,20,24,30,36,44,52,62,74, 90,110,134,162,196,238,288,342,418,576},
   {4,4,4,4,4,4,6,6,8, 8,10,12,16,20,24,28,34,42,50,54, 76,158},
   {0,4*3,8*3,12*3,16*3,22*3,30*3,40*3,52*3,66*3, 84*3,106*3,136*3,192*3},
   {4,4,4,4,6,8,10,12,14,18,22,30,56} } ,
 { {0,4,8,12,16,20,24,30,36,42,50,60,72, 88,106,128,156,190,230,276,330,384,576},
   {4,4,4,4,4,4,6,6,6, 8,10,12,16,18,22,28,34,40,46,54, 54,192},
   {0,4*3,8*3,12*3,16*3,22*3,28*3,38*3,50*3,64*3, 80*3,100*3,126*3,192*3},
   {4,4,4,4,6,6,10,12,14,16,20,26,66} } ,
 { {0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576} ,
   {4,4,4,4,4,4,6,6,8,10,12,16,20,24,30,38,46,56,68,84,102, 26} ,
   {0,4*3,8*3,12*3,16*3,22*3,30*3,42*3,58*3,78*3,104*3,138*3,180*3,192*3} ,
   {4,4,4,4,6,8,12,16,20,26,34,42,12} } 
};

#ifdef MAP
#if 0
struct mapping {
  int pos;
  int win;   /* 0: 3 = l, 0-2 = window */
  int cb;    /* cb for max-win calc */
  int scale; /* pos of scale value */
};
#endif

int map[3][3][405]; /* 402,405,310 */
int *mapend[3][3];
struct newhuff
{
  unsigned int linbits;
  short *table;
};
extern struct newhuff ht[];
extern struct newhuff htc[];
#endif
 
static real tan1_1[16],tan2_1[16],tan1_2[16],tan2_2[16];

/* 
 * init tables for layer-3 
 */
void init_layer3(void)
{
  int i,j,m;
  static int init=0;
  static double Ci[8]={-0.6,-0.535,-0.33,-0.185,-0.095,-0.041,-0.0142,-0.0037};
  double sq;

  if(init)
    return;
  init = 1;

  for(i=-256;i<118;i++)
    gainpow2[i+256] = pow((double)2.0,(double) 0.25 * ( (double) - i - 210.0 ));

  for(i=0;i<8207;i++)
    ispow[i] = pow((double)i,(double)4.0/3.0);

  for (i=0;i<8;i++)
  {
    sq=sqrt(1.0+Ci[i]*Ci[i]);
    aa_cs[i] = 1.0/sq;
    aa_ca[i] = Ci[i]/sq;
  }

   for(i=0;i<18;i++)
   {
     win[0][i]    = win[1][i]    = sin( M_PI/36.0 * ((double)i+0.5) );
     win[0][i+18] = win[3][i+18] = sin( M_PI/36.0 * ((double)i+18.5) );
   }
   for(i=0;i<6;i++)
   {
     win[1][i+18] = win[3][i+12] = 1.0;
     win[1][i+24] =                sin( M_PI/12.0 *((double)i+6.5) );
     win[1][i+30] = win[3][i]    = 0.0;
                    win[3][i+6 ] = sin( M_PI/12.0 *((double)i+0.5) );
   }

   for(j=0;j<4;j++) {
     if(j == 2)
       continue;
     for(i=0;i<9;i++)
       win[j][i] *= 0.5 / ( cos ( ( M_PI * ( (i+9) * 2.0 + 1.0)) / 72.0 ) );
     for(i=9;i<27;i++)
       win[j][i] *= -0.5 / ( cos ( ( M_PI * ( (26-i) * 2.0 + 1.0)) / 72.0 ) );
     for(i=27;i<36;i++)
       win[j][i] *= -0.5 / ( cos ( ( M_PI * ( (i-27) * 2.0 + 1.0)) / 72.0 ) );

   }

  for(i=0;i<9;i++)
    COS9[i] = cos( M_PI / 18.0 * i);


   for(i=0;i<9;i++)
     tfcos36[i] = 1.0 / ( 2.0 * cos ( ( M_PI * (i * 2.0 + 1.0)) / 36.0 ) );
 
  for(i=0;i<12;i++)
  {
    win[2][i] = sin( M_PI/12.0*((double)i+0.5) ) ;
    for(m=0;m<6;m++)
      COS1[i][m] = cos( M_PI/24.0*(2.0*(double)i+7.0)*(2.0*(double)m+1.0) );
  }

  for(j=0;j<4;j++) {
    for(i=0;i<36;i+=2)
      win1[j][i] = win[j][i];
    for(i=1;i<36;i+=2)
      win1[j][i] = - win[j][i];
  }

  for(i=0;i<15;i++)
  {
    double t = tan((double) i * (M_PI / 12.0));
    tan1_1[i] = t / (1.0+t);
    tan2_1[i] = 1.0 / (1.0 + t);
    tan1_2[i] = M_SQRT2 * t / (1.0+t);
    tan2_2[i] = M_SQRT2 / (1.0 + t);
  }

#ifdef MAP
  for(m=0;m<3;m++)
  {
   struct bandInfoStruct *bi = &bandInfo[m];
   int *mp;
   int cb,win,scf;
   int *bdf;
   int oldscale;

   mp = map[m][0];
   bdf = bi->longDiff;
   i = 0; oldscale = -1; scf = 0;
   for(cb = 0; cb < 8 ; cb++,scf++) {
     for(j=*bdf++;j;j-=2,i+=2) { 
       if(oldscale != scf) {
         *mp++ = 1;
         *mp++ = i;
         *mp++ = 3;
         *mp++ = cb;
         oldscale = scf;
       }
       else 
         *mp++ = 0;
     }   
   }
   bdf = bi->shortDiff+3;
   for(cb=3;cb<13;cb++) {
     int l = *bdf++;
     for(win=0;win<3;win++,scf++) {
       for(j=0;j<l;j+=2) {
         if(oldscale != scf) {
           *mp++ = 1;
           *mp++ = i + j*3 + win;
           *mp++ = win;
           *mp++ = cb;
           oldscale = scf;
         }
         else
           *mp++ = 0;
       }
     }
     i += 3*l;
   }
   mapend[m][0] = mp;

   i = 0; oldscale = -1; scf=0;
   mp = map[m][1];
   bdf = bi->shortDiff+0;
   for(cb=0;cb<13;cb++) {
     int l = *bdf++;
     for(win=0;win<3;win++,scf++) {
       for(j=0;j<l;j+=2) {
         if(oldscale != scf) {
           *mp++ = 1;
           *mp++ = i + j*3 + win;
           *mp++ = win;
           *mp++ = cb;
           oldscale = scf;
         }
         else
           *mp++ = 0;
       }
     }
     i += 3*l;
   }
   mapend[m][1] = mp;

   mp = map[m][2];
   bdf = bi->longDiff;
   i = 0; oldscale = -1; scf = 0;
   for(cb = 0; cb < 22 ; cb++,scf++) {
     for(j=*bdf++;j;j-=2,i+=2) { 
       if(oldscale != scf) {
         *mp++ = 1;
         *mp++ = cb;
         oldscale = scf;
       } 
       else 
         *mp++ = 0;
     }   
   }
   mapend[m][2] = mp;

  }  
#endif
}

/*
 * read additional side information
 */
static void III_get_side_info(struct III_sideinfo *si,int stereo,int ms_stereo,long sfreq)
{
   int ch, gr;

   init_layer3();   

   si->main_data_begin = getbits(9);
   if (stereo == 1)
     si->private_bits = getbits_fast(5);
   else 
     si->private_bits = getbits_fast(3);

   for (ch=0; ch<stereo; ch++) {
       si->ch[ch].gr[0].scfsi = -1;
       si->ch[ch].gr[1].scfsi = getbits_fast(4);
   }

   for (gr=0; gr<2; gr++) 
   {
     for (ch=0; ch<stereo; ch++) 
     {
       register struct gr_info_s *gr_info = &(si->ch[ch].gr[gr]);

       gr_info->part2_3_length = getbits(12);
       gr_info->big_values = getbits_fast(9);
       gr_info->pow2gain = gainpow2+256 - getbits_fast(8);
       if(ms_stereo)
         gr_info->pow2gain += 2;
       gr_info->scalefac_compress = getbits_fast(4);
/* window-switching flag == 1 for block_Type != 0 .. and block-type == 0 -> win-sw-flag = 0 */
       if(get1bit()) 
       {
         int i;
         gr_info->block_type = getbits_fast(2);
         gr_info->mixed_block_flag = get1bit();
         gr_info->table_select[0] = getbits_fast(5);
         gr_info->table_select[1] = getbits_fast(5); /* table_select[2] not needed, because there is no region2 */
         for(i=0;i<3;i++)
           gr_info->full_gain[i] = gr_info->pow2gain + (getbits_fast(3)<<3);

         if(gr_info->block_type == 0) {
           fprintf(stderr,"Blocktype == 0 and window-switching == 1 not allowed.\n");
           exit(1);
         }
         /* region_count/start parameters are implicit in this case. */       
         gr_info->region1start = 36>>1;
         gr_info->region2start = 576>>1;
       }
       else 
       {
         int i,r0c,r1c;
         for (i=0; i<3; i++)
           gr_info->table_select[i] = getbits_fast(5);
         r0c = getbits_fast(4);
         r1c = getbits_fast(3);
         gr_info->region1start = bandInfo[sfreq].longIdx[r0c+1] >> 1 ;
         gr_info->region2start = bandInfo[sfreq].longIdx[r0c+1+r1c+1] >> 1;
         gr_info->block_type = 0;
         gr_info->mixed_block_flag = 0;
       }
       gr_info->preflag = get1bit();
       gr_info->scalefac_scale = get1bit();
       gr_info->count1table_select = get1bit();
     }
   }
}

/*
 * read scalefactors
 */
static void III_get_scale_factors(int *scf,struct gr_info_s *gr_info)
{
    if (gr_info->block_type == 2) 
    {
      int i=18;
      int num = slen[0][gr_info->scalefac_compress];

      if (gr_info->mixed_block_flag) {
         for (i=8;i;i--)
           *scf++ = getbits_fast(num);
         i = 9;
      }

      for (;i;i--)
        *scf++ = getbits_fast(num);
      num = slen[1][gr_info->scalefac_compress];
      for (i = 18; i; i--)
        *scf++ = getbits_fast(num);
      *scf++ = 0; *scf++ = 0; *scf++ = 0; /* short[13][0..2] = 0 */
    }
    else 
    {
      int i,num=slen[0][gr_info->scalefac_compress];
      int scfsi = gr_info->scfsi;

      if(scfsi < 0) { /* scfsi < 0 => granule == 0 */
         for(i=11;i;i--)
           *scf++ = getbits_fast(num);
         num=slen[1][gr_info->scalefac_compress];
         for(i=10;i;i--)
           *scf++ = getbits_fast(num);
      }
      else {
        if(!(scfsi & 0x8))
          for (i=6;i;i--)
            *scf++ = getbits_fast(num);
        else {
          *scf++ = 0; *scf++ = 0; *scf++ = 0;  /* set to ZERO necessary? */
          *scf++ = 0; *scf++ = 0; *scf++ = 0;
        }

        if(!(scfsi & 0x4))
          for (i=5;i;i--)
            *scf++ = getbits_fast(num);
        else {
          *scf++ = 0; *scf++ = 0; *scf++ = 0;  /* set to ZERO necessary? */
          *scf++ = 0; *scf++ = 0;
        }

        num=slen[1][gr_info->scalefac_compress];
        if(!(scfsi & 0x2))
          for(i=5;i;i--)
            *scf++ = getbits_fast(num);
        else {
          *scf++ = 0; *scf++ = 0; *scf++ = 0;  /* set to ZERO necessary? */
          *scf++ = 0; *scf++ = 0;
        }

        if(!(scfsi & 0x1))
          for (i=5;i;i--)
            *scf++ = getbits_fast(num);
        else {
          *scf++ = 0; *scf++ = 0; *scf++ = 0;  /* set to ZERO necessary? */
          *scf++ = 0; *scf++ = 0;
        }
      }

      *scf++ = 0;  /* no l[21] in original sources */
    }
}

#ifndef MAP
/*
 * l1,l2,l3 vorab berechnen und dann Huffman decode in
 * das dequantize integrieren .. 
 */
static void III_hufman_decode(int is[SBLIMIT][SSLIMIT],struct gr_info_s *gr_info,int part2_start)
{
   int h,l1,l2,l3,l4;
   int *is1 = (int *) is;
   int bv       = gr_info->big_values;
   int part2end = gr_info->part2_3_length + part2_start;
   int region1  = gr_info->region1start;
   int region2  = gr_info->region2start;

   l4 = ((576>>1)-bv);
   if(bv <= region1) {
     l1 = bv; l2 = 0; l3 = 0;
   }
   else {
     l1 = region1;
     if(bv <= region2) {
       l2 = bv - l1;  l3 = 0;
     }
     else {
       l2 = region2 - l1; l3 = bv - region2;
     }
   }

   h = gr_info->table_select[0];
   for(;l1>0;l1--,is1+=2)
     huffman_decoder(h, is1);
   h = gr_info->table_select[1];
   for(;l2>0;l2--,is1+=2)
     huffman_decoder(h, is1);
   h = gr_info->table_select[2];
   for(;l3>0;l3--,is1+=2)
     huffman_decoder(h, is1);

   l4 >>= 1;

/* 
 * we may loss the 'odd' bit here ..
 * we just clear the two corresponding fields 
 * check this policy later ..
 * we omit a buffer overrun 
 */

   h = gr_info->count1table_select;
   for(;l4 > 0 && hsstell() < part2end;l4--,is1+=4) 
     huffman_count1(h,is1);

   if( (h = (part2end-hsstell()) ) )
   {  /* Dismiss stuffing Bits (or rewind) */
     while ( h > 16 ) {
        getbits(16);
        h -= 16;
     }
     if(h >= 0 )
        getbits(h);
     else
     {  
        fprintf(stderr,"mpg123: Can't rewind stream by %d bits!\n",-h);
#if 0
        l4 += 2; is1 -= 4;
        rewindNbits( -h );
#else
        exit(1);
#endif
     }
   }

   memset(is1,0,(sizeof(int)*576) - ((char *)is1-(char *)is) );
}
#endif

static int pretab1[22] = {0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,3,2,0};
static int pretab2[22] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

#ifdef MAP
/*
 * don't forget to apply the same changes to III_dequantize_sample_ms() !!! 
 * (note: maxband stuff would only be necessary for second channel and I-stero)
 */
static void III_dequantize_sample(real xr[SBLIMIT][SSLIMIT],int *scf,
   struct gr_info_s *gr_info,int sfreq,int part2_start)
{
  int shift = 1 + gr_info->scalefac_scale;
  real *xrpnt = (real *) xr;
  int l[3],l3;
  int bv       = gr_info->big_values;
  int part2end = gr_info->part2_3_length + part2_start;
  int region1  = gr_info->region1start;
  int region2  = gr_info->region2start;
  int *me;

  l3 = ((576>>1)-bv)>>1;   
/*
 * we may lose the 'odd' bit here !! 
 * check this later gain 
 */
  if(bv <= region1) {
    l[0] = bv; l[1] = 0; l[2] = 0;
  }
  else {
    l[0] = region1;
    if(bv <= region2) {
      l[1] = bv - l[0];  l[2] = 0;
    }
    else {
      l[1] = region2 - l[0]; l[2] = bv - region2;
    }
  }
 
  if(gr_info->block_type == 2) {
    int i,max[4];
    int step=0,win=0,cb=0;
    register real v = 0.0;
    register int *m;

    if(gr_info->mixed_block_flag) {
      max[3] = -1;
      max[0] = max[1] = max[2] = 2;
      m = map[sfreq][0];
      me = mapend[sfreq][0];
    }
    else {
      max[0] = max[1] = max[2] = -1;
      m = map[sfreq][1];
      me = mapend[sfreq][1];
    }

    for(i=0;i<2;i++) {
      int lp = l[i];
      struct newhuff *h = ht+gr_info->table_select[i];
      for(;lp;lp--) {
        register int x,y;
        if( (*m++) ) {
          xrpnt = ((real *) xr) + (*m++);
          win = *m++;
          cb = *m++;
          if(win == 3) {
            v = gr_info->pow2gain[(*scf++) << shift];
            step = 1;
          }
          else {
            v = gr_info->full_gain[win][(*scf++) << shift];
            step = 3;
          }
        }
        {
          register short *val = h->table;
          while((y=*val++)<0)
            if (get1bit())
              val -= y;
          x = y >> 4;
          y &= 0xf;
        }
        if(x == 15) {
          max[win] = cb;
          x += getbits(h->linbits);
          if(get1bit())
            *xrpnt = -ispow[x] * v;
          else
            *xrpnt =  ispow[x] * v;
        }
        else if(x) {
          max[win] = cb;
          if(get1bit())
            *xrpnt = -ispow[x] * v;
          else
            *xrpnt =  ispow[x] * v;
        }
        else
          *xrpnt = 0.0;
        xrpnt += step;
        if(y == 15) {
          max[win] = cb;
          y += getbits(h->linbits);
          if(get1bit())
            *xrpnt = -ispow[y] * v;
          else
            *xrpnt =  ispow[y] * v;
        }
        else if(y) {
          max[win] = cb;
          if(get1bit())
            *xrpnt = -ispow[y] * v;
          else
            *xrpnt =  ispow[y] * v;
        }
        else
          *xrpnt = 0.0;
        xrpnt += step;
      }
    }

    for(;l3 && hsstell() < part2end;l3--) {
      struct newhuff *h = htc+gr_info->count1table_select;
      register short *val = h->table,a;

      while((a=*val++)<0)
        if (get1bit())
          val -= a;

      for(i=0;i<4;i++) {
        if(!(i & 1) && (*m++) ) {
          xrpnt = ((real *) xr) + (*m++);
          win = *m++;
          cb = *m++;
          if(win == 3) {
            v = gr_info->pow2gain[(*scf++) << shift];
            step = 1;
          }
          else {
            v = gr_info->full_gain[win][(*scf++) << shift];
            step = 3;
          }
        }
        if( (a & (0x8>>i)) ) {
          max[win] = cb;
          if(get1bit()) 
            *xrpnt = -v;
          else
            *xrpnt = v;
        }
        else
          *xrpnt = 0.0;
        xrpnt += step;
      }
    }
 
    while( m < me ) {
      if(*m++) {
        xrpnt = ((real *) xr) + *m++;
        if( (*m++) == 3)
          step = 1;
        else
          step = 3;
        m++; /* cb */
      }
      *xrpnt = 0.0;
      xrpnt += step;
      *xrpnt = 0.0;
      xrpnt += step;
    }

    gr_info->maxband[0] = max[0]+1;
    gr_info->maxband[1] = max[1]+1;
    gr_info->maxband[2] = max[2]+1;
    gr_info->maxbandl = max[3]+1;
  }
  else {
    int *pretab = gr_info->preflag ? pretab1 : pretab2;
    int i,max = -1;
    int cb = 0;
    int *m = map[sfreq][2];
    register real v = 0.0;
    me = mapend[sfreq][2];

    for(i=0;i<3;i++) {
      int lp = l[i];
      struct newhuff *h = ht+gr_info->table_select[i];

      for(;lp;lp--) {
        int x,y;
        if( (*m++) ) {
          v = gr_info->pow2gain[((*scf++) + (*pretab++)) << shift];
          cb = *m++;
        }
        {
          register short *val = h->table;
          while((y=*val++)<0)
            if (get1bit())
              val -= y;
          x = y >> 4;
          y &= 0xf;
        }
        if (x == 15) {
          max = cb;
          x += getbits(h->linbits);
          if(get1bit())
            *xrpnt++ = -ispow[x] * v;
          else
            *xrpnt++ =  ispow[x] * v;
        }
        else if(x) {
          max = cb;
          if(get1bit())
            *xrpnt++ = -ispow[x] * v;
          else
            *xrpnt++ =  ispow[x] * v;
        }
        else
          *xrpnt++ = 0.0;

        if (y == 15) {
          max = cb;
          y += getbits(h->linbits);
          if(get1bit())
            *xrpnt++ = -ispow[y] * v;
          else
            *xrpnt++ =  ispow[y] * v;
        }
        else if(y) {
          max = cb;
          if(get1bit())
            *xrpnt++ = -ispow[y] * v;
          else
            *xrpnt++ =  ispow[y] * v;
        }
        else
          *xrpnt++ = 0.0;
      }
    }
    for(;l3 && hsstell() < part2end;l3--) {
      struct newhuff *h = htc+gr_info->count1table_select;
      register short *val = h->table,a;

      while((a=*val++)<0)
        if (get1bit())
          val -= a;

      for(i=0;i<4;i++) {
        if(!(i & 1) && (*m++) ) {
          v = gr_info->pow2gain[((*scf++) + (*pretab++)) << shift];
          cb = *m++;
        }
        if ( (a & (0x8>>i)) ) {
          max = cb;
          if(get1bit())
            *xrpnt++ = -v;
          else
            *xrpnt++ = v;
        }
        else
          *xrpnt++ = 0.0;
      }
    }
    for(i=(&xr[SBLIMIT][SSLIMIT]-xrpnt)>>1;i;i--) {
      *xrpnt++ = 0.0;
      *xrpnt++ = 0.0;
    }

    gr_info->maxbandl = max+1;
  }

  {
    int h;
    if( (h = (part2end-hsstell()) ) ) {
      while ( h > 16 ) {
         getbits(16); /* Dismiss stuffing Bits */
         h -= 16;
      }
      if(h >= 0 )
         getbits(h);
      else {
         fprintf(stderr,"mpg123: Can't rewind stream by %d bits!\n",-h);
         exit(1);
      }
    }
  }
}

static void III_dequantize_sample_ms(real xr[2][SBLIMIT][SSLIMIT],int *scf,
   struct gr_info_s *gr_info,int sfreq,int part2_start)
{
  int shift = 1 + gr_info->scalefac_scale;
  real *xrpnt = (real *) xr[1];
  real *xr0pnt = (real *) xr[0];
  int l[3],l3;
  int bv       = gr_info->big_values;
  int part2end = gr_info->part2_3_length + part2_start;
  int region1  = gr_info->region1start;
  int region2  = gr_info->region2start;
  int *me;

  l3 = ((576>>1)-bv)>>1;   
/*
 * we may lose the 'odd' bit here !! 
 * check this later gain 
 */
  if(bv <= region1) {
    l[0] = bv; l[1] = 0; l[2] = 0;
  }
  else {
    l[0] = region1;
    if(bv <= region2) {
      l[1] = bv - l[0];  l[2] = 0;
    }
    else {
      l[1] = region2 - l[0]; l[2] = bv - region2;
    }
  }
 
  if(gr_info->block_type == 2) {
    int i,max[4];
    int step=0,win=0,cb=0;
    register real v = 0.0;
    int *m;

    if(gr_info->mixed_block_flag) {
      max[3] = -1;
      max[0] = max[1] = max[2] = 2;
      m = map[sfreq][0];
      me = mapend[sfreq][0];
    }
    else {
      max[0] = max[1] = max[2] = -1;
      m = map[sfreq][1];
      me = mapend[sfreq][1];
    }

    for(i=0;i<2;i++) {
      int lp = l[i];
      struct newhuff *h = ht+gr_info->table_select[i];
      for(;lp;lp--) {
        int x,y;
        if(*m++) {
          xrpnt = ((real *) xr[1]) + *m;
          xr0pnt = ((real *) xr[0]) + *m++;
          win = *m++;
          cb = *m++;
          if(win == 3) {
            v = gr_info->pow2gain[(*scf++) << shift];
            step = 1;
          }
          else {
            v = gr_info->full_gain[win][(*scf++) << shift];
            step = 3;
          }
        }
        {
          register short *val = h->table;
          while((y=*val++)<0)
            if (get1bit())
              val -= y;
          x = y >> 4;
          y &= 0xf;
        }
        if(x == 15) {
          max[win] = cb;
          x += getbits(h->linbits);
          if(get1bit()) {
            real a = ispow[x] * v;
            *xrpnt = *xr0pnt + a;
            *xr0pnt -= a;
          }
          else {
            real a = ispow[x] * v;
            *xrpnt = *xr0pnt - a;
            *xr0pnt += a;
          }
        }
        else if(x) {
          max[win] = cb;
          if(get1bit()) {
            real a = ispow[x] * v;
            *xrpnt = *xr0pnt + a;
            *xr0pnt -= a;
          }
          else {
            real a = ispow[x] * v;
            *xrpnt = *xr0pnt - a;
            *xr0pnt += a;
          }
        }
        else
          *xrpnt = *xr0pnt;
        xrpnt += step;
        xr0pnt += step;

        if(y == 15) {
          max[win] = cb;
          y += getbits(h->linbits);
          if(get1bit()) {
            real a = ispow[y] * v;
            *xrpnt = *xr0pnt + a;
            *xr0pnt -= a;
          }
          else {
            real a = ispow[y] * v;
            *xrpnt = *xr0pnt - a;
            *xr0pnt += a;
          }
        }
        else if(y) {
          max[win] = cb;
          if(get1bit()) {
            real a = ispow[y] * v;
            *xrpnt = *xr0pnt + a;
            *xr0pnt -= a;
          }
          else {
            real a = ispow[y] * v;
            *xrpnt = *xr0pnt - a;
            *xr0pnt += a;
          }
        }
        else
          *xrpnt = *xr0pnt;
        xrpnt += step;
        xr0pnt += step;
      }
    }

    for(;l3 && hsstell() < part2end;l3--) {
      struct newhuff *h = htc+gr_info->count1table_select;
      register short *val = h->table,a;

      while((a=*val++)<0)
        if (get1bit())
          val -= a;

      for(i=0;i<4;i++) {
        if(!(i & 1) && (*m++) ) {
          xrpnt = ((real *) xr[1]) + *m;
          xr0pnt = ((real *) xr[0]) + *m++;
          win = *m++;
          cb = *m++;
          if(win == 3) {
            v = gr_info->pow2gain[(*scf++) << shift];
            step = 1;
          }
          else {
            v = gr_info->full_gain[win][(*scf++) << shift];
            step = 3;
          }
        }
        if( (a & (0x8>>i)) ) {
          max[win] = cb;
          if(get1bit()) {
            *xrpnt = *xr0pnt + v;
            *xr0pnt -= v;
          }
          else {
            *xrpnt = *xr0pnt - v;
            *xr0pnt += v;
          }
        }
        else
          *xrpnt = *xr0pnt;
        xrpnt += step;
        xr0pnt += step;
      }
    }
 
    while( m < me ) {
      if(*m++) {
        xrpnt = ((real *) xr) + *m;
        xr0pnt = ((real *) xr) + *m++;
        if(*m++ == 3)
          step = 1;
        else
          step = 3;
        m++; /* cb */
      }
      *xrpnt = *xr0pnt;
      xrpnt += step;
      xr0pnt += step;
      *xrpnt = *xr0pnt;
      xrpnt += step;
      xr0pnt += step;
    }

    gr_info->maxband[0] = max[0]+1;
    gr_info->maxband[1] = max[1]+1;
    gr_info->maxband[2] = max[2]+1;
    gr_info->maxbandl = max[3]+1;
  }
  else {
    int *pretab = gr_info->preflag ? pretab1 : pretab2;
    int i,max = -1;
    int cb = 0;
    register int *m = map[sfreq][2];
    register real v = 0.0;
    me = mapend[sfreq][2];

    for(i=0;i<3;i++) {
      int lp = l[i];
      struct newhuff *h = ht+gr_info->table_select[i];

      for(;lp;lp--) {
        int x,y;
        if(*m++) {
          v = gr_info->pow2gain[((*scf++) + (*pretab++)) << shift];
          cb = *m++;
        }
        {
          register short *val = h->table;
          while((y=*val++)<0)
            if (get1bit())
              val -= y;
          x = y >> 4;
          y &= 0xf;
        }
        if (x == 15) {
          max = cb;
          x += getbits(h->linbits);
          if(get1bit()) {
            real a = ispow[x] * v;
            *xrpnt++ = *xr0pnt + a;
            *xr0pnt++ -= a;
          }
          else {
            real a = ispow[x] * v;
            *xrpnt++ = *xr0pnt - a;
            *xr0pnt++ += a;
          }
        }
        else if(x) {
          max = cb;
          if(get1bit()) {
            real a = ispow[x] * v;
            *xrpnt++ = *xr0pnt + a;
            *xr0pnt++ -= a;
          }
          else {
            real a = ispow[x] * v;
            *xrpnt++ = *xr0pnt - a;
            *xr0pnt++ += a;
          }
        }
        else
          *xrpnt++ = *xr0pnt++;

        if (y == 15) {
          max = cb;
          y += getbits(h->linbits);
          if(get1bit()) {
            real a = ispow[y] * v;
            *xrpnt++ = *xr0pnt + a;
            *xr0pnt++ -= a;
          }
          else {
            real a = ispow[y] * v;
            *xrpnt++ = *xr0pnt - a;
            *xr0pnt++ += a;
          }
        }
        else if(y) {
          max = cb;
          if(get1bit()) {
            real a = ispow[y] * v;
            *xrpnt++ = *xr0pnt + a;
            *xr0pnt++ -= a;
          }
          else {
            real a = ispow[y] * v;
            *xrpnt++ = *xr0pnt - a;
            *xr0pnt++ += a;
          }
        }
        else
          *xrpnt++ = *xr0pnt++;
      }
    }
    for(;l3 && hsstell() < part2end;l3--) {
      struct newhuff *h = htc+gr_info->count1table_select;
      register short *val = h->table,a;

      while((a=*val++)<0)
        if (get1bit())
          val -= a;

      for(i=0;i<4;i++) {
        if(!(i & 1) && *m++) {
          v = gr_info->pow2gain[((*scf++) + (*pretab++)) << shift];
          cb = *m++;
        }
        if ( (a & (0x8>>i)) ) {
          max = cb;
          if(get1bit()) {
            *xrpnt++ = *xr0pnt + v;
            *xr0pnt++ -= v;
          }
          else {
            *xrpnt++ = *xr0pnt - v;
            *xr0pnt++ += v;
          }
        }
        else
          *xrpnt++ = *xr0pnt++;
      }
    }
    for(i=(&xr[1][SBLIMIT][SSLIMIT]-xrpnt)>>1;i;i--) {
      *xrpnt++ = *xr0pnt++;
      *xrpnt++ = *xr0pnt++;
    }

    gr_info->maxbandl = max+1;
  }

  {
    int h;
    if( (h = (part2end-hsstell()) ) ) {
      while ( h > 16 ) {
         getbits(16); /* Dismiss stuffing Bits */
         h -= 16;
      }
      if(h >= 0 )
         getbits(h);
      else {
         fprintf(stderr,"mpg123: Can't rewind stream by %d bits!\n",-h);
         exit(1);
      }
    }
  }
}


#else
static void III_dequantize_sample(int is[SBLIMIT][SSLIMIT],
   real xr[SBLIMIT][SSLIMIT],int *scf,struct gr_info_s *gr_info,int sfreq)
{
   int cb;
   real *xrpnt = (real *) xr;
   int *ispnt = (int *) is;
   int shift = 1 + gr_info->scalefac_scale;
   int *bdf;
   struct bandInfoStruct *bi = &bandInfo[sfreq];

   if (gr_info->block_type == 2)
   {
     int maxband[3];
     if (gr_info->mixed_block_flag)
     {
       int maxbandl=-1;

       bdf = bi->longDiff;
       for(cb = 0; cb < 8 ; cb++)
       {
         int j;
         real v = gr_info->pow2gain[(*scf++) << shift];

         for(j=*bdf++;j;j--)
         {
           int val;
           if ( !(val = *ispnt++) )
             *xrpnt++ = 0.0;
           else
           {
             maxbandl = cb;
             if ( val < 0 )
               *xrpnt++ = -ispow[-val] * v;
             else
               *xrpnt++ = ispow[val] * v;
           }
         }
       }
       cb = 3;
       gr_info->maxbandl = maxbandl + 1;
     }
     else
       cb = 0;

     maxband[0] = maxband[1] = maxband[2] = cb-1;

     bdf = bi->shortDiff+cb;
     for(;cb < 13;cb++)
     {
       int sfb_lines=*bdf++,window;
       real *xrpnts = xrpnt;
       for(window=0; window<3; window++,xrpnts++)
       {
         int j;
         real v = gr_info->full_gain[window][(*scf++) << shift];

         xrpnt = xrpnts;
         for(j=sfb_lines;j;j--,xrpnt+=3) 
         {
           int val;
           if (!(val=*ispnt++) )
             *xrpnt = 0.0;
           else
           {
             maxband[window] = cb;
             if ( val < 0 )
               *xrpnt = -ispow[-val] * v;
             else
               *xrpnt =  ispow[ val] * v;
           }
         }
       }
       xrpnt -= 2;
     }
     gr_info->maxband[0] = maxband[0]+1;
     gr_info->maxband[1] = maxband[1]+1;
     gr_info->maxband[2] = maxband[2]+1;
   }
   else /* (gr_info->block_type != 2) */
   {
     int maxbandl=-1;
     int *pretab = gr_info->preflag ? pretab1 : pretab2;

     bdf = bi->longDiff;
     for(cb=0;cb<22;cb++)
     {
       int j;
       real v = gr_info->pow2gain[((*scf++) + (*pretab++)) << shift];

       for(j=*bdf++;j;j--)
       {
         int val;
         if ( !(val = *ispnt++) )
           *xrpnt++ = 0.0;
         else
         {
           maxbandl = cb;
           if ( val < 0 )
             *xrpnt++ = -ispow[-val] * v;
           else
             *xrpnt++ = ispow[val] * v;
         }
       }
     }
     gr_info->maxbandl = maxbandl+1;
   }
}

/*
 * dequantize sample for channel 2 and do the MS stuff at once
 */
static void III_dequantize_sample_ms(int is[SBLIMIT][SSLIMIT],
  real xr[2][SBLIMIT][SSLIMIT],int *scf,struct gr_info_s *gr_info,int sfreq)
{
   real *xrpnt = (real *) xr[1];
   real *xr0pnt = (real *) xr[0];
#if 0
   int i;
   III_dequantize_sample(is,xr[1],scf,gr_info,sfreq);
   for(i=0;i<576;i++) {
     register real a = *xrpnt;
     *xrpnt++ = *xr0pnt - a;
     *xr0pnt++ += a;
   }
#else

   int cb;
   int *ispnt = (int *) is;
   int shift = gr_info->scalefac_scale + 1;
   int *bdf;
   struct bandInfoStruct *bi = &bandInfo[sfreq];

   if (gr_info->block_type == 2)
   {
     int maxband[3];
     if (gr_info->mixed_block_flag)
     {
       int maxbandl=-1;

       bdf = bi->longDiff;
       for(cb = 0; cb < 8 ; cb++)
       {
         int j;
         real v = gr_info->pow2gain[(*scf++) << shift];

         for(j=*bdf++;j;j--)
         {
           int val;
           if ( !(val = *ispnt++) )
             *xrpnt++ = *xr0pnt++;
           else
           {
             maxbandl = cb;
             if ( val < 0 ) {
               real a = ispow[-val] * v;
               *xrpnt++ = *xr0pnt + a;
               *xr0pnt++ -= a;
             }
             else {
               real a = ispow[val] * v;
               *xrpnt++ = *xr0pnt - a;
               *xr0pnt++ += a;
             }
           }
         }
       }
       cb = 3;
       gr_info->maxbandl = maxbandl + 1;
     }
     else
       cb = 0;

     maxband[0] = maxband[1] = maxband[2] = cb-1;

     bdf = bi->shortDiff+cb;
     for(;cb < 13;cb++)
     {
       int sfb_lines=*bdf++,window;
       real *xrpnts = xrpnt;
       real *xr0pnts = xr0pnt;
       for(window=0; window<3; window++,xrpnts++,xr0pnts++)
       {
         int j;
         real v = gr_info->full_gain[window][(*scf++) << shift];

         xrpnt = xrpnts; xr0pnt = xr0pnts; 
         for(j=sfb_lines;j;j--,xrpnt+=3,xr0pnt+=3) 
         {
           int val;
           if (!(val=*ispnt++) )
             *xrpnt = *xr0pnt;
           else
           {
             maxband[window] = cb;
             if ( val < 0 ) {
               real a = ispow[-val] * v;
               *xrpnt = *xr0pnt + a;
               *xr0pnt -= a;
             }
             else {
               real a = ispow[ val] * v;
               *xrpnt =  *xr0pnt - a;
               *xr0pnt += a;
             }
           }
         }
       }
       xrpnt -= 2; xr0pnt -= 2;
     }
     gr_info->maxband[0] = maxband[0]+1;
     gr_info->maxband[1] = maxband[1]+1;
     gr_info->maxband[2] = maxband[2]+1;
   }
   else
   {
     int j,maxbandl=-1;
     int *pretab = gr_info->preflag ? pretab1 : pretab2;

     bdf = bi->longDiff;
     for(cb=0;cb<22;cb++)
     {
       real v = gr_info->pow2gain[((*scf++) + (*pretab++)) << shift];

       for(j=*bdf++;j;j--)
       {
         int val;
         if ( !(val = *ispnt++) )
           *xrpnt++ = *xr0pnt++; 
         else
         {
           maxbandl = cb;
           if ( val < 0 ) {
             real a = ispow[-val] * v;
             *xrpnt++ = *xr0pnt + a;
             *xr0pnt++ -= a;
           }
           else {
             real a = ispow[val] * v;
             *xrpnt++ = *xr0pnt - a;
             *xr0pnt++ += a;
           }
         }
       }
     }
     gr_info->maxbandl = maxbandl+1;
   }
#endif
}
#endif

/* 
 * III_stereo: calculate real channel values for Joint-I-Stereo-mode
 */
static void III_stereo(real xr_buf[2][SBLIMIT][SSLIMIT],int *scalefac,
   struct gr_info_s *gr_info,int sfreq,int ms_stereo)
{
      real (*xr)[SBLIMIT*SSLIMIT] = (real (*)[SBLIMIT*SSLIMIT] ) xr_buf;
      struct bandInfoStruct *bi = &bandInfo[sfreq];
      real *tan1,*tan2;

      if(ms_stereo) {
        tan1 = tan1_2; tan2 = tan2_2;
      }
      else {
        tan1 = tan1_1; tan2 = tan2_1;
      }

      if (gr_info->block_type == 2)
      {
         int win,do_l = 0;
         if( gr_info->mixed_block_flag )
           do_l = 1;

         for (win=0;win<3;win++) /* process each window */
         {
             /* get first band with zero values */
           int is_p,sb,idx,sfb = gr_info->maxband[win];  /* sfb is minimal 3 for mixed mode */
           if(sfb > 3)
             do_l = 0;

           for(;sfb<12;sfb++)
           {
             is_p = scalefac[sfb*3+win-gr_info->mixed_block_flag]; /* scale: 0-15 */ 
             if(is_p != 7) {
               real t1,t2;
               sb = bi->shortDiff[sfb];
               idx = bi->shortIdx[sfb] + win;
               t1 = tan1[is_p]; t2 = tan2[is_p];
               for (; sb > 0; sb--,idx+=3)
               {
                 real v = xr[0][idx];
                 xr[0][idx] = v * t1;
                 xr[1][idx] = v * t2;
               }
             }
           }

#if 1
/* in the original: copy 10 to 11 , here: copy 11 to 12 
maybe still wrong??? (copy 12 to 13?) */
           is_p = scalefac[11*3+win-gr_info->mixed_block_flag]; /* scale: 0-15 */
           sb = bi->shortDiff[12];
           idx = bi->shortIdx[12] + win;
#else
           is_p = scalefac[10*3+win-gr_info->mixed_block_flag]; /* scale: 0-15 */
           sb = bi->shortDiff[11];
           idx = bi->shortIdx[11] + win;
#endif
           if(is_p != 7)
           {
             real t1,t2;
             t1 = tan1[is_p]; t2 = tan2[is_p];
             for ( ; sb > 0; sb--,idx+=3 )
             {  
               real v = xr[0][idx];
               xr[0][idx] = v * t1;
               xr[1][idx] = v * t2;
             }
           }
         } /* end for(win; .. ; . ) */

         if (do_l)
         {
/* also check l-part, if ALL bands in the three windows are 'empty'
 * and mode = mixed_mode 
 */
           int sfb = gr_info->maxbandl;
           int idx = bi->longIdx[sfb];

           for ( ; sfb<8; sfb++ )
           {
             int sb = bi->longDiff[sfb];
             int is_p = scalefac[sfb]; /* scale: 0-15 */
             if(is_p != 7) {
               real t1,t2;
               t1 = tan1[is_p]; t2 = tan2[is_p];
               for ( ; sb > 0; sb--,idx++)
               {
                 real v = xr[0][idx];
                 xr[0][idx] = v * t1;
                 xr[1][idx] = v * t2;
               }
             }
             else 
               idx += sb;
           }
         }     
      } 
      else /* ((gr_info->block_type != 2)) */
      {
        int sfb = gr_info->maxbandl;
        int is_p,idx = bi->longIdx[sfb];
        for ( ; sfb<21; sfb++)
        {
          int sb = bi->longDiff[sfb];
          is_p = scalefac[sfb]; /* scale: 0-15 */
          if(is_p != 7) {
            real t1,t2;
            t1 = tan1[is_p]; t2 = tan2[is_p];
            for ( ; sb > 0; sb--,idx++)
            {
               real v = xr[0][idx];
               xr[0][idx] = v * t1;
               xr[1][idx] = v * t2;
            }
          }
          else
            idx += sb;
        }

        is_p = scalefac[20]; /* copy l-band 20 to l-band 21 */
        if(is_p != 7)
        {
          int sb;
          real t1 = tan1[is_p],t2 = tan2[is_p]; 

          for ( sb = bi->longDiff[21]; sb > 0; sb--,idx++ )
          {
            real v = xr[0][idx];
            xr[0][idx] = v * t1;
            xr[1][idx] = v * t2;
          }
        }
      } /* ... */
}

static void III_antialias(real xr[SBLIMIT][SSLIMIT],struct gr_info_s *gr_info)
{
   int sblim;

   if(gr_info->block_type == 2)
   {
      if(!gr_info->mixed_block_flag) 
        return;
      else
       sblim = 1; 
   }
   else
     sblim = SBLIMIT-1;

   /* 31 alias-reduction operations between each pair of sub-bands */
   /* with 8 butterflies between each pair                         */

   {
     int sb;
     real *xr1=(real *) xr[1];

     for(sb=sblim;sb;sb--,xr1+=10)
     {
       int ss;
       real *cs=aa_cs,*ca=aa_ca;
       real *xr2 = xr1;
       for(ss=7;ss>=0;ss--)
       {       /* upper and lower butterfly inputs */
         register real bu = *--xr2,bd = *xr1;
         *xr2   = (bu * (*cs)   ) - (bd * (*ca)   );
         *xr1++ = (bd * (*cs++) ) + (bu * (*ca++) );
       }
     }
  }
}

/* This is an optimized DCT from Jeff Tsay's maplay 1.2+ package.
 I've done the 'manual unroll ;)' and saved one multiplication
 by doing it together with the window mul. (MH)

This uses Byeong Gi Lee's Fast Cosine Transform algorithm, but the
9 point IDCT needs to be reduced further. Unfortunately, I don't
know how to do that, because 9 is not an even number. - Jeff.*/

/*------------------------------------------------------------------*/
/*                                                                  */
/*    Function: Calculation of the inverse MDCT                     */
/*    In the case of short blocks the 3 output vectors are already  */
/*    overlapped and added in this modul.                           */
/*                                                                  */
/*    New layer3                                                    */
/*                                                                  */
/*------------------------------------------------------------------*/

static void mdct36(real *inbuf,real *o1,real *o2,int block_type,real wintab[4][36],real *tsbuf)
{
	{
		register real *in = inbuf;

		in[17]+=in[16]; in[16]+=in[15]; in[15]+=in[14];
		in[14]+=in[13]; in[13]+=in[12]; in[12]+=in[11];
		in[11]+=in[10]; in[10]+=in[9];  in[9] +=in[8];
		in[8] +=in[7];  in[7] +=in[6];  in[6] +=in[5];
		in[5] +=in[4];  in[4] +=in[3];  in[3] +=in[2];
		in[2] +=in[1];  in[1] +=in[0];

		in[17]+=in[15]; in[15]+=in[13]; in[13]+=in[11]; in[11]+=in[9];
		in[9] +=in[7];  in[7] +=in[5];  in[5] +=in[3];  in[3] +=in[1];

{

#define MACRO0(v) \
    out2[9+(v)] = (tmp = sum0 + sum1) * w[27+(v)]; \
    out2[8-(v)] = tmp * w[26-(v)];  \
    ts[SBLIMIT*(8-(v))] = out1[8-(v)] + (tmp = sum0 - sum1) * w[8-(v)]; \
    ts[SBLIMIT*(9+(v))] = out1[9+(v)] + tmp * w[9+(v)];

#define MACRO1(v) { \
	real sum0,sum1,tmp; \
    sum0 = tmp1a + tmp2a; \
	sum1 = (tmp1b + tmp2b) * tfcos36[(v)]; \
	MACRO0(v); \
}
#define MACRO2(v) { \
    real sum0,sum1,tmp; \
    sum0 = tmp2a - tmp1a; \
    sum1 = (tmp2b - tmp1b) * tfcos36[(v)]; \
	MACRO0(v); \
}


    register const real *c = COS9;
    register real *out2 = o2;
	register real *w = wintab[block_type];
	register real *out1 = o1;
	register real *ts = tsbuf;

    real ta33,ta66,tb33,tb66;
    real tmp1a,tmp2a,tmp1b,tmp2b;

    ta33 = in[2*3+0] * c[3];
    ta66 = in[2*6+0] * c[6];
    tb33 = in[2*3+1] * c[3];
    tb66 = in[2*6+1] * c[6];
 
    tmp1a =             in[2*1+0] * c[1] + ta33 + in[2*5+0] * c[5] + in[2*7+0] * c[7];
    tmp1b =             in[2*1+1] * c[1] + tb33 + in[2*5+1] * c[5] + in[2*7+1] * c[7];
    tmp2a = in[2*0+0] + in[2*2+0] * c[2] + in[2*4+0] * c[4] + ta66 + in[2*8+0] * c[8];
    tmp2b = in[2*0+1] + in[2*2+1] * c[2] + in[2*4+1] * c[4] + tb66 + in[2*8+1] * c[8];

	MACRO1(0);
	MACRO2(8);

    tmp1a = ( in[2*1+0] - in[2*5+0] - in[2*7+0] ) * c[3];
    tmp1b = ( in[2*1+1] - in[2*5+1] - in[2*7+1] ) * c[3];
    tmp2a = ( in[2*2+0] - in[2*4+0] - in[2*8+0] ) * c[6] - in[2*6+0] + in[2*0+0];
    tmp2b = ( in[2*2+1] - in[2*4+1] - in[2*8+1] ) * c[6] - in[2*6+1] + in[2*0+1];

	MACRO1(1);
	MACRO2(7);

    tmp1a =             in[2*1+0] * c[5] - ta33 - in[2*5+0] * c[7] + in[2*7+0] * c[1];
    tmp1b =             in[2*1+1] * c[5] - tb33 - in[2*5+1] * c[7] + in[2*7+1] * c[1];
    tmp2a = in[2*0+0] - in[2*2+0] * c[8] - in[2*4+0] * c[2] + ta66 + in[2*8+0] * c[4];
    tmp2b = in[2*0+1] - in[2*2+1] * c[8] - in[2*4+1] * c[2] + tb66 + in[2*8+1] * c[4];

	MACRO1(2);
	MACRO2(6);

    tmp1a =             in[2*1+0] * c[7] - ta33 + in[2*5+0] * c[1] - in[2*7+0] * c[5];
    tmp1b =             in[2*1+1] * c[7] - tb33 + in[2*5+1] * c[1] - in[2*7+1] * c[5];
    tmp2a = in[2*0+0] - in[2*2+0] * c[4] + in[2*4+0] * c[8] + ta66 - in[2*8+0] * c[2];
    tmp2b = in[2*0+1] - in[2*2+1] * c[4] + in[2*4+1] * c[8] + tb66 - in[2*8+1] * c[2];

	MACRO1(3);
	MACRO2(5);

	{
		real tmp,sum0,sum1;
    	sum0 =  in[2*0+0] - in[2*2+0] + in[2*4+0] - in[2*6+0] + in[2*8+0];
    	sum1 = (in[2*0+1] - in[2*2+1] + in[2*4+1] - in[2*6+1] + in[2*8+1] ) * tfcos36[4];
		MACRO0(4);
	}
}
	}
}

/* 
 * maybe we should do some optimization in mdct12()
 */
static void mdct12(real *in,real *rawout1,real *rawout2,int sb,real *ts)
{
#define DCTLINE \
    { \
      register int m; \
      register real dum1,dum2; \
      register real *mp = in; \
	\
      asum  = *mp * (dum2 = *cp2++); \
      sum   = (*mp++) * (dum1 = *cp1++); \
      asum1 = *mp * dum2; \
      sum1  = (*mp++) * dum1; \
      asum2 = *mp * dum2; \
      sum2  = (*mp++) * dum1; \
      for(m=4;m>=0;m--) \
      { \
        asum  += *mp * (dum2 = *cp2++); \
        sum   += (*mp++) * (dum1 = *cp1++); \
        asum1 += *mp * dum2; \
        sum1  += (*mp++) * dum1; \
        asum2 += *mp * dum2; \
        sum2  += (*mp++) * dum1; \
      } \
    } 

  real *cp1,*cp2,*wp=win[2];

  rawout2[12]=rawout2[13]=rawout2[14]=rawout2[15]=rawout2[16]=rawout2[17]=0.0;
  ts[SBLIMIT*0] = rawout1[0]; ts[SBLIMIT*1] = rawout1[1]; ts[SBLIMIT*2] = rawout1[2];
  ts[SBLIMIT*3] = rawout1[3]; ts[SBLIMIT*4] = rawout1[4]; ts[SBLIMIT*5] = rawout1[5];

  if(sb) {
    real sum,sum1,sum2,asum,asum1,asum2;

    cp1=COS1[0]; cp2=COS1[6];
    DCTLINE
    {
      register real dum1,dum2;
      dum1 = wp[0];
      dum2 = wp[5];
      ts[SBLIMIT*6]  = rawout1[6]  + sum   * dum1;
      ts[SBLIMIT*11] = rawout1[11] + sum  * dum2;
      ts[SBLIMIT*12] = rawout1[12] + asum  * dum2  + sum1 * dum1;
      ts[SBLIMIT*17] = rawout1[17] - (asum * dum1 - sum1 * dum2);
      rawout2[0]  = asum1  * dum2 + sum2 * dum1;
      rawout2[5]  = - asum1 * dum1 + sum2 * dum2;
      rawout2[6]  = asum2  * dum2;
      rawout2[11] = - asum2 * dum1;
    }
    DCTLINE
    {
      register real dum1,dum2;
      dum1 = wp[1];
      dum2 = wp[5-1];
      ts[SBLIMIT*(6+1)]  = rawout1[6+1] - sum   * dum1;
      ts[SBLIMIT*(11-1)] = rawout1[11-1] - sum  * dum2;
      ts[SBLIMIT*(12+1)] = rawout1[12+1] - (asum  * dum2  + sum1 * dum1);
      ts[SBLIMIT*(17-1)] = rawout1[17-1] + (asum * dum1 - sum1 * dum2);
      rawout2[1]    = - asum1  * dum2  - sum2 * dum1;
      rawout2[5-1]  = asum1 * dum1 - sum2 * dum2;
      rawout2[6+1]  = - asum2  * dum2;
      rawout2[11-1] = asum2 * dum1;
    }
    DCTLINE
    {
      register real dum1,dum2;
      dum1 = wp[2];
      dum2 = wp[5-2];
      ts[SBLIMIT*(6+2)]  = rawout1[6+2]  + sum   * dum1;
      ts[SBLIMIT*(11-2)] = rawout1[11-2] + sum  * dum2;
      ts[SBLIMIT*(12+2)] = rawout1[12+2] + asum  * dum2  + sum1 * dum1;
      ts[SBLIMIT*(17-2)] = rawout1[17-2] - asum * dum1 + sum1 * dum2;
      rawout2[2]    = asum1  * dum2 + sum2 * dum1;
      rawout2[5-2]  = - asum1 * dum1 + sum2 * dum2;
      rawout2[6+2]  = asum2  * dum2;
      rawout2[11-2] = - asum2 * dum1;
    }
  }
  else {
    real sum,sum1,sum2,asum,asum1,asum2;

    cp1=COS1[0]; cp2=COS1[6];
    DCTLINE
    {
      register real dum1,dum2;
      dum1 = wp[0];
      dum2 = wp[5];
      ts[SBLIMIT*(6)]  = rawout1[6] + sum   * dum1;
      ts[SBLIMIT*(11)] = rawout1[11] - sum  * dum2;
      ts[SBLIMIT*(12)] = rawout1[12] + asum  * dum2  + sum1 * dum1;
      ts[SBLIMIT*(17)] = rawout1[17] + asum * dum1 - sum1 * dum2;
      rawout2[0]    = asum1  * dum2  + sum2 * dum1;
      rawout2[5]  = asum1 * dum1 - sum2 * dum2;
      rawout2[6]  = asum2  * dum2;
      rawout2[11] = asum2 * dum1;
    }
    DCTLINE
    {
      register real dum1,dum2;
      dum1 = wp[1];
      dum2 = wp[5-1];
      ts[SBLIMIT*(6+1)]  = rawout1[6+1] + sum   * dum1;
      ts[SBLIMIT*(11-1)] = rawout1[11-1] - sum  * dum2;
      ts[SBLIMIT*(12+1)] = rawout1[12+1] + asum  * dum2  + sum1 * dum1;
      ts[SBLIMIT*(17-1)] = rawout1[17-1] + asum * dum1 - sum1 * dum2;
      rawout2[1]    = asum1  * dum2  + sum2 * dum1;
      rawout2[5-1]  = asum1 * dum1 - sum2 * dum2;
      rawout2[6+1]  = asum2  * dum2;
      rawout2[11-1] = asum2 * dum1;
    }
    DCTLINE
    {
      register real dum1,dum2;
      dum1 = wp[2];
      dum2 = wp[5-2];
      ts[SBLIMIT*(6+2)]  = rawout1[6+2] + sum   * dum1;
      ts[SBLIMIT*(11-2)] = rawout1[11-2] - sum  * dum2;
      ts[SBLIMIT*(12+2)] = rawout1[12+2] + asum  * dum2  + sum1 * dum1;
      ts[SBLIMIT*(17-2)] = rawout1[17-2] + asum * dum1 - sum1 * dum2;
      rawout2[2]    = asum1  * dum2  + sum2 * dum1;
      rawout2[5-2]  = asum1 * dum1 - sum2 * dum2;
      rawout2[6+2]  = asum2  * dum2;
      rawout2[11-2] = asum2 * dum1;

    }

  }
}


/*
 * III_hybrid:
 *   we still need a faster DCT for the mdct36 and mdct12 functions
 *   (though at least the current mdct36 is fairly fast)
 */
static void III_hybrid(real fsIn[SBLIMIT][SSLIMIT],real tsOut[SSLIMIT][SBLIMIT],
   int ch,struct gr_info_s *gr_info)
{
   real *tspnt = (real *) tsOut;
   static real block[2][2][SBLIMIT*SSLIMIT] = { { { 0, } } };
   static int blc[2]={0,0};
   real *rawout1,*rawout2;
   int bt1,bt2;

   bt1 = gr_info->mixed_block_flag ? 0 : gr_info->block_type;
   bt2 = gr_info->block_type;

   {
     int b = blc[ch];
     rawout1=block[b][ch];
     b=-b+1;
     rawout2=block[b][ch];
     blc[ch] = b;
   }

   if(bt2 == 2) {
     int sb;
     for (sb=0; sb<SBLIMIT; sb++,tspnt++,rawout1+=18,rawout2+=18) {
       if( ((sb < 2) ? bt1 : bt2) )
         mdct12(fsIn[sb],rawout1,rawout2,(sb & 1),tspnt);
       else
         mdct36(fsIn[sb],rawout1,rawout2,0,(sb & 1) ? win1 : win,tspnt);
     }
   }
   else {
     int sb;
     for (sb=0; sb<SBLIMIT; sb++,tspnt++,rawout1+=18,rawout2+=18) {
       mdct36(fsIn[sb],rawout1,rawout2,(sb < 2) ? bt1 : bt2,(sb & 1) ? win1 : win,tspnt);
     }
  }
}

int do_layer3(struct frame *fr,int outmode,struct audio_info_struct *ai)
{
  int gr, ch, ss,clip=0;
  int scalefacs[2][39];	/* max 39 for short[13][3] mode, mixed: 38, long: 22 */
  struct III_sideinfo sideinfo;
  int stereo = fr->stereo;
  int single = fr->single;
  int ms_stereo,i_stereo;
  int sfreq = fr->sampling_frequency;
  int stereo1;

  if(stereo == 1) {
    stereo1 = 1;
    single = 0; 
  }
  else if(single >= 0)
    stereo1 = 1;
  else
    stereo1 = 2;

  ms_stereo = (fr->mode == MPG_MD_JOINT_STEREO) && (fr->mode_ext & 0x2);
  i_stereo = (fr->mode == MPG_MD_JOINT_STEREO) && (fr->mode_ext & 0x1);

  III_get_side_info(&sideinfo,stereo,ms_stereo,sfreq);
  set_pointer(sideinfo.main_data_begin);

  for (gr=0;gr<2;gr++) 
  {
    static real hybridIn[2][SBLIMIT][SSLIMIT];
    static real hybridOut[2][SSLIMIT][SBLIMIT];
#ifndef MAP
    static int is[SBLIMIT][SSLIMIT];
#endif

    {
      struct gr_info_s *gr_info = &(sideinfo.ch[0].gr[gr]);
      long part2_start = hsstell();
      III_get_scale_factors(scalefacs[0],gr_info);
#ifdef MAP
      III_dequantize_sample(hybridIn[0], scalefacs[0],gr_info,sfreq,part2_start);
#else
      III_hufman_decode(is,gr_info,part2_start);
      III_dequantize_sample(is, hybridIn[0], scalefacs[0],gr_info,sfreq);
#endif
    }
    if(stereo == 2) {
      struct gr_info_s *gr_info = &(sideinfo.ch[1].gr[gr]);
      long part2_start = hsstell();
      III_get_scale_factors(scalefacs[1],gr_info);
#ifdef MAP
      if(ms_stereo)
        III_dequantize_sample_ms(hybridIn,scalefacs[1],gr_info,sfreq,part2_start); 
      else
        III_dequantize_sample(hybridIn[1],scalefacs[1],gr_info,sfreq,part2_start); 
#else
      III_hufman_decode(is,gr_info, part2_start);
      if(ms_stereo)
        III_dequantize_sample_ms(is,hybridIn,scalefacs[1],gr_info,sfreq);
      else
        III_dequantize_sample(is,hybridIn[1],scalefacs[1],gr_info,sfreq);
#endif

      if(i_stereo)
        III_stereo(hybridIn,scalefacs[1],gr_info,sfreq,ms_stereo);

      if(single >= 0) {
        if(single == 3) {
          register int i;
          register real *in0 = (real *) hybridIn[0],*in1 = (real *) hybridIn[1];
          for(i=0;i<SSLIMIT*SBLIMIT;i++,in0++)
              *in0 = (*in0 + *in1++) * 0.5;
        }
        if(single == 1) {
          register int i;
          register real *in0 = (real *) hybridIn[0],*in1 = (real *) hybridIn[1];
          for(i=0;i<SSLIMIT*SBLIMIT;i++)
            *in0++ = *in1++;
        }
      }
    }

    for(ch=0;ch<stereo1;ch++) {
      struct gr_info_s *gr_info = &(sideinfo.ch[ch].gr[gr]);
      III_antialias(hybridIn[ch],gr_info);
      III_hybrid(hybridIn[ch], hybridOut[ch], ch,gr_info);
    }

    for(ss=0;ss<SSLIMIT;ss++)
    {
      if(single >= 0)
      {
        int i;
        short *pcm = pcm_sample+pcm_point;
        clip += SubBandSynthesis (hybridOut[0][ss],0,pcm);
        for(i=0;i<32;i++,pcm+=2)
          pcm[1] = pcm[0];
      }
      else {
          clip += SubBandSynthesis (hybridOut[0][ss],0,pcm_sample+pcm_point);
          clip += SubBandSynthesis (hybridOut[1][ss],1,pcm_sample+pcm_point);
      }
      pcm_point += 64;

#ifdef VARMODESUPPORT
      if (playlimit < 128) {
        pcm_point -= playlimit >> 1;
        playlimit = 0;
      }
      else
        playlimit -= 128;
#endif

      if(pcm_point >= audiobufsize)
        audio_flush(outmode,ai);
    }
  }
  
  return clip;
}


