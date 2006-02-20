/* Geez, why are WAV RIFF headers are so secret?  I got something together,
   but wow...  anyway, I hope someone will find this useful.
   - Samuel Audet <guardia@cam.org> */

/* minor simplifications by MH */

#include <stdlib.h>
#include <stdio.h>
#include "mpg123.h"

struct
{
   byte riffheader[4];
   byte WAVElen[4];
   struct
   {
      byte fmtheader[8];
      byte fmtlen[4];
      struct
      {
         byte FormatTag[2];
         byte Channels[2];
         byte SamplesPerSec[4];
         byte AvgBytesPerSec[4];
         byte BlockAlign[2];
         byte BitsPerSample[2]; /* format specific for PCM */
      } fmt;
      struct
      {
         byte dataheader[4];
         byte datalen[4];
         /* from here you insert your PCM data */
      } data;
   } WAVE;
} RIFF = 
{  { 'R','I','F','F' } , { sizeof(RIFF.WAVE),0,0,0 } , 
   {  { 'W','A','V','E','f','m','t',' ' } , { sizeof(RIFF.WAVE.fmt),0,0,0} ,
      { {1,0} , {0,0},{0,0,0,0},{0,0,0,0},{0,0},{0,0} } ,
      { { 'd','a','t','a' }  , {0,0,0,0} }
   }
};


static FILE *wavfp;
static long datalen = 0;
static int flipendian=0;

/* Convertfunctions: */
/* always little endian */

static void long2littleendian(long inval,byte *outval,int b)
{
  int i;
  for(i=0;i<b;i++) {
    outval[i] = (inval>>(i*8)) & 0xff;
  } 
}

int wav_open(struct audio_info_struct *ai, char *wavfilename)
{
   int bps;
   
   flipendian = 0;

   /* standard MS PCM, and its format specific is BitsPerSample */
   long2littleendian(1,RIFF.WAVE.fmt.FormatTag,sizeof(RIFF.WAVE.fmt.FormatTag));

   if(ai->format == AUDIO_FORMAT_SIGNED_16) {
      static long i,a=0,b=0,c=0;
      long2littleendian(bps=16,RIFF.WAVE.fmt.BitsPerSample,sizeof(RIFF.WAVE.fmt.BitsPerSample));
      for(i=0;i<sizeof(long);i++) {
         ((byte *)&a)[i] = i;
         b<<=8;
         b |= i;
         c |= i << (i*8);
      }
      if(a == b)
        flipendian = 1;
      else if(a != c) {
        fprintf(stderr,"Strange endianess?? %08lx %08lx %08lx\n",a,b,c);
        exit(1);
      }
   }
   else if(ai->format == AUDIO_FORMAT_UNSIGNED_8)
      long2littleendian(bps=8,RIFF.WAVE.fmt.BitsPerSample,sizeof(RIFF.WAVE.fmt.BitsPerSample));
   else
   {
      fprintf(stderr,"Format not supported.");
      return -1;
   }

   if(ai->rate < 0) ai->rate = 44100;

   long2littleendian(ai->channels,RIFF.WAVE.fmt.Channels,sizeof(RIFF.WAVE.fmt.Channels));
    long2littleendian(ai->rate,RIFF.WAVE.fmt.SamplesPerSec,sizeof(RIFF.WAVE.fmt.SamplesPerSec));
   long2littleendian((int)(ai->channels * ai->rate * bps)>>3,
         RIFF.WAVE.fmt.AvgBytesPerSec,sizeof(RIFF.WAVE.fmt.AvgBytesPerSec));
   long2littleendian((int)(ai->channels * bps)>>3,
         RIFF.WAVE.fmt.BlockAlign,sizeof(RIFF.WAVE.fmt.BlockAlign));

   wavfp = fopen(wavfilename,"w");
   if(!wavfp)
     return -1;

   long2littleendian(datalen,RIFF.WAVE.data.datalen,sizeof(RIFF.WAVE.data.datalen));
   long2littleendian(datalen+sizeof(RIFF.WAVE),RIFF.WAVElen,sizeof(RIFF.WAVElen));
   fwrite(&RIFF, sizeof(RIFF),1,wavfp);

   datalen = 0;
   
   return 0;
}

int wav_write(unsigned char *buf,int len)
{
   int temp;
   int i;

   if(!wavfp) 
     return 0;
  
   if(flipendian) {
     if(len & 1) {
       fprintf(stderr,"Odd number of bytes!\n");
       exit(1);
     }
     for(i=0;i<len;i+=2) {
       unsigned char tmp;
       tmp = buf[i+0];
       buf[i+0] = buf[i+1];
       buf[i+1] = tmp;
     }
   }

   temp = fwrite(buf, 1, len, wavfp);
   if(temp <= 0)
     return 0;
     
   datalen += temp;

   return temp;
}

int wav_close(void)
{
   if(!wavfp) 
      return 0;

   rewind(wavfp);

   long2littleendian(datalen,RIFF.WAVE.data.datalen,sizeof(RIFF.WAVE.data.datalen));
   long2littleendian(datalen+sizeof(RIFF.WAVE),RIFF.WAVElen,sizeof(RIFF.WAVElen));
   fwrite(&RIFF, sizeof(RIFF),1,wavfp);

   return 0;
}

