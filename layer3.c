/* 
 * Mpeg Layer-3 audio decoder 
 * --------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp.
 * All rights reserved. See also 'README'
 *
 * - I'm currently working on that .. needs a few more optimizations,
 *   though the code is now fast enough to run in realtime on a Sparc10
 * - a few personal notes are in german .. 
 *
 * used source: 
 *   mpeg1_iis package
 */ 

#include "mpg123.h"

extern void rewindNbits(int bits);

extern int  hsstell(void);
extern void set_pointer(long);
extern int  SubBandSynthesis(real *,int,short *);

extern void huffman_decoder(int ,int *);
extern void huffman_count1(int,int *);

static real ispow[8250];
static real scalepow1[19];
static real scalepow2[19];
static real aa_ca[8],aa_cs[8];
#if 0
static real COS[36][18];
#endif
static real COS1[12][6];
static real win[4][36];
static real win1[4][36];
static real gainpow[256];
#if 0
static real COS18[72];
#endif
static real COS9[9];
static real tfcos36[9];
 
static real tan1_1[16],tan2_1[16],tan1_2[16],tan2_2[16];
static real *tan1,*tan2;

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

  for(i=0;i<256;i++)
    gainpow[i] = pow(2.0,0.25 * ( (double) i - 210.0));

  for(i=0;i<8250;i++)
    ispow[i] = pow((double)i,(double)4.0/3.0);

  for(i=0;i<19;i++)
  {
    scalepow1[i] = pow(2.0, -0.5 * (double) i);
    scalepow2[i] = pow(2.0, -1.0 * (double) i);
  }

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

#if 0
  {
   int k;
   for(k=0,i=0;i<9;i++)
     for(j=1;j<9;j++)
        COS18[k++] = cos( M_PI / 18.0 * (2*i+1) * j );
  }
#endif

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


#if 0
  for(p=0;p<36;p++)
    for(m=0;m<18;m++)
      COS[p][m] = cos( M_PI/72.0*(2.0*(double)p+19.0)*(2.0*(double)m+1.0) );
#endif

  for(i=0;i<15;i++)
  {
    double t = tan((real) i * (M_PI / 12.0));
    tan1_1[i] = t / (1.0+t);
    tan2_1[i] = 1.0 / (1.0 + t);
    tan1_2[i] = t / (1.0+t) / M_SQRT1_2;
    tan2_2[i] = 1.0 / (1.0 + t) / M_SQRT1_2;
  }
}

/*
 * read additional frame-informations
 */

void III_get_side_info(III_side_info_t *si,struct frame *fr)
{
   int ch, gr, i;
   int stereo = fr->stereo;
   int ms_stereo = (fr->mode == MPG_MD_JOINT_STEREO) &&
              (fr->mode_ext & 0x2);

   init_layer3();   

   if(ms_stereo) {
     tan1 = tan1_2; tan2 = tan2_2;
   } 
   else {
     tan1 = tan1_1; tan2 = tan2_1;
   }
 
   si->main_data_begin = getbits(9);
   if (stereo == 1)
     si->private_bits = getbits_fast(5);
   else 
     si->private_bits = getbits_fast(3);

   for (ch=0; ch<stereo; ch++)
       si->ch[ch].scfsi = getbits_fast(4);

   for (gr=0; gr<2; gr++) 
   {
     for (ch=0; ch<stereo; ch++) 
     {
       struct gr_info_s *gr_info = &(si->ch[ch].gr[gr]);

       gr_info->part2_3_length = getbits(12);
       gr_info->big_values = getbits_fast(9);
       gr_info->global_gain = getbits_fast(8);
       if(ms_stereo) {
         gr_info->pow2gain = M_SQRT1_2 * gainpow[gr_info->global_gain];
       }
       else {
         gr_info->pow2gain = gainpow[gr_info->global_gain];
       }
       gr_info->scalefac_compress = getbits_fast(4);
       gr_info->window_switching_flag = get1bit();
       if(gr_info->window_switching_flag) 
       {
         gr_info->block_type = getbits_fast(2);
         gr_info->mixed_block_flag = get1bit();
         for (i=0; i<2; i++)
           gr_info->table_select[i] = getbits_fast(5);
           /* gr_info->table_select[2] not needed because there is no region2 */
         for (i=0; i<3; i++)
           gr_info->full_gain[i] = gr_info->pow2gain * scalepow2[getbits_fast(3)<<1];
               
   /* Set region_count parameters since they are implicit in this case. */       
         if(gr_info->block_type == 0)
         {
           printf("Side info bad: block_type == 0 in split block.\n");
           exit(0);
         }
         else if (gr_info->block_type == 2 && gr_info->mixed_block_flag == 0)
           gr_info->region0_count = 8; /* MI 9; */
         else 
           gr_info->region0_count = 7; /* MI 8; */
         gr_info->region1_count = 20 - gr_info->region0_count;
          /* no region2 */
       }
       else 
       {
         for (i=0; i<3; i++)
           gr_info->table_select[i] = getbits_fast(5);
         gr_info->region0_count = getbits_fast(4);
         gr_info->region1_count = getbits_fast(3);
         gr_info->block_type = 0;
         gr_info->mixed_block_flag = 0;
       }
       gr_info->preflag = get1bit();
       gr_info->scalefac_scale = get1bit();
       gr_info->count1table_select = get1bit();
     }
   }
}

                         
static int slen[2][16] = {{0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4},
                   {0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3}};
static struct sfBI_struct
{
   int l[23];
   int s[14];
} sfBandIndex[3] = 
{
   {{0,4,8,12,16,20,24,30,36,44,52,62,74,90,110,134,162,196,238,288,342,418,576},
    {0,4,8,12,16,22,30,40,52,66,84,106,136,192}},
   {{0,4,8,12,16,20,24,30,36,42,50,60,72,88,106,128,156,190,230,276,330,384,576},
    {0,4,8,12,16,22,28,38,50,64,80,100,126,192}},
   {{0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576},
    {0,4,8,12,16,22,30,42,58,78,104,138,180,192}}
};

/* moegliche optimierung: scalefactors am Stueck in ein Feld laden
   unabhaengig von 'l' und 's' .. i-stereo muss beachtet werden */

/*
 * read scalefactors
 */

void III_get_scale_factors(III_scalefac_t *scalefac,III_side_info_t *si,
                           int gr,int ch,struct frame *fr)
{
  int i;
  struct gr_info_s *gr_info = &(si->ch[ch].gr[gr]);

    if (gr_info->block_type == 2) 
    {
      int *scf,num;

      if (gr_info->mixed_block_flag) 
      { /* MIXED */ /* NEW - ag 11/25 */
         scf=(*scalefac)[ch].l;
         num = slen[0][gr_info->scalefac_compress];
         for (i=8;i;i--)
           *scf++ = getbits(num);
         scf=(*scalefac)[ch].s[3];
         i = 9;
      }
      else
      {
        i = 18;
        scf=(*scalefac)[ch].s[0];
      }

      num = slen[0][gr_info->scalefac_compress];
      for (;i;i--)
        *scf++ = getbits(num);
      num = slen[1][gr_info->scalefac_compress];
      for (i = 18; i; i--)
        *scf++ = getbits(num);
      *scf++ = 0; *scf++ = 0; *scf++ = 0; 
    }
    else 
    {
      int *scf=(*scalefac)[ch].l;
      int num=slen[0][gr_info->scalefac_compress];

      if(!gr) {
         for(i=11;i;i--)
           *scf++ = getbits(num);
         num=slen[1][gr_info->scalefac_compress];
         for(i=10;i;i--)
           *scf++ = getbits(num);
      }
      else {
        int scfsi = si->ch[ch].scfsi;
        if(!(scfsi & 0x8))
          for (i=6;i;i--)
            *scf++ = getbits(num);
        else
          scf+=6; /* set them to zero ???????  */

        if(!(scfsi & 0x4))
          for (i=5;i;i--)
            *scf++ = getbits(num);
        else
          scf+=5; /* set them to zero ??????? */

        num=slen[1][gr_info->scalefac_compress];
        if(!(scfsi & 0x2))
          for(i=5;i;i--)
            *scf++ = getbits(num);
        else
          scf+=5; /* set them to zero ??????? */

        if(!(scfsi & 0x1))
          for (i=5;i;i--)
            *scf++ = getbits(num);
        else
          scf+=5; /* set them to zero ??????? */
      }

      *scf++ = 0;  /* no l[21] in original sources */
      *scf++ = 0; 
    }
}

/*
 * l1,l2,l3 vorab berechnen und dann Huffman decode in
 * das dequantize integrieren .. 
 */
void III_hufman_decode(int is[SBLIMIT][SSLIMIT],III_side_info_t *si,
                       int ch,int gr,int part2_start,struct frame *fr)
{
   struct gr_info_s *gr_info = &(si->ch[ch].gr[gr]);
   int h,l1,l2,l3;
   int *is1 = (int *) is;
   int region1Start,region2Start;
   int bv=gr_info->big_values,remain;
   int part2end = part2_start + si->ch[ch].gr[gr].part2_3_length;

   if (gr_info->block_type == 2)
   {
      region1Start = 36  >>1;  /* sfb[9/3]*3=36 */
      region2Start = 576 >>1; /* No Region2 for short block case. */
   }
   else 
   {
      region1Start = sfBandIndex[fr->sampling_frequency]
                           .l[gr_info->region0_count + 1] >> 1 ; /* MI */
      region2Start = sfBandIndex[fr->sampling_frequency]
                              .l[gr_info->region0_count +
                              gr_info->region1_count + 2] >> 1 ; /* MI */
   }

   if(bv <= region1Start)
   {
     l1 = bv; l2 = 0; l3 = 0;
   }
   else
   {
     l1 = region1Start;
     if(bv <= region2Start)
     {
       l2 = bv - l1;  l3 = 0;
     }
     else
     {
       l2 = region2Start - l1; l3 = bv - region2Start;
     }
   }

   h = gr_info->table_select[0];
   for(;l1;l1--,is1+=2)
     huffman_decoder(h, is1);
   h = gr_info->table_select[1];
   for(;l2;l2--,is1+=2)
     huffman_decoder(h, is1);
   h = gr_info->table_select[2];
   for(;l3;l3--,is1+=2)
     huffman_decoder(h, is1);

   remain = ((SSLIMIT*SBLIMIT)>>1) - bv;
   h = gr_info->count1table_select;
   while( ( hsstell() < part2end ) && remain > 0 ) 
   {
/* maybe we should check borders here .. (remain,part2end) */
/* for odd 'remain' values it could happen that we write
   two more values than wanted */
      huffman_count1(h,is1);
      is1 += 4; remain -= 2;
   }

   if( (h = (part2end-hsstell()) ) )
   {  /* Dismiss stuffing Bits (or rewind) */
     while ( h > 16 ) {
        getbits( 16 );
        h -= 16;
     }
     if(h > 0 )
        getbits( h );
     else
     {  
        remain += 2;
        is1 -= 4;
        fprintf(stderr,"l3/huffman: shouldn't happen?? ... diff: %d\n",h-part2end);
        rewindNbits( -h );
     }
   }

   if(remain > 0)
     memset(is1,0,remain*2*sizeof(int));
}


static int pretab1[22] = {0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,3,2,0};
static int pretab2[22] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* hier ist bereits das reorder integriert, Achtung III_stereo veraendert sich
   bei i_stereo-mode dadurch erheblich sonst bisher keine Probleme entdeckt;
   der nachfolgende code sollte sich dadurch weiter vereinfachen */

void III_dequantize_sample(int is[SBLIMIT][SSLIMIT],real xr[SBLIMIT][SSLIMIT],
                           III_scalefac_t *scalefac,struct gr_info_s *gr_info,
                           int ch,struct frame *fr)
{
   int cb,sfreq=fr->sampling_frequency;
   real *xrpnt = (real *) xr;
   int *ispnt = (int *) is;
   real *scalepow = gr_info->scalefac_scale ? scalepow2 : scalepow1;
   int *scf,*scBI;

   if (gr_info->block_type == 2)
   {
     int maxband[3] = { -1,-1,-1 };
     if (gr_info->mixed_block_flag)
     {
       int maxbandl=-1;
       scf  = (*scalefac)[ch].l;
       scBI = sfBandIndex[sfreq].l;
       for(cb = 0; cb < 8 ; cb++)
       {
         int j;
         real v = gr_info->pow2gain * scalepow[*scf++];
         for(j=*scBI++,j=*scBI-j;j;j--)
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

     scf  = (*scalefac)[ch].s[cb];
     scBI = &sfBandIndex[sfreq].s[cb];
     for(;cb < 13;cb++)
     {
       int sfb_start,sfb_lines,freq,window;
       real *xrpnts = xrpnt;
       sfb_start=*scBI++;
       sfb_lines=*scBI - sfb_start;
       for(window=0; window<3; window++,xrpnts++)
       {
         real v = gr_info->full_gain[window] * scalepow[*scf++];
         xrpnt = xrpnts;
         for(freq=0;freq<sfb_lines;freq++,xrpnt+=3) 
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
   else
   {
     int j,maxbandl=-1;
     int *pretab = gr_info->preflag ? pretab1 : pretab2;
     scf  = (*scalefac)[ch].l;
     scBI = sfBandIndex[sfreq].l;
     for(cb=0;cb<22;cb++)
     {
       real v = gr_info->pow2gain * scalepow[(*scf++) + (*pretab++)];
       for(j=*scBI++,j=*scBI-j;j;j--)
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
 * dequantize sample for channel 2 and MS stereo mode and do
 * the MS stuff at once
 */
void III_dequantize_sample_ms(int is[SBLIMIT][SSLIMIT],real xr[SBLIMIT][SSLIMIT],
	real xr0[SBLIMIT][SSLIMIT],
	III_scalefac_t *scalefac,struct gr_info_s *gr_info,
	int ch,struct frame *fr)
{
   int cb,sfreq=fr->sampling_frequency;
   real *xrpnt = (real *) xr;
   real *xr0pnt = (real *) xr0;
   int *ispnt = (int *) is;
   real *scalepow = gr_info->scalefac_scale ? scalepow2 : scalepow1;
   int *scf,*scBI;

   if (gr_info->block_type == 2)
   {
     int maxband[3] = { -1,-1,-1 };
     if (gr_info->mixed_block_flag)
     {
       int maxbandl=-1;
       scf  = (*scalefac)[ch].l;
       scBI = sfBandIndex[sfreq].l;
       for(cb = 0; cb < 8 ; cb++)
       {
         int j;
         real v = gr_info->pow2gain * scalepow[*scf++];
         for(j=*scBI++,j=*scBI-j;j;j--)
         {
           int val;
           if ( !(val = *ispnt++) )
             *xrpnt++ = *xr0pnt++;
           else
           {
             maxbandl = cb;
             if ( val < 0 ) {
               real a = -ispow[-val] * v;
               *xrpnt++ = *xr0pnt - a;
               *xr0pnt++ += a;
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

     scf  = (*scalefac)[ch].s[cb];
     scBI = &sfBandIndex[sfreq].s[cb];
     for(;cb < 13;cb++)
     {
       int sfb_start,sfb_lines,freq,window;
       real *xrpnts = xrpnt;
       real *xr0pnts = xr0pnt;
       sfb_start=*scBI++;
       sfb_lines=*scBI - sfb_start;
       for(window=0; window<3; window++,xrpnts++,xr0pnts++)
       {
         real v = gr_info->full_gain[window] * scalepow[*scf++];
         xrpnt = xrpnts; xr0pnt = xr0pnts; 
         for(freq=0;freq<sfb_lines;freq++,xrpnt+=3,xr0pnt+=3) 
         {
           int val;
           if (!(val=*ispnt++) )
             *xrpnt = *xr0pnt;
           else
           {
             maxband[window] = cb;
             if ( val < 0 ) {
               real a = -ispow[-val] * v;
               *xrpnt = *xr0pnt - a;
               *xr0pnt += a;
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
     scf  = (*scalefac)[ch].l;
     scBI = sfBandIndex[sfreq].l;
     for(cb=0;cb<22;cb++)
     {
       real v = gr_info->pow2gain * scalepow[(*scf++) + (*pretab++)];
       for(j=*scBI++,j=*scBI-j;j;j--)
       {
         int val;
         if ( !(val = *ispnt++) )
           *xrpnt++ = *xr0pnt++; 
         else
         {
           maxbandl = cb;
           if ( val < 0 ) {
             real a = - ispow[-val] * v;
             *xrpnt++ = *xr0pnt - a;
             *xr0pnt++ += a;
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
}


/* eventuell das joint-stereo-zeug in die dequantize prozedur integrieren; 
   ms_stereo sollte keine probleme machen */
/* precalculate the 3*scalefac and scalefac[sfb+1]-scalefac[sfb] stuff */
/* M_SQRT1_2 bei ms_stereo auch fuer i_stereo einbauen ..benoetigt dann
   2 * 2 tan-tables -> done?*/
/* 
 * III_stereo: calculate real channel values according to the stereo model
 */
void III_stereo(real xr[2][SBLIMIT][SSLIMIT],III_scalefac_t *scalefac,struct gr_info_s *gr_info,struct frame *fr)
{
   struct sfBI_struct *sfBI = sfBandIndex + fr->sampling_frequency;
   int j;

   real *xr0 = (real *) xr[0];
   real *xr1 = (real *) xr[1];

   if(fr->mode != MPG_MD_JOINT_STEREO) 
     return;

   if(fr->mode_ext & 0x1)	/* i_stereo */
   {
     int sb,is_p,do_l;
     char is_pos[576];

     memset(is_pos,0,576);

      if (gr_info->block_type == 2)
      {  
         int sfbcnt;

         if( gr_info->mixed_block_flag )
         {
           sfbcnt=2;
           do_l = 1;
         }
         else
         {
           sfbcnt=-1;
           do_l = 0;
         }

         for ( j=0; j<3; j++ ) /* process each window */
         {                      
             /* get first band with zero values */
           int i,sfb = gr_info->maxband[j];
           if(sfb > 3)
             do_l = 0;

           for(;sfb<12;sfb++)
           {
             is_p = (*scalefac)[1].s[sfb][j]; /* scale: 0-15 */ 
             if(is_p != 7) {
               real t1,t2;
               sb = sfBI->s[sfb+1]-sfBI->s[sfb];
               i = 3*(sfBI->s[sfb]) + j;
               t1 = tan1[is_p]; t2 = tan2[is_p];
               for (; sb > 0; sb--,i+=3)
               {
                 real v = xr0[i];
                 is_pos[i] = !0;
                 xr0[i] = v * t1;
                 xr1[i] = v * t2;
               }
             }
           }

#if 1
/* in the original: copy 10 to 11 , here: copy 11 to 12 */
           is_p = (*scalefac)[1].s[11][j]; /* scale: 0-15 */
           sb = sfBI->s[13]-sfBI->s[12];
           i = 3*(sfBI->s[12]) + j;
#else
           is_p = (*scalefac)[1].s[10][j]; /* scale: 0-15 */
           sb = sfBI->s[12]-sfBI->s[11];
           i = 3*(sfBI->s[11]) + j;
#endif
           if(is_p != 7)
           {
             real t1,t2;
             t1 = tan1[is_p]; t2 = tan2[is_p];
             for ( ; sb > 0; sb--,i+=3 )
             {  
               real v = xr0[i];
               is_pos[i] = !0;
               xr0[i] = v * t1;
               xr1[i] = v * t2;
             }
           }
         } /* end for( j ; .. ; . ) */

         if (do_l)
         {
/* also check l-part, if ALL bands in the three windows are 'empty'
 * and mode = mixed_mode 
 */
           int sfb = gr_info->maxbandl;
           int i = sfBI->l[sfb];

           for ( ; sfb<8; sfb++ )
           {
             is_p = (*scalefac)[1].l[sfb]; /* scale: 0-15 */
             sb = sfBI->l[sfb+1]-sfBI->l[sfb];
             if(is_p != 7) {
               real t1,t2;
               t1 = tan1[is_p]; t2 = tan2[is_p];
               for ( ; sb > 0; sb--,i++)
               {
                 real v = xr0[i];
                 is_pos[i] = !0;
                 xr0[i] = v * t1;
                 xr1[i] = v * t2;
               }
             }
             else 
               i += sb;
           }
         }     
      } 
      else /* ((gr_info->block_type != 2)) */
      {
        int sfb = gr_info->maxbandl;
        int i = sfBI->l[sfb];
        for ( ; sfb<21; sfb++)
        {
          is_p = (*scalefac)[1].l[sfb]; /* scale: 0-15 */
          sb = sfBI->l[sfb+1]-sfBI->l[sfb];
          if(is_p != 7) {
            real t1,t2;
            t1 = tan1[is_p]; t2 = tan2[is_p];
            for ( ; sb > 0; sb--,i++)
            {
               real v = xr0[i];
               is_pos[i] = !0;
               xr0[i] = v * t1;
               xr1[i] = v * t2;
            }
          }
          else
            i += sb;
        }

        is_p = is_pos[ sfBI->l[20] ]; /* copy l-band 20 to l-band 21 */
        if(is_p != 7)
        {
          real t1 = tan1[is_p],t2 = tan2[is_p]; 
          for ( sb = 576 - sfBI->l[21]; sb > 0; sb--,i++ )
          {
            real v = xr0[i];
            is_pos[i] = !0;
            xr0[i] = v * t1;
            xr1[i] = v * t2;
          }
        }
      } /* ... */
   }
}

void III_antialias(real xr[SBLIMIT][SSLIMIT],struct gr_info_s *gr_info,struct frame *fr)
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
#if 0
    real tmpbuf[9];
#endif

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

#if 0
{
		int i;
		register real *cp = COS18;
		register real *tmp = tmpbuf;
		register real *out2 = o2+9;

		for(i=0; i<9; i++,cp+=8) {
			register real sum0 = in[0];
			register real sum1 = in[1];
			sum0 += in[2]  * cp[0]; sum1 += in[3]  * cp[0];
			sum0 += in[4]  * cp[1]; sum1 += in[5]  * cp[1];
			sum0 += in[6]  * cp[2]; sum1 += in[7]  * cp[2];
			sum0 += in[8]  * cp[3]; sum1 += in[9]  * cp[3];
			sum0 += in[10] * cp[4]; sum1 += in[11] * cp[4];
			sum0 += in[12] * cp[5]; sum1 += in[13] * cp[5];
			sum0 += in[14] * cp[6]; sum1 += in[15] * cp[6];
			sum0 += in[16] * cp[7]; sum1 += in[17] * cp[7];

			sum1 *= tfcos36[i]; 
			*out2++ = sum0 + sum1;
			*tmp++  = sum0 - sum1;
		}
}
    {
        register real *w = wintab[block_type];
        register real *out1 = o1;
        register real *ts = tsbuf;
        register real *tmp = tmpbuf;

        ts[SBLIMIT*0] = out1[0] + tmp[8] * w[0];
        ts[SBLIMIT*1] = out1[1] + tmp[7] * w[1];
        ts[SBLIMIT*2] = out1[2] + tmp[6] * w[2];
        ts[SBLIMIT*3] = out1[3] + tmp[5] * w[3];
        ts[SBLIMIT*4] = out1[4] + tmp[4] * w[4];
        ts[SBLIMIT*5] = out1[5] + tmp[3] * w[5];
        ts[SBLIMIT*6] = out1[6] + tmp[2] * w[6];
        ts[SBLIMIT*7] = out1[7] + tmp[1] * w[7];
        ts[SBLIMIT*8] = out1[8] + tmp[0] * w[8];

        ts[SBLIMIT* 9] = out1[9]  + tmp[0] * w[9];
        ts[SBLIMIT*10] = out1[10] + tmp[1] * w[10];
        ts[SBLIMIT*11] = out1[11] + tmp[2] * w[11];
        ts[SBLIMIT*12] = out1[12] + tmp[3] * w[12];
        ts[SBLIMIT*13] = out1[13] + tmp[4] * w[13];
        ts[SBLIMIT*14] = out1[14] + tmp[5] * w[14];
        ts[SBLIMIT*15] = out1[15] + tmp[6] * w[15];
        ts[SBLIMIT*16] = out1[16] + tmp[7] * w[16];
        ts[SBLIMIT*17] = out1[17] + tmp[8] * w[17];
    }
    {
        register real *w = wintab[block_type];
        register real *out2 = o2;

        out2[0] = out2[17] * w[18];
        out2[1] = out2[16] * w[19];
        out2[2] = out2[15] * w[20];
        out2[3] = out2[14] * w[21];
        out2[4] = out2[13] * w[22];
        out2[5] = out2[12] * w[23];
        out2[6] = out2[11] * w[24];
        out2[7] = out2[10] * w[25];
        out2[8] = out2[ 9] * w[26];

        out2[ 9] *= w[27];
        out2[10] *= w[28];
        out2[11] *= w[29];
        out2[12] *= w[30];
        out2[13] *= w[31];
        out2[14] *= w[32];
        out2[15] *= w[33];
        out2[16] *= w[34];
        out2[17] *= w[35];
   }
#else
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
#endif
	}
}

/* 
 * maybe we should do some optimization in mdct12()
 */
static INLINE void mdct12(real *in,real *rawout1,real *rawout2,int sb,real *ts)
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
void III_hybrid(real fsIn[SBLIMIT][SSLIMIT],real tsOut[SSLIMIT][SBLIMIT],
                int ch,struct gr_info_s *gr_info,struct frame *fr)
{
   real *tspnt = (real *) tsOut;
   static real block[2][2][SBLIMIT*18] = { { { 0, } } };
   static int blc[2]={0,0};
   real *rawout1,*rawout2;
   int bt1,bt2;

   bt1 = (gr_info->window_switching_flag && gr_info->mixed_block_flag) ? 0 : gr_info->block_type;
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
  III_scalefac_t III_scalefac;
  III_side_info_t III_side_info;
  int stereo = fr->stereo;
  int sgn=0,single = fr->single;
  int stereo0=0,stereo1=fr->stereo;
  int ms_stereo;

  if(single >= 0 && stereo == 2) {
    if(single == 3) {
      stereo1 = 1;
      stereo0 = 0;
      sgn = 0;
    }
    else 
      stereo1 = (sgn = stereo0 = single) + 1;
  }
  else if(stereo == 1)
    sgn = single = 0;

  III_get_side_info(&III_side_info, fr);
  set_pointer(III_side_info.main_data_begin);
  ms_stereo = (fr->mode == MPG_MD_JOINT_STEREO) && (fr->mode_ext & 0x2);

  for (gr=0;gr<2;gr++) 
  {
    static real hybridIn[2][SBLIMIT][SSLIMIT];
    static real hybridOut[2][SSLIMIT][SBLIMIT];

    for (ch=0; ch<stereo; ch++) 
    {
/*
 * SBLIMIT+1 = slightly oversize because of bug???? in hufman_decode?? 
 * at least it currently may happen, that the hufman decoder writes more then SBLIMIT*SSLIMIT values
 */
      static int is[SBLIMIT+1][SSLIMIT];
      int part2_start = hsstell();
      III_get_scale_factors(&III_scalefac,&III_side_info,gr,ch,fr);
      III_hufman_decode(is, &III_side_info, ch, gr, part2_start,fr);
      if(!ch || !ms_stereo) {
        III_dequantize_sample(is, hybridIn[ch], &III_scalefac,&(III_side_info.ch[ch].gr[gr]), ch, fr);
      }
      else {
        III_dequantize_sample_ms(is, hybridIn[1], hybridIn[0], &III_scalefac,&(III_side_info.ch[ch].gr[gr]), ch, fr);
      }
    }

    III_stereo(hybridIn,&III_scalefac,&(III_side_info.ch[1].gr[gr]), fr);
    if(single == 3) {
      int i;
      for(ss=0;ss<SSLIMIT;ss++)
        for(i=0;i<SBLIMIT;i++)
#if 0
          hybridOut[0][ss][i] = (hybridOut[0][ss][i] + hybridOut[1][ss][i]) * 0.5;
#else
          hybridIn[0][ss][i] = (hybridIn[0][ss][i] + hybridIn[1][ss][i]) * 0.5;
#endif
    }

    for (ch=stereo0; ch<stereo1; ch++) 
    {
      III_antialias(hybridIn[ch],&(III_side_info.ch[ch].gr[gr]), fr);
      III_hybrid(hybridIn[ch], hybridOut[ch], ch,&(III_side_info.ch[ch].gr[gr]), fr);
    }

    for (ss=0;ss<18;ss++)
    {
      if(single >= 0)
      {
        int i;
        clip += SubBandSynthesis (hybridOut[sgn][ss],0,pcm_sample+pcm_point);
        for(i=0;i<32;i++) {
          pcm_sample[pcm_point+1] = pcm_sample[pcm_point];
          pcm_point+=2;
        }
      }
      else {
          clip += SubBandSynthesis (hybridOut[0][ss],0,pcm_sample+pcm_point);
          clip += SubBandSynthesis (hybridOut[1][ss],1,pcm_sample+pcm_point);
          pcm_point += 64;
      }

#ifdef VARMODESUPPORT
      if (playlimit < 128) {
        pcm_point -= playlimit >> 1;
        playlimit = 0;
      }
      else
        playlimit -= 128;
#endif

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
  }
  
  return clip;
}


