/* 
 * Mpeg Layer-2 audio decoder 
 * --------------------------
 * copyright (c) 1995 by Michael Hipp, All rights reserved. See also 'README'
 *
 */

#include "mpg123.h"

extern real muls[27][64];
extern int grp_3tab[32 * 3]; /* filled to 27 */
extern int grp_5tab[128 * 3]; /* filled to 125 */
extern int grp_9tab[1024 * 3]; /* filled to 729 */

extern int  SubBandSynthesis(real *,int,short *);

void II_step_one(unsigned int *bit_alloc,int *scale,struct frame *fr)
{
    int stereo = fr->stereo-1;
    int sblimit = fr->sblimit;
    int jsbound = fr->jsbound;
    int sblimit2 = fr->sblimit<<stereo;
    struct al_table *alloc1 = fr->alloc;
    int i;
    static unsigned int scfsi_buf[64];
    unsigned int *scfsi,*bita;
    int sc,step;

    bita = bit_alloc;
    if(stereo)
    {
      for (i=jsbound;i;i--,alloc1+=(1<<step))
      {
        *bita++ = (char) getbits(step=alloc1->bits);
        *bita++ = (char) getbits(step);
      }
      for (i=sblimit-jsbound;i;i--,alloc1+=(1<<step))
      {
        bita[0] = (char) getbits(step=alloc1->bits);
        bita[1] = bita[0];
        bita+=2;
      }
      bita = bit_alloc;
      scfsi=scfsi_buf;
      for (i=sblimit2;i;i--)
        if (*bita++)
          *scfsi++ = (char) getbits_fast(2);
    }
    else /* mono */
    {
      for (i=sblimit;i;i--,alloc1+=(1<<step))
        *bita++ = (char) getbits(step=alloc1->bits);
      bita = bit_alloc;
      scfsi=scfsi_buf;
      for (i=sblimit;i;i--)
        if (*bita++)
          *scfsi++ = (char) getbits_fast(2);
    }

    bita = bit_alloc;
    scfsi=scfsi_buf;
    for (i=sblimit2;i;i--) 
      if (*bita++)
        switch (*scfsi++) 
        {
          case 0: 
                *scale++ = getbits_fast(6);
                *scale++ = getbits_fast(6);
                *scale++ = getbits_fast(6);
                break;
          case 1 : 
                *scale++ = sc = getbits_fast(6);
                *scale++ = sc;
                *scale++ = getbits_fast(6);
                break;
          case 2: 
                *scale++ = sc = getbits_fast(6);
                *scale++ = sc;
                *scale++ = sc;
                break;
          default:              /* case 3 */
                *scale++ = getbits_fast(6);
                *scale++ = sc = getbits_fast(6);
                *scale++ = sc;
                break;
        }

}

void II_step_two(unsigned int *bit_alloc,real fraction[2][4][SBLIMIT],int *scale,struct frame *fr,int x1)
{
    int i,j,k,ba;
    int stereo = fr->stereo;
    int sblimit = fr->sblimit;
    int jsbound = fr->jsbound;
    struct al_table *alloc2,*alloc1 = fr->alloc;
    unsigned int *bita=bit_alloc;
    int d1,step;

    for (i=0;i<jsbound;i++,alloc1+=(1<<step))
    {
      step = alloc1->bits;
      for (j=0;j<stereo;j++)
      {
        if ( (ba=*bita++) ) 
        {
          k=(alloc2 = alloc1+ba)->bits;
          if( (d1=alloc2->d) < 0) 
          {
            real cm=muls[k][scale[x1]];
            fraction[j][0][i] = ((real) ((int)getbits(k) + d1)) * cm;
            fraction[j][1][i] = ((real) ((int)getbits(k) + d1)) * cm;
            fraction[j][2][i] = ((real) ((int)getbits(k) + d1)) * cm;
          }        
          else 
          {
            static int *table[] = { 0,0,0,grp_3tab,0,grp_5tab,0,0,0,grp_9tab };
            unsigned int idx,*tab,m=scale[x1];
            idx = (unsigned int) getbits(k);
            tab = table[d1] + idx + idx + idx;
            fraction[j][0][i] = muls[*tab++][m];
            fraction[j][1][i] = muls[*tab++][m];
            fraction[j][2][i] = muls[*tab][m];  
          }
          scale+=3;
        }
        else
          fraction[j][0][i] = fraction[j][1][i] = fraction[j][2][i] = 0.0;
      }
    }

    for (i=jsbound;i<sblimit;i++,alloc1+=(1<<step))
    {
      step = alloc1->bits;
      bita++;	/* channel 1 and channel 2 bitalloc are the same */
      if ( (ba=*bita++) )
      {
        k=(alloc2 = alloc1+ba)->bits;
        if( (d1=alloc2->d) < 0)
        {
          real cm;
          cm=muls[k][scale[x1+3]];
          fraction[1][0][i] = (fraction[0][0][i] = (real) ((int)getbits(k) + d1) ) * cm;
          fraction[1][1][i] = (fraction[0][1][i] = (real) ((int)getbits(k) + d1) ) * cm;
          fraction[1][2][i] = (fraction[0][2][i] = (real) ((int)getbits(k) + d1) ) * cm;
          cm=muls[k][scale[x1]];
          fraction[0][0][i] *= cm; fraction[0][1][i] *= cm; fraction[0][2][i] *= cm;
        }
        else
        {
          static int *table[] = { 0,0,0,grp_3tab,0,grp_5tab,0,0,0,grp_9tab };
          unsigned int idx,*tab,m1,m2;
          m1 = scale[x1]; m2 = scale[x1+3];
          idx = (unsigned int) getbits(k);
          tab = table[d1] + idx + idx + idx;
          fraction[0][0][i] = muls[*tab][m1]; fraction[1][0][i] = muls[*tab++][m2];
          fraction[0][1][i] = muls[*tab][m1]; fraction[1][1][i] = muls[*tab++][m2];
          fraction[0][2][i] = muls[*tab][m1]; fraction[1][2][i] = muls[*tab][m2];
        }
        scale+=6;
      }
      else {
        fraction[0][0][i] = fraction[0][1][i] = fraction[0][2][i] =
        fraction[1][0][i] = fraction[1][1][i] = fraction[1][2][i] = 0.0;
      }
/* 
   should we use individual scalefac for channel 2 or
   is the current way the right one , where we just copy channel 1 to
   channel 2 ?? 
   The current 'strange' thing is, that we throw away the scalefac
   values for the second channel ...!!
-> changed .. now we use the scalefac values of channel one !! 
*/
    }

    for(i=sblimit;i<SBLIMIT;i++)
      for (j=0;j<stereo;j++)
        fraction[j][0][i] = fraction[j][1][i] = fraction[j][2][i] = 0.0;

}

int do_layer2(struct frame *fr,int outmode,struct audio_info_struct *ai)
{
  int clip=0;
  int i,j;
  int stereo = fr->stereo;
  real fraction[2][4][SBLIMIT]; /* pick_table clears unused subbands */
  unsigned int bit_alloc[64];
  int scale[192];
  int single = fr->single;

  if(stereo == 1 || single == 3)
    single = 0;

  II_step_one(bit_alloc, scale, fr);

  for (i=0;i<SCALE_BLOCK;i++) 
  {
    II_step_two(bit_alloc,fraction,scale,fr,i>>2);
    for (j=0;j<3;j++) 
    {
      if(single >= 0)
      {
        int k;
        clip += SubBandSynthesis (fraction[single][j],0,pcm_sample+pcm_point);
        for(k=0;k<64;k+=2)
          pcm_sample[pcm_point+k+1] = pcm_sample[pcm_point+k];
      }
      else {
          clip += SubBandSynthesis (fraction[0][j],0,pcm_sample+pcm_point);
          clip += SubBandSynthesis (fraction[1][j],1,pcm_sample+pcm_point);
      }

      pcm_point += 64;
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


