/* 
 * Mpeg Layer audio decoder V0.55
 * -------------------------------
 * copyright (c) 1995,1996 by Michael Hipp, All rights reserved.
 * See also 'README' !
 *
 * used sources:
 *   mpegaudio package
 */

#include <stdlib.h>

/* #define SET_PRIO */

#ifdef SET_PRIO
#include <sys/resource.h>
#endif

#include "mpg123.h"

char *prgName;

static void usage(void);
static void print_title(void);

int outmode = DECODE_AUDIO;

long outscale  = 32768;
int checkrange = FALSE;
int verbose    = FALSE;
int quiet      = FALSE;
long numframes = -1;
long startFrame = 0;

#ifndef MAXNAMLEN
#define MAXNAMLEN	256
#endif

int main(int argc,char *argv[])
{
    struct frame fr;
    int i, stereo,clip=0;
    int crc_error_count, total_error_count;
    unsigned int old_crc;
    unsigned long frameNum = 0;
    char fname[MAXNAMLEN] = { 0, };
    struct audio_info_struct ai;

#ifdef SET_PRIO
	int mypid = getpid();
	setpriority(PRIO_PROCESS,mypid,-20);
#endif

    fr.single = -1; /* both channels */

    ai.gain = ai.rate = ai.output = -1;
    ai.device = NULL;
    ai.channels = 2;

    prgName = (char *) argv[0];

    if(argc == 1)
      usage();

    for(i=1;i<argc;i++)
    {
      if(!strcmp(argv[i],"-skip"))
      {
        i++;
        startFrame = atoi(argv[i]);
      }
      else if(!strcmp(argv[i],"-audiodevice")) {
        i++;
        ai.device = strdup(argv[i]);
      }
      else if(!strcmp(argv[i],"-t"))
        outmode = DECODE_TEST;
      else if(!strcmp(argv[i],"-s"))
        outmode = DECODE_STDOUT;
      else if(!strcmp(argv[i],"-c"))
        checkrange = TRUE;
      else if(!strcmp(argv[i],"-v") || !strcmp(argv[i],"-verbose"))
        verbose = TRUE;
      else if(!strcmp(argv[i],"-single0") || !strcmp(argv[i],"-left"))
        fr.single = 0;
      else if(!strcmp(argv[i],"-single1") || !strcmp(argv[i],"-right"))
        fr.single = 1;
      else if(!strcmp(argv[i],"-singlemix") || !strcmp(argv[i],"-mix"))
        fr.single = 3;
      else if(!strcmp(argv[i],"-g") || !strcmp(argv[i],"-gain"))
      {
        i++;
        ai.gain = atoi(argv[i]);
      }
      else if(!strcmp(argv[i],"-r"))
      {
        i++;
        ai.rate = atoi(argv[i]);
      }
      else if(!strcmp(argv[i],"-oh"))
        ai.output = AUDIO_OUT_HEADPHONES;
      else if(!strcmp(argv[i],"-os"))
        ai.output = AUDIO_OUT_INTERNAL_SPEAKER;
      else if(!strcmp(argv[i],"-ol"))
        ai.output = AUDIO_OUT_LINE_OUT;
      else if(!strcmp(argv[i],"-f"))
      {
        i++;
        outscale = atoi(argv[i]);
      }
      else if(!strcmp(argv[i],"-n"))
      {
        i++;
        numframes = atoi(argv[i]);
      }
#ifdef VARMODESUPPORT
      else if(!strcmp(argv[i],"-var")) {
        varmode = TRUE;
        audiobufsize = ((audiobufsize >> 1) + 63) & 0xffffc0;
      }
#endif
      else if(!strcmp(argv[i],"-q"))
        quiet = TRUE;
      else if(argv[i][0] == '-')
      {
        fprintf(stderr,"Unknown option: %s\n",argv[i]);
        usage();
      }
      else
      {
        if(fname[0])
          usage();
        strcpy(fname,argv[i]);
      }
    }

    if (!quiet)
      print_title();

    if(outmode==DECODE_AUDIO) {
      if(audio_open(&ai) < 0) {
        perror("audio");
        exit(1);
      }
    }

    make_decode_tables(outscale);
    if(strlen(fname))
      open_stream(fname);
    else
      open_stream(NULL);
  
    for(frameNum=0;read_frame(&fr) && numframes;frameNum++) 
    {
       stereo = fr.stereo;
       crc_error_count   = 0;
       total_error_count = 0;

       if(!frameNum)
       {
         if (!quiet)
           print_header(&fr);
         if(ai.rate == -1) {
           ai.rate = freqs[fr.sampling_frequency];
           audio_set_rate(&ai);
         }
       }
       if(frameNum < startFrame) {
         if(fr.lay == 3) {
           set_pointer(512);
         }
         continue;
       }
       numframes--;

       if (fr.error_protection)
          old_crc = getbits(16);

       switch(fr.lay)
       {
         case 1:
           clip = do_layer1(&fr,outmode,&ai);
           break;
         case 2:
           clip = do_layer2(&fr,outmode,&ai);
           break;
         case 3:
           clip = do_layer3(&fr,outmode,&ai);
           break;
       }
       if(verbose)
         fprintf(stderr, "{%4lu}\r",frameNum);
       if(clip > 0 && checkrange)
         fprintf(stderr,"%d samples clipped\n", clip);
    }
    audio_flush(outmode, &ai);

    close_stream();

    audio_close(&ai);

    if (!quiet)
      fprintf(stderr,"Decoding of \"%s\" is finished\n", fname);
    exit( 0 );
}

static void print_title()
{
    fprintf(stderr,"High Performance MPEG 1.0 Audio Player for Layer 1,2 and 3. ..\n");
    fprintf(stderr,"Version 0.55 (97/04/02). Written and copyrights by Michael Hipp.\n");
    fprintf(stderr,"Uses code from various people. See 'README' for more!\n");
}

static void usage(void)  /* print syntax & exit */
{
   print_title();
   fprintf(stderr,"usage: %s [-n <num>] [-f <scalefactor>] [-t|-s] [-c] [-v] inputBS\n", prgName);
   fprintf(stderr,"where:\n");
   fprintf(stderr," -t\t\ttestmode .. only decoding, no output.\n");
   fprintf(stderr," -s\t\twrite to stdout\n");
   fprintf(stderr," -c\t\tcheck for filter range violations.\n");     
   fprintf(stderr," -v\t\tverbose.\n");
   fprintf(stderr," -q\t\tquiet.\n");
   fprintf(stderr," -n <num>\tdecode only <num> frames.\n");
   fprintf(stderr," -skip <num>\tskip first <num> frames.\n");
   fprintf(stderr," -f <factor>\tChange scalefactor (default: 32768).\n");
   fprintf(stderr," -r <smplerate>\tSet samplerate (default: Automatic).\n");
   fprintf(stderr," -g <gain>\tSet audio hardware output gain.\n");
   fprintf(stderr," -os,-oh,-ol\tSet audio output line: speaker/headphone/line.\n");
   fprintf(stderr," -single0,-single1\tfor stereomodes: decode only channel No.0 or No.1.\n");
   fprintf(stderr," -singlemix\tonly Layer3: mix both channels\n");
   fprintf(stderr," inputBS  input bit stream of encoded audio\n");
   exit(1);
}


