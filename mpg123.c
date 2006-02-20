/* 
 * Mpeg Layer audio decoder V0.57
 * -------------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp, All rights reserved.
 * See also 'README' !
 *
 * used sources:
 *   mpegaudio package
 */

#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

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
int tryresync  = FALSE;
int verbose    = FALSE;
int quiet      = FALSE;
long numframes = -1;
long startFrame = 0;
int usebuffer  = 0;
int buffer_fd[2];
int buffer_pid;

#ifndef MAXNAMLEN
#define MAXNAMLEN	256
#endif

void catch_child()
{
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

void init_output(struct audio_info_struct *ai)
{
  if (usebuffer) {
    if (pipe(buffer_fd)) {
      perror ("pipe()");
      exit (1);
    }
    catchsignal (SIGCHLD, catch_child);
    switch ((buffer_pid = fork())) {
      case -1: /* error */
        perror("fork()");
        exit(1);
      case 0: /* child */
        close (buffer_fd[1]);
        buffer_loop(ai);
        close (buffer_fd[0]);
        exit(0);
      default: /* parent */
        close (buffer_fd[0]);
        outmode = DECODE_BUFFER;
    }
  }

  if(outmode==DECODE_AUDIO) {
    if(audio_open(ai) < 0) {
      perror("audio");
      exit(1);
    }
  }
}

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
    fr.synth = synth_1to1;

    ai.gain = ai.rate = ai.output = -1;
    ai.device = NULL;
    ai.channels = 2;

    if (!(prgName = strrchr(argv[0], '/')))
      prgName = argv[0];
    (prgName = strrchr(argv[0], '/')) ? prgName++ : (prgName = argv[0]);

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
      else if(!strcmp(argv[i],"-2to1"))
        fr.synth = synth_2to1;
      else if(!strcmp(argv[i],"-4to1"))
        fr.synth = synth_4to1;
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
      else if(!strcmp(argv[i],"-rs"))
        tryresync = TRUE;
      else if(!strcmp(argv[i],"-b"))
      {
        i++;
        usebuffer = atoi(argv[i]);
      }
      else if(!strcmp(argv[i],"-"))
        fname[0] = '\0';   /* read from stdin */
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
#ifndef AUDIO_USES_FD
    if (usebuffer && outmode == DECODE_AUDIO) {
      fprintf(stderr,"Buffering not supported for this kind of audio hardware.\n");
      exit(1);
    }
#endif

    if (!quiet)
      print_title();

    make_decode_tables(outscale);
    init_layer2(); /* inits also shared tables with layer1 */
    init_layer3();

    if(fname[0])
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
         }
         init_output(&ai);
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
         fprintf(stderr, "\r{%4lu} ",frameNum);
       if(clip > 0 && checkrange)
         fprintf(stderr,"%d samples clipped\n", clip);
    }
    audio_flush(outmode, &ai);

    close_stream();

    if (usebuffer) {
      close (buffer_fd[1]);
      waitpid (buffer_pid, NULL, 0);
    }

    if(outmode==DECODE_AUDIO)
      audio_close(&ai);

    if (!quiet)
      fprintf(stderr,"Decoding of \"%s\" is finished\n", fname);
    exit( 0 );
}

static void print_title()
{
    fprintf(stderr,"High Performance MPEG 1.0 Audio Player for Layer 1,2 and 3. ..\n");
    fprintf(stderr,"Version 0.57 (97/04/06). Written and copyrights by Michael Hipp.\n");
    fprintf(stderr,"Uses code from various people. See 'README' for more!\n");
}

static void usage(void)  /* print syntax & exit */
{
   print_title();
   fprintf(stderr,"usage: %s [option(s)] inputBS\n", prgName);
   fprintf(stderr,"supported options:\n");
   fprintf(stderr," -t\t\ttestmode .. only decoding, no output.\n");
   fprintf(stderr," -s\t\twrite to stdout\n");
   fprintf(stderr," -c\t\tcheck for filter range violations.\n");     
   fprintf(stderr," -v\t\tverbose.\n");
   fprintf(stderr," -q\t\tquiet.\n");
   fprintf(stderr," -n <num>\tdecode only <num> frames.\n");
   fprintf(stderr," -skip <num>\tskip first <num> frames.\n");
   fprintf(stderr," -rs\t\ttry to resync and continue on errors.\n");
   fprintf(stderr," -b <size>\tUse an output buffer of <size> Kbytes (default: 0 [none])\n");
   fprintf(stderr," -f <factor>\tchange scalefactor (default: 32768).\n");
   fprintf(stderr," -r <smplerate>\tset samplerate (default: Automatic).\n");
   fprintf(stderr," -g <gain>\tset audio hardware output gain.\n");
   fprintf(stderr," -os,-oh,-ol\tset audio output line: speaker/headphone/line.\n");
   fprintf(stderr," -single0,-single1\tfor stereomodes: decode only channel No.0 or No.1.\n");
   fprintf(stderr," -singlemix\tonly Layer3: mix both channels\n");
   fprintf(stderr," inputBS  input bit stream of encoded audio\n");
   fprintf(stderr,"See the manpage %s(1) for more information.\n", prgName);
   exit(1);
}


