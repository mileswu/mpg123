/* 
 * Mpeg Layer audio decoder V0.59g
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
#include <sys/time.h>
#include <sys/resource.h>

/* #define SET_PRIO */

#include "mpg123.h"
#include "getlopt.h"

#include "version.h"

static void usage(char *dummy);
static void print_title(void);

int outmode = DECODE_AUDIO;

char *listname = NULL;
long outscale  = 32768;
int checkrange = FALSE;
int tryresync  = FALSE;
int quiet      = FALSE;
int verbose    = 0;
int doublespeed= 0;
int halfspeed  = 0;
long numframes = -1;
long startFrame= 0;
int usebuffer  = 0;
int buffer_fd[2];
int buffer_pid;

static void catch_child(void)
{
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

static int intflag = FALSE;

static void catch_interrupt(void)
{
  intflag = TRUE;
}

static struct frame fr;
static struct audio_info_struct ai;
txfermem *buffermem;

#define FRAMEBUFUNIT (18 * 64 * 4)

void init_output(void)
{
  static int init_done = FALSE;

  if (init_done)
    return;
  init_done = TRUE;
  if (usebuffer) {
    unsigned int bufferbytes;
    if (usebuffer < 32)
      usebuffer = 32;
    bufferbytes = (usebuffer * 1024);
    bufferbytes -= bufferbytes % FRAMEBUFUNIT;
    xfermem_init (&buffermem, bufferbytes);
    pcm_sample = (short *) buffermem->data;
    pcm_point = 0;
    catchsignal (SIGCHLD, catch_child);
    switch ((buffer_pid = fork())) {
      case -1: /* error */
        perror("fork()");
        exit(1);
      case 0: /* child */
        xfermem_init_reader (buffermem);
        buffer_loop (&ai);
        xfermem_done_reader (buffermem);
        xfermem_done (buffermem);
        exit(0);
      default: /* parent */
        xfermem_init_writer (buffermem);
        outmode = DECODE_BUFFER;
    }
  }
  else {
    if (!(pcm_sample = (short *) malloc(audiobufsize * 2))) {
      perror ("malloc()");
      exit (1);
    }
  }

  if(outmode==DECODE_AUDIO) {
    if(audio_open(&ai) < 0) {
      perror("audio");
      exit(1);
    }
    audio_set_rate (&ai);
  }
}

char *get_next_file (int argc, char *argv[])
{
    static FILE *listfile = NULL;
    static char line[1024];

    if (listname || listfile) {
        if (!listfile) {
            if (!*listname || !strcmp(listname, "-")) {
                listfile = stdin;
                listname = NULL;
            }
            else if (!strncmp(listname, "http://", 7)) 
                listfile = http_open(listname);
            else if (!(listfile = fopen(listname, "rb"))) {
                perror (listname);
                exit (1);
            }
            if (verbose)
                fprintf (stderr, "Using playlist from %s ...\n",
                        listname ? listname : "standard input");
        }
        do {
            if (fgets(line, 1023, listfile)) {
                line[strcspn(line, "\t\n\r")] = '\0';
                if (line[0]=='\0' || line[0]=='#')
                    continue;
                return (line);
            }
            else {
                if (*listname)
                   fclose (listfile);
                listname = NULL;
                listfile = NULL;
            }
        } while (listfile);
    }
    if (loptind < argc)
    	return (argv[loptind++]);
    return (NULL);
}

void set_synth (char *arg)
{
    if (*arg == '2') {
        fr.synth = synth_2to1;
        fr.down_sample = 1;
    }
    else {
        fr.synth = synth_4to1;
        fr.down_sample = 2;
    }
}

#ifdef VARMODESUPPORT
void set_varmode (char *arg)
{
    audiobufsize = ((audiobufsize >> 1) + 63) & 0xffffc0;
}
#endif

void set_output (char *arg)
{
    switch (*arg) {
        case 'h': ai.output = AUDIO_OUT_HEADPHONES; break;
        case 's': ai.output = AUDIO_OUT_INTERNAL_SPEAKER; break;
        case 'l': ai.output = AUDIO_OUT_LINE_OUT; break;
        default:
            fprintf (stderr, "%s: Unknown argument \"%s\" to option \"%s\".\n",
                prgName, arg, loptarg);
            exit (1);
    }
}

void set_verbose (char *arg)
{
    verbose++;
}

topt opts[] = {
    {'k', "skip",        GLO_ARG | GLO_NUM,  0, &startFrame, 0},
    {'a', "audiodevice", GLO_ARG | GLO_CHAR, 0, &ai.device,  0},
    {'2', "2to1",        0,          set_synth, 0,           0},
    {'4', "4to1",        0,          set_synth, 0,           0},
    {'t', "test",        0,                  0, &outmode, DECODE_TEST},
    {'s', "stdout",      0,                  0, &outmode, DECODE_STDOUT},
    {'c', "check",       0,                  0, &checkrange, TRUE},
    {'v', "verbose",     0,        set_verbose, 0,           0},
    {'q', "quiet",       0,                  0, &quiet,      TRUE},
    {'y', "resync",      0,                  0, &tryresync,  TRUE},
    {'0', "single0",     0,                  0, &fr.single,  0},
    {0,   "left",        0,                  0, &fr.single,  0},
    {'1', "single1",     0,                  0, &fr.single,  1},
    {0,   "right",       0,                  0, &fr.single,  1},
    {'m', "singlemix",   0,                  0, &fr.single,  3},
    {0,   "mix",         0,                  0, &fr.single,  3},
    {'g', "gain",        GLO_ARG | GLO_NUM,  0, &ai.gain,    0},
    {'r', "rate",        GLO_ARG | GLO_NUM,  0, &ai.rate,    0},
    {0,   "headphones",  0,                  0, &ai.output, AUDIO_OUT_HEADPHONES},
    {0,   "speaker",     0,                  0, &ai.output, AUDIO_OUT_INTERNAL_SPEAKER},
    {0,   "lineout",     0,                  0, &ai.output, AUDIO_OUT_LINE_OUT},
    {'o', "output",      GLO_ARG | GLO_CHAR, set_output, 0,  0},
    {'f', "scale",       GLO_ARG | GLO_NUM,  0, &outscale,   0},
    {'n', "frames",      GLO_ARG | GLO_NUM,  0, &numframes,  0},
#ifdef VARMODESUPPORT
    {'v', "var",         0,        set_varmode, &varmode,    TRUE},
#endif
    {'b', "buffer",      GLO_ARG | GLO_NUM,  0, &usebuffer,  0},
    {'d', "doublespeed", GLO_ARG | GLO_NUM,  0, &doublespeed,0},
    {'h', "halfspeed",   GLO_ARG | GLO_NUM,  0, &halfspeed,  0},
    {'p', "proxy",       GLO_ARG | GLO_CHAR, 0, &proxyurl,   0},
    {'@', "list",        GLO_ARG | GLO_CHAR, 0, &listname,   0},
    {'?', "help",        0,              usage, 0,           0}
};

int main(int argc, char *argv[])
{
    int result, stereo, clip=0;
    int crc_error_count, total_error_count;
    unsigned int old_crc;
    unsigned long frameNum = 0;
    char *fname;
    struct timeval start_time, now;
    unsigned long secdiff;

#ifdef SET_PRIO
	int mypid = getpid();
	setpriority(PRIO_PROCESS,mypid,-20);
#endif

    fr.single = -1; /* both channels */
    fr.synth = synth_1to1;
    fr.down_sample = 0;

    ai.gain = ai.rate = ai.output = -1;
    ai.device = NULL;
    ai.channels = 2;

    (prgName = strrchr(argv[0], '/')) ? prgName++ : (prgName = argv[0]);

    while ((result = getlopt(argc, argv, opts)))
      switch (result) {
        case GLO_UNKNOWN:
          fprintf (stderr, "%s: Unknown option \"%s\".\n", prgName, loptarg);
          exit (1);
        case GLO_NOARG:
          fprintf (stderr, "%s: Missing argument for option \"%s\".\n",
              prgName, loptarg);
          exit (1);
      }
    if (loptind >= argc && !listname)
      usage(NULL);

    if (!quiet)
      print_title();

    make_decode_tables(outscale);
    init_layer2(); /* inits also shared tables with layer1 */
    init_layer3(fr.down_sample);
    catchsignal (SIGINT, catch_interrupt);

    while ((fname = get_next_file(argc, argv))) {
      if(!*fname || !strcmp(fname, "-"))
        fname = NULL;
      open_stream(fname);
      if (!quiet)
        fprintf(stderr, "\nPlaying MPEG stream from %s ...\n",
                fname ? fname : "standard input");

      gettimeofday (&start_time, NULL);
      read_frame_init();
      for(frameNum=0;read_frame(&fr) && numframes && !intflag;frameNum++) 
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
           init_output();
         }
         if(frameNum < startFrame || (doublespeed && (frameNum % doublespeed))) {
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
           case 3:
             clip = do_layer3(&fr,outmode,&ai);
             break;
           case 2:
             clip = do_layer2(&fr,outmode,&ai);
             break;
           case 1:
             clip = do_layer1(&fr,outmode,&ai);
             break;
         }
         if (usebuffer) {
           if (!intflag) {
             buffermem->freeindex =
                   (buffermem->freeindex + pcm_point * 2) % buffermem->size;
             if (buffermem->wakeme[XF_READER])
               xfermem_putcmd(buffermem->fd[XF_WRITER], XF_CMD_WAKEUP);
           }
           pcm_sample = (short *) (buffermem->data + buffermem->freeindex);
           pcm_point = 0;
           while (xfermem_get_freespace(buffermem) < FRAMEBUFUNIT)
             if (xfermem_block(XF_WRITER, buffermem) == XF_CMD_TERMINATE) {
               intflag = TRUE;
               break;
             }
           if (intflag)
             break;
         }

         if(verbose) {
           if (verbose > 1 || !(frameNum & 0xf))
             fprintf(stderr, "\r{%4lu} ",frameNum);
           if (verbose > 1 && usebuffer)
             fprintf (stderr, "%7d ", xfermem_get_usedspace(buffermem));
         }
         if(clip > 0 && checkrange)
           fprintf(stderr,"%d samples clipped\n", clip);
      }
      close_stream(fname);
      if (!quiet) {
        int sfreq = freqs[fr.sampling_frequency];
        int secs = (frameNum * ((fr.lay == 1) ? 384 : 1152) + (sfreq / 2))
                   / sfreq;
        fprintf(stderr,"[%d:%02d] Decoding of %s finished.\n", secs / 60,
                secs % 60, fname ? fname : "standard input");
      }
      if (intflag) {
        gettimeofday (&now, NULL);
        secdiff = (now.tv_sec - start_time.tv_sec) * 1000;
        if (now.tv_usec >= start_time.tv_usec)
          secdiff += (now.tv_usec - start_time.tv_usec) / 1000;
        else
          secdiff -= (start_time.tv_usec - now.tv_usec) / 1000;
        if (secdiff < 1000)
          break;
        intflag = FALSE;
      }
    }

    if (usebuffer) {
      xfermem_done_writer (buffermem);
      waitpid (buffer_pid, NULL, 0);
      xfermem_done (buffermem);
    }
    else {
      audio_flush(outmode, &ai);
      free (pcm_sample);
    }

    if(outmode==DECODE_AUDIO)
      audio_close(&ai);
    exit( 0 );
}

static void print_title(void)
{
    fprintf(stderr,"High Performance MPEG 1.0/2.0 Audio Player for Layer 1, 2 and 3.\n");
    fprintf(stderr,"Version %s (%s). Written and copyrights by Michael Hipp.\n", prgVersion, prgDate);
    fprintf(stderr,"Uses code from various people. See 'README' for more!\n");
    fprintf(stderr,"THIS SOFTWARE COMES WITH ABSOLUTELY NO WARRANTY! USE AT YOUR OWN RISK!\n");
}

static void usage(char *dummy)  /* print syntax & exit */
{
   print_title();
   fprintf(stderr,"\nusage: %s [option(s)] [file(s) | URL(s) | -]\n", prgName);
   fprintf(stderr,"supported options [defaults in brackets]:\n");
   fprintf(stderr,"   -v    increase verbosity level       -q    quiet (don't print title)\n");
   fprintf(stderr,"   -t    testmode (no output)           -s    write to stdout\n");
   fprintf(stderr,"   -k n  skip first n frames [0]        -n n  decode only n frames [all]\n");
   fprintf(stderr,"   -c    check range violations         -y    try to resync on errors\n");
   fprintf(stderr,"   -b n  output buffer: n Kbytes [0]    -f n  change scalefactor [32768]\n");
   fprintf(stderr,"   -r n  override samplerate [auto]     -g n  set audio hardware output gain\n");
   fprintf(stderr,"   -os   output to built-in speaker     -oh   output to headphones\n");
   fprintf(stderr,"   -ol   output to line-out connector   -a d  set audio device\n");
   fprintf(stderr,"   -2    downsample 1:2 (22 kHz)        -4    downsample 1:4 (11 kHz)\n");
   fprintf(stderr,"   -d n  play every n'th frame only     -h n  play every frame n times\n");
   fprintf(stderr,"   -0    decode channel 0 (left) only   -1    decode channel 1 (right) only\n");
   fprintf(stderr,"   -m    mix both channels (mono)       -p p  use HTTP proxy p [$HTTP_PROXY]\n");
   fprintf(stderr,"   -@ f  read filenames/URLs from f\n");
   fprintf(stderr,"See the manpage %s(1) for more information.\n", prgName);
   exit(1);
}


