#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include "mpg123.h"
#include "tables.h"

/* max = 1728 */
#define MAXFRAMESIZE 1792

static int tabsel_123[3][16] = 
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} };

long freqs[4] = { 44100, 48000, 32000, 999999 };

#ifdef I386_ASSEM
int  bitindex;
unsigned char *wordpointer;
#else
static int  bitindex;
static unsigned char *wordpointer;
#endif

static int fsize=0,fsizeold=0,ssize;
static unsigned char bsspace[2][MAXFRAMESIZE+512]; /* MAXFRAMESIZE */
static unsigned char *bsbuf=bsspace[1],*bsbufold;
static int bsnum=0;

struct ibuf {
	struct ibuf *next;
	struct ibuf *prev;
	unsigned char *buf;
	unsigned char *pnt;
	int len;
	/* skip,time stamp */
};

struct ibuf ibufs[2];
struct ibuf *cibuf;
int ibufnum=0;

short pcm_sample[AUDIOBUFSIZE];
int pcm_point = 0;
int audiobufsize = AUDIOBUFSIZE;

#ifdef VARMODESUPPORT
	/*
	 *   This is a dirty hack!  It might burn your PC and kill your cat!
	 *   When in "varmode", specially formatted layer-3 mpeg files are
	 *   expected as input -- it will NOT work with standard mpeg files.
	 *   The reason for this:
	 *   Varmode mpeg files enable my own GUI player to perform fast
	 *   forward and backward functions, and to jump to an arbitrary
	 *   timestamp position within the file.  This would be possible
	 *   with standard mpeg files, too, but it would be a lot harder to
	 *   implement.
	 *   A filter for converting standard mpeg to varmode mpeg is
	 *   available on request, but it's really not useful on its own.
	 *
	 *   Oliver Fromme  <oliver.fromme@heim3.tu-clausthal.de>
	 *   Mon Mar 24 00:04:24 MET 1997
	 */
int varmode = FALSE;
int playlimit;
#endif

static FILE *filept;

static void get_II_stuff(struct frame *fr)
{
  static int translate[3][2][16] = 
   { { { 0,2,2,2,2,2,2,0,0,0,1,1,1,1,1,0 } ,
       { 0,2,2,0,0,0,1,1,1,1,1,1,1,1,1,0 } } ,
     { { 0,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0 } ,
       { 0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0 } } ,
     { { 0,3,3,3,3,3,3,0,0,0,1,1,1,1,1,0 } ,
       { 0,3,3,0,0,0,1,1,1,1,1,1,1,1,1,0 } } };

  int table,sblim;
  static struct al_table *tables[4] = { alloc_0, alloc_1, alloc_2, alloc_3 };
  static int sblims[4] = { 27 , 30 , 8, 12 };

  table = translate[fr->sampling_frequency][2-fr->stereo][fr->bitrate_index];
  sblim = sblims[table];

  fr->alloc = tables[table];
  fr->II_sblimit = sblim;
}

void audio_flush(int outmode, struct audio_info_struct *ai)
{
  if (pcm_point) {
    switch (outmode) {
      case DECODE_STDOUT:
        write (1, pcm_sample, pcm_point * 2);
        break;
      case DECODE_AUDIO:
        audio_play_samples (ai, pcm_sample, pcm_point);
        break;
      case DECODE_BUFFER:
        write (buffer_fd[1], pcm_sample, pcm_point * 2);
        break;
    }
    pcm_point = 0;
  }
}

void (*catchsignal(int signum, void(*handler)()))()
{
  struct sigaction new_sa;
  struct sigaction old_sa;

  new_sa.sa_handler = handler;
  sigemptyset(&new_sa.sa_mask);
  new_sa.sa_flags = 0;
  if (sigaction(signum, &new_sa, &old_sa) == -1)
    return ((void (*)()) -1);
  return (old_sa.sa_handler);
}

#define HDRCMPMASK 0xfffffddf

int read_frame(struct frame *fr)
{
  static long fs[3][16] = {
    { 0 , 104, 156, 182, 208, 261, 313, 365, 417, 522, 626, 731, 835, 1044, 1253, },
    { 0 , 96 , 144, 168, 192, 240, 288, 336, 384, 480, 576, 672, 768, 960 , 1152, },
    { 0 , 144, 216, 252, 288, 360, 432, 504, 576, 720, 864, 1008, 1152, 1440, 1728 } };

/*
  static int jsb_table[3][4] =  { { 4, 8, 12, 16 }, { 4, 8, 12, 16}, { 0, 4, 8, 16} };
*/

  static unsigned long oldhead=0,newhead;
  static unsigned long firsthead=0;

  static unsigned char ssave[34];
  unsigned char hbuf[8];
  static int framesize;
  static int halfphase = 0;
  int l;
  int try = 0;

  if (halfspeed)
    if (halfphase--) {
      bitindex = 0;
      wordpointer = (unsigned char *) bsbuf;
      if (fr->lay == 3)
        memcpy (bsbuf, ssave, ssize);
      return 1;
    }
    else
      halfphase = halfspeed - 1;

#ifdef VARMODESUPPORT
  if (varmode) {
    if(fread(hbuf,1,8,filept) != 8)
      return 0;
  }
  else
#endif
    if(fread(hbuf,1,4,filept) != 4)
      return 0;

  newhead = ((unsigned long) hbuf[0] << 24) | ((unsigned long) hbuf[1] << 16) |
            ((unsigned long) hbuf[2] << 8) | (unsigned long) hbuf[3];

  if(oldhead != newhead || !oldhead)
  {
#if 0
    fprintf(stderr,"Major headerchange %08lx->%08lx.\n",oldhead,newhead);
#endif

    if( (newhead & 0xfff80000) != 0xfff80000)
    {
      if((newhead & 0xfff80000) == 0xfff00000)
        fprintf(stderr,"MPEG 2.0 Audio not supported!\n");
      else if((newhead & 0xfff80000) == 0xffe00000)
        fprintf(stderr,"MPEG '2.5' Audio not supported!\n");
      else
        fprintf(stderr,"Illegal Audio-MPEG-Header 0x%08lx at offset 0x%lx.\n",
                newhead,ftell(filept)-4);
      if (tryresync && oldhead) {
            /* Read more bytes until we find something that looks
               reasonably like a valid header.  This is not a
               perfect strategy, but it should get us back on the
               track within a short time (and hopefully without
               too much distortion in the audio output).  */
        do {
          try++;
          memmove (&hbuf[0], &hbuf[1], 7);
#ifdef VARMODESUPPORT
          if (fread(&hbuf[varmode?7:3],1,1,filept) != 1)
#else
          if (fread(&hbuf[3],1,1,filept) != 1)
#endif
            return 0;
          newhead = ((unsigned long) hbuf[0] << 24) | ((unsigned long) hbuf[1] << 16) |
                    ((unsigned long) hbuf[2] << 8) | (unsigned long) hbuf[3];
        } while ((newhead & HDRCMPMASK) != (oldhead & HDRCMPMASK)
              && (newhead & HDRCMPMASK) != (firsthead & HDRCMPMASK));
        if (!quiet)
          fprintf (stderr, "Skipped %d bytes in input.\n", try);
      }
      else
        exit(1);
    }
    if (!firsthead)
      firsthead = newhead;

    fr->version = 1;
    if (!tryresync || !oldhead) {
          /* If "tryresync" is true, assume that certain
             parameters do not change within the stream! */
      fr->lay = 4-((newhead>>17)&3);
      fr->bitrate_index = ((newhead>>12)&0xf);
      fr->sampling_frequency = ((newhead>>10)&0x3);
      fr->error_protection = ((newhead>>16)&0x1)^0x1;
    }
    fr->padding = ((newhead>>9)&0x1);
    fr->extension = ((newhead>>8)&0x1);
    fr->copyright = ((newhead>>3)&0x1);
    fr->original = ((newhead>>2)&0x1);
    fr->emphasis = newhead & 0x3;
    fr->mode = ((newhead>>6)&0x3);
    fr->mode_ext = ((newhead>>4)&0x3);
    fr->padding = ((newhead>>9)&0x1);
    fr->stereo = (fr->mode == MPG_MD_MONO) ? 1 : 2;

    oldhead = newhead;

    if(!fr->bitrate_index)
    {
      fprintf(stderr,"Free format not supported.\n");
      exit(1);
    }

    switch(fr->lay)
    {
      case 1:
#ifdef VARMODESUPPORT
        if (varmode) {
          fprintf(stderr,"Sorry, layer-1 not supported in varmode.\n"); 
          exit(1);
        }
#endif
        fr->jsbound = (fr->mode == MPG_MD_JOINT_STEREO) ? 
                         (fr->mode_ext<<2)+4 : 32;
        framesize  = (long) tabsel_123[0][fr->bitrate_index] * 12000;
        framesize /= freqs[fr->sampling_frequency];
        framesize  = ((framesize+fr->padding)<<2)-4;
        break;
      case 2:
#ifdef VARMODESUPPORT
        if (varmode) {
          fprintf(stderr,"Sorry, layer-2 not supported in varmode.\n"); 
          exit(1);
        }
#endif
        get_II_stuff(fr);
        fr->jsbound = (fr->mode == MPG_MD_JOINT_STEREO) ?
                         (fr->mode_ext<<2)+4 : fr->II_sblimit;
        framesize = fs[fr->sampling_frequency][fr->bitrate_index]-4;
        framesize += fr->padding;
        break;
      case 3:
        ssize = (fr->stereo == 1) ? 17 : 32;
        if(fr->error_protection)
          ssize += 2;
#ifdef VARMODESUPPORT
        if (varmode)
          playlimit = ((unsigned int) hbuf[6] << 8) | (unsigned int) hbuf[7];
          framesize = ssize + 
                      (((unsigned int) hbuf[4] << 8) | (unsigned int) hbuf[5]);
        else {
#endif
          framesize  = (long) tabsel_123[2][fr->bitrate_index] * 144000;
          framesize /= freqs[fr->sampling_frequency];
          framesize = framesize + fr->padding - 4;
#ifdef VARMODESUPPORT
        }
#endif
        break; 
      default:
        fprintf(stderr,"Sorry, unknow layer type.\n"); 
        exit(1);
    }
  }

  fsizeold=fsize;	/* for Layer3 */
  bsbufold = bsbuf;	
  bsbuf = bsspace[bsnum]+512;
  bsnum = (bsnum + 1) & 1;

  fsize = framesize;
 
  if( (l=fread(bsbuf,1,fsize,filept)) != fsize)
  {
    if(l <= 0)
      return 0;
    memset(bsbuf+l,0,fsize-l);
  }

  if (halfspeed && fr->lay == 3)
    memcpy (ssave, bsbuf, ssize);

  bitindex = 0;
  wordpointer = (unsigned char *) bsbuf;

  return 1;
}

void print_header(struct frame *fr)
{
	static char *modes[4] = { "Stereo", "Joint-Stereo", "Dual-Channel", "Single-Channel" };
	static char *layers[4] = { "Unknown" , "I", "II", "III" };
 
	fprintf(stderr,"Layer: %s, Freq: %ld, mode: %s, modext: %d, BPF: %d\n",
		layers[fr->lay],freqs[fr->sampling_frequency],
		modes[fr->mode],fr->mode_ext,fsize+4);
	fprintf(stderr,"chan: %d, copyright: %s, original: %s, CRC: %s, emphasis: %d.\n",
		fr->stereo,fr->copyright?"Yes":"No",
		fr->original?"Yes":"No",fr->error_protection?"Yes":"No",
		fr->emphasis);
	fprintf(stderr,"Bitrate: %d Kbits/s, Extension value: %d\n",
		tabsel_123[fr->lay-1][fr->bitrate_index],fr->extension);
}

/* open the device to read the bit stream from it */

void open_stream(char *bs_filenam)
{
    if (!bs_filenam)
        filept = stdin;
    else if (!strncmp(bs_filenam, "http://", 7)) 
        filept = http_open(bs_filenam);
    else if (!(filept = fopen(bs_filenam, "rb"))) {
        perror (bs_filenam);
        exit(1);
    }
}

/*close the device containing the bit stream after a read process*/

void close_stream(char *bs_filenam)
{
    if (bs_filenam)
        fclose(filept);
}

#ifndef I386_ASSEM
#ifdef _gcc_
inline 
#endif
unsigned int getbits(int number_of_bits)
{
  unsigned long rval;

  if(!number_of_bits)
    return 0;

  {
    rval = wordpointer[0];
    rval <<= 8;
    rval |= wordpointer[1];
    rval <<= 8;
    rval |= wordpointer[2];
#if 0
    rval = ((unsigned int) wordpointer[0] << 16)+((unsigned int) wordpointer[1]<<8)+
                 (unsigned int) wordpointer[2];
#endif
    rval <<= bitindex;
    rval &= 0xffffff;

    bitindex += number_of_bits;

    rval >>= (24-number_of_bits);

    wordpointer += (bitindex>>3);
    bitindex &= 7;
  }

  return rval;
}

#ifdef _gcc_
inline
#endif
unsigned int getbits_fast(int number_of_bits)
{
  unsigned long rval;

  {
    rval = wordpointer[0];
    rval <<= 8;	
    rval |= wordpointer[1];
    rval <<= bitindex;
    rval &= 0xffff;
#if 0
    rval = ((unsigned int) high << (8-bitindex) )+((unsigned int) (unsigned char) wordpointer[1]);
#endif
    bitindex += number_of_bits;

    rval >>= (16-number_of_bits);

    wordpointer += (bitindex>>3);
    bitindex &= 7;
  }

  return rval;
}

#ifdef _gcc_
inline 
#endif
unsigned int get1bit(void)
{
  unsigned char rval;

  rval = *wordpointer << bitindex;

  bitindex++;
  wordpointer += (bitindex>>3);
  bitindex &= 7;

  return rval>>7;
}
#endif

void set_pointer(long backstep)
{
  wordpointer = bsbuf + ssize - backstep;
  if (backstep)
    memcpy(wordpointer,bsbufold+fsizeold-backstep,backstep);
  bitindex = 0; 
}


