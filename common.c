#include <ctype.h>
#include <stdlib.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef READ_MMAP
#include <sys/mman.h>
#ifndef MAP_FAILED
#define MAP_FAILED ( (void *) -1 )
#endif
#endif

#include "mpg123.h"
#include "tables.h"

/* max = 1728 */
#define MAXFRAMESIZE 1792

#define SKIP_JUNK 1

int tabsel_123[2][3][16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,} }
};

long freqs[9] = { 44100, 48000, 32000, 22050, 24000, 16000 , 11025 , 12000 , 8000 };

#ifdef I386_ASSEM
int bitindex;
unsigned char *wordpointer;
#else
static int bitindex;
static unsigned char *wordpointer;
#endif

static int fsizeold=0,ssize;
static unsigned char bsspace[2][MAXFRAMESIZE+512]; /* MAXFRAMESIZE */
static unsigned char *bsbuf=bsspace[1],*bsbufold;
static int bsnum=0;

static unsigned long oldhead = 0;
static unsigned long firsthead=0;

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

unsigned char *pcm_sample;
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

static int filept;
static int filept_opened;

static int decode_header(struct frame *fr,unsigned long newhead);
static void print_id3_tag(char *buf);

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
  static struct al_table *tables[5] = 
       { alloc_0, alloc_1, alloc_2, alloc_3 , alloc_4 };
  static int sblims[5] = { 27 , 30 , 8, 12 , 30 };

  if(fr->lsf)
    table = 4;
  else
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
        write (1, pcm_sample, pcm_point);
        break;
      case DECODE_AUDIO:
        audio_play_samples (ai, pcm_sample, pcm_point);
        break;
      case DECODE_BUFFER:
        write (buffer_fd[1], pcm_sample, pcm_point);
        break;
    }
    pcm_point = 0;
  }
}

#ifndef WIN32
void (*catchsignal(int signum, void(*handler)()))()
{
  struct sigaction new_sa;
  struct sigaction old_sa;

#ifdef DONT_CATCH_SIGNALS
  printf ("Not catching any signals.\n");
  return ((void (*)()) -1);
#endif

  new_sa.sa_handler = handler;
  sigemptyset(&new_sa.sa_mask);
  new_sa.sa_flags = 0;
  if (sigaction(signum, &new_sa, &old_sa) == -1)
    return ((void (*)()) -1);
  return (old_sa.sa_handler);
}
#endif

#define HDRCMPMASK 0xfffffd00

/*******************************************************************
 * stream based operation
 */

void stream_close(void)
{
    if (filept_opened)
        close(filept);
}

/**************************************** 
 * HACK,HACK,HACK: step back <num> frames 
 * can only work if the 'stream' isn't a real stream but a file
 */
static int stream_back_frame(struct frame *fr,int num)
{
	long bytes;
	unsigned char buf[4];
	unsigned long newhead;

	if(!firsthead)
		return 0;

	bytes = (fr->framesize+8)*(num+2);

	if(lseek(filept,-bytes,SEEK_CUR) < 0)
		return -1;

	if(read(filept,buf,4) != 4)
		return -1;

	newhead = (buf[0]<<24) + (buf[1]<<16) + (buf[2]<<8) + buf[3];
	
	while( (newhead & HDRCMPMASK) != (firsthead & HDRCMPMASK) ) {
		if(read(filept,buf,1) != 1)
			return -1;
		newhead <<= 8;
		newhead |= buf[0];
		newhead &= 0xffffffff;
	}

	if( lseek(filept,-4,SEEK_CUR) < 0)
		return -1;
	
	read_frame(fr);
	read_frame(fr);

	if(fr->lay == 3) {
		set_pointer(512);
	}

	return 0;
}

static int stream_head_read(unsigned char *hbuf,unsigned long *newhead)
{
#ifdef VARMODESUPPORT
	if(varmode) {
		if(read(filept,hbuf,8) != 8)
			return FALSE;
	}
	else
#else
	if(read(filept,hbuf,4) != 4)
		return FALSE;
#endif

	*newhead = ((unsigned long) hbuf[0] << 24) |
	           ((unsigned long) hbuf[1] << 16) |
	           ((unsigned long) hbuf[2] << 8)  |
	            (unsigned long) hbuf[3];

	return TRUE;
}

static int stream_head_shift(unsigned char *hbuf,unsigned long *head)
{
  memmove (&hbuf[0], &hbuf[1], 3);
  if(read(filept,hbuf+3,1) != 1)
    return 0;
  *head <<= 8;
  *head |= hbuf[3];
  *head &= 0xffffffff;
  return 1;
}

static int stream_skip_bytes(int len)
{
  return lseek(filept,len,SEEK_CUR);
}

static int stream_read_frame_body(int size)
{
  long l;

  if( (l=read(filept,bsbuf,size)) != size)
  {
    if(l <= 0)
      return 0;
    memset(bsbuf+l,0,size-l);
  }

  bitindex = 0;
  wordpointer = (unsigned char *) bsbuf;

  return 1;
}

static long stream_tell(void)
{
  return lseek(filept,0,SEEK_CUR);
}


#ifdef READ_MMAP
/*********************************************************+
 * memory mapped operation 
 *
 */
static unsigned char *mapbuf;
static unsigned char *mappnt;
static unsigned char *mapend;

static int mapped_init(struct reader *rd_struct) 
{
	long len;
	char buf[128];

        if((len=lseek(filept,0,SEEK_END)) < 0) {
                return -1;
        }
	lseek(filept,-128,SEEK_END);
	if(read(filept,buf,128) != 128) {
		return -1;
	}
	if(!strncmp(buf,"TAG",3)) {
		print_id3_tag(buf);
		len -= 128;
	}
	lseek(filept,0,SEEK_SET);
	if(len <= 0)
		return -1;

        mappnt = mapbuf = 
		mmap(NULL, len, PROT_READ, MAP_SHARED , filept, 0);
	if(!mapbuf || mapbuf == MAP_FAILED)
		return -1;

	mapend = mapbuf + len;

fprintf(stderr,"Using memory mapped IO\n");

	return 0;
}

static void mapped_close(void)
{
	munmap(mapbuf,mapend-mapbuf);
	if (filept_opened)
		close(filept);
}

static int mapped_head_read(unsigned char *hbuf,unsigned long *newhead) 
{
	unsigned long nh;

	if(mappnt + 4 > mapend)
		return FALSE;

	nh = (*hbuf++ = *mappnt++)  << 24;
	nh |= (*hbuf++ = *mappnt++) << 16;
	nh |= (*hbuf++ = *mappnt++) << 8;
	nh |= (*hbuf++ = *mappnt++) ;

	*newhead = nh;
	return TRUE;
}

static int mapped_head_shift(unsigned char *hbuf,unsigned long *head)
{
  if(mappnt + 1 > mapend)
    return FALSE;
  memmove (&hbuf[0], &hbuf[1], 3);
  hbuf[3] = *mappnt++;
  *head <<= 8;
  *head |= hbuf[3];
  *head &= 0xffffffff;
  return TRUE;
}

static int mapped_skip_bytes(int len)
{
  if(mappnt + len > mapend)
    return FALSE;
  mappnt += len;
  return TRUE;
}

static int mapped_read_frame_body(int size)
{

#if 1
  if(mappnt + size > mapend)
    return FALSE;
#else
  long l;
  if(size > (mapend-mappnt)) {
    l = mapend-mappnt;
    memcpy(bsbuf,mappnt,l);
    memset(bsbuf+l,0,size-l);
  }
  else
#endif
    memcpy(bsbuf,mappnt,size);

  mappnt += size;
  bitindex = 0;
  wordpointer = (unsigned char *) bsbuf;

  return TRUE;
}

static int mapped_back_frame(struct frame *fr,int num)
{
    long bytes;
    unsigned long newhead;

    if(!firsthead)
        return 0;

    bytes = (fr->framesize+8)*(num+2);

    if( (mappnt - bytes) < mapbuf || (mappnt - bytes + 4) > mapend)
        return -1;
    mappnt -= bytes;

    newhead = (mappnt[0]<<24) + (mappnt[1]<<16) + (mappnt[2]<<8) + mappnt[3];
    mappnt += 4;

    while( (newhead & HDRCMPMASK) != (firsthead & HDRCMPMASK) ) {
        if(mappnt + 1 > mapend)
            return -1;
        newhead <<= 8;
        newhead |= *mappnt++;
        newhead &= 0xffffffff;
    }
    mappnt -= 4;

    read_frame(fr);
    read_frame(fr);

    if(fr->lay == 3)
        set_pointer(512);

    return 0;
}

static long mapped_tell(void)
{
  return mappnt - mapbuf;
}

#endif

/*****************************************************************
 * read frame helper
 */

struct reader *rd;
struct reader readers[] = {
#ifdef READ_SYSTEM
 { system_init,
   NULL,	/* filled in by system_init() */
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL } ,
#endif
#ifdef READ_MMAP
 { mapped_init,
   mapped_close,
   mapped_head_read,
   mapped_head_shift,
   mapped_skip_bytes,
   mapped_read_frame_body,
   mapped_back_frame,
   mapped_tell } ,
#endif
 { NULL,
   stream_close,
   stream_head_read,
   stream_head_shift,
   stream_skip_bytes,
   stream_read_frame_body,
   stream_back_frame,
   stream_tell }
};


void read_frame_init (void)
{
    oldhead = 0;
    firsthead = 0;
}

int head_check(unsigned long head)
{
    if( (head & 0xffe00000) != 0xffe00000)
	return FALSE;
    if(!((head>>17)&3))
	return FALSE;
    if( ((head>>12)&0xf) == 0xf)
	return FALSE;
    if( ((head>>10)&0x3) == 0x3 )
	return FALSE;
    return TRUE;
}



/*****************************************************************
 * read next frame
 */
int read_frame(struct frame *fr)
{
  unsigned long newhead;
  static unsigned char ssave[34];
  unsigned char hbuf[8];

  fsizeold=fr->framesize;       /* for Layer3 */

  if (halfspeed) {
    static int halfphase = 0;
    if (halfphase--) {
      bitindex = 0;
      wordpointer = (unsigned char *) bsbuf;
      if (fr->lay == 3)
        memcpy (bsbuf, ssave, ssize);
      return 1;
    }
    else
      halfphase = halfspeed - 1;
  }

read_again:
  if(!rd->head_read(hbuf,&newhead))
    return FALSE;

  if(oldhead != newhead || !oldhead)
  {

init_resync:

    fr->header_change = 2;
    if(oldhead) {
      if((oldhead & 0xc00) == (newhead & 0xc00)) {
        if( (oldhead & 0xc0) == 0 && (newhead & 0xc0) == 0)
    	  fr->header_change = 1; 
        else if( (oldhead & 0xc0) > 0 && (newhead & 0xc0) > 0)
	  fr->header_change = 1;
      }
    }


#ifdef SKIP_JUNK
	if(!firsthead && !head_check(newhead) ) {
		int i;

		fprintf(stderr,"Junk at the beginning\n");

		/* I even saw RIFF headers at the beginning of MPEG streams ;( */
		if(newhead == ('R'<<24)+('I'<<16)+('F'<<8)+'F') {
			rd->skip_bytes(68);
			fprintf(stderr,"Skipped RIFF header!\n");
			goto read_again;
		}

		/* and those ugly ID3 tags */
		if((newhead & 0xffffff00) == ('T'<<24)+('A'<<16)+('G'<<8)) {
			rd->skip_bytes(124);
			fprintf(stderr,"Skipped ID3 Tag!\n");
			goto read_again;
		}
#if 0
		/* search in 32 bit steps through the first 2K */
		for(i=0;i<512;i++) {
			if(!rd->head_read(hbuf,&newhead))
				return 0;
			if(head_check(newhead))
				break;
		}
		if(i==512)
#endif
		{
			/* step in byte steps through next 64K */
			for(i=0;i<65536;i++) {
				if(!rd->head_shift(hbuf,&newhead))
					return 0;
				if(head_check(newhead))
					break;
			}
			if(i == 65536) {
				fprintf(stderr,"Giving up searching valid MPEG header\n");
				return 0;
			}
		}
		/* 
		 * should we additionaly check, whether a new frame starts at
		 * the next expected position? (some kind of read ahead)
		 * We could implement this easily, at least for files.
		 */
	}
#endif

    if( (newhead & 0xffe00000) != 0xffe00000) {
      if (!param.quiet)
        fprintf(stderr,"Illegal Audio-MPEG-Header 0x%08lx at offset 0x%lx.\n",
              newhead,rd->tell()-4);
      if (param.tryresync) {
        int try = 0;
            /* Read more bytes until we find something that looks
               reasonably like a valid header.  This is not a
               perfect strategy, but it should get us back on the
               track within a short time (and hopefully without
               too much distortion in the audio output).  */
        do {
          try++;
          rd->head_shift(hbuf,&newhead);
          if (!oldhead)
            goto init_resync;       /* "considered harmful", eh? */

        } while ((newhead & HDRCMPMASK) != (oldhead & HDRCMPMASK)
              && (newhead & HDRCMPMASK) != (firsthead & HDRCMPMASK));
        if (!param.quiet)
          fprintf (stderr, "Skipped %d bytes in input.\n", try);
      }
      else
        return (0);
    }
    if (!firsthead)
      firsthead = newhead;

    if(!decode_header(fr,newhead))
      return 0;

  }
  else
    fr->header_change = 0;

  /* flip/init buffer for Layer 3 */
  bsbufold = bsbuf;
  bsbuf = bsspace[bsnum]+512;
  bsnum = (bsnum + 1) & 1;

  /* read main data into memory */
  if(!rd->read_frame_body(fr->framesize))
    return 0;

  if (halfspeed && fr->lay == 3)
    memcpy (ssave, bsbuf, ssize);

  return 1;

}


/*
 * the code a header and write the information
 * into the frame structure
 */
static int decode_header(struct frame *fr,unsigned long newhead)
{
    if( newhead & (1<<20) ) {
      fr->lsf = (newhead & (1<<19)) ? 0x0 : 0x1;
      fr->mpeg25 = 0;
    }
    else {
      fr->lsf = 1;
      fr->mpeg25 = 1;
    }
    
    if (!param.tryresync || !oldhead) {
          /* If "tryresync" is true, assume that certain
             parameters do not change within the stream! */
      fr->lay = 4-((newhead>>17)&3);
      fr->bitrate_index = ((newhead>>12)&0xf);
      if( ((newhead>>10)&0x3) == 0x3) {
        fprintf(stderr,"Stream error\n");
        exit(1);
      }
      if(fr->mpeg25) {
        fr->sampling_frequency = 6 + ((newhead>>10)&0x3);
      }
      else
        fr->sampling_frequency = ((newhead>>10)&0x3) + (fr->lsf*3);
      fr->error_protection = ((newhead>>16)&0x1)^0x1;
    }

    if(fr->mpeg25) /* allow Bitrate change for 2.5 ... */
      fr->bitrate_index = ((newhead>>12)&0xf);

    fr->padding   = ((newhead>>9)&0x1);
    fr->extension = ((newhead>>8)&0x1);
    fr->mode      = ((newhead>>6)&0x3);
    fr->mode_ext  = ((newhead>>4)&0x3);
    fr->copyright = ((newhead>>3)&0x1);
    fr->original  = ((newhead>>2)&0x1);
    fr->emphasis  = newhead & 0x3;

    fr->stereo    = (fr->mode == MPG_MD_MONO) ? 1 : 2;

    oldhead = newhead;

    if(!fr->bitrate_index)
    {
      fprintf(stderr,"Free format not supported.\n");
      return (0);
    }

    switch(fr->lay)
    {
      case 1:
		fr->do_layer = do_layer1;
#ifdef VARMODESUPPORT
        if (varmode) {
          fprintf(stderr,"Sorry, layer-1 not supported in varmode.\n"); 
          return (0);
        }
#endif
        fr->jsbound = (fr->mode == MPG_MD_JOINT_STEREO) ? 
                         (fr->mode_ext<<2)+4 : 32;
        fr->framesize  = (long) tabsel_123[fr->lsf][0][fr->bitrate_index] * 12000;
        fr->framesize /= freqs[fr->sampling_frequency];
        fr->framesize  = ((fr->framesize+fr->padding)<<2)-4;
        break;
      case 2:
		fr->do_layer = do_layer2;
#ifdef VARMODESUPPORT
        if (varmode) {
          fprintf(stderr,"Sorry, layer-2 not supported in varmode.\n"); 
          return (0);
        }
#endif
        get_II_stuff(fr);
        fr->jsbound = (fr->mode == MPG_MD_JOINT_STEREO) ?
                         (fr->mode_ext<<2)+4 : fr->II_sblimit;
        fr->framesize = (long) tabsel_123[fr->lsf][1][fr->bitrate_index] * 144000;
        fr->framesize /= freqs[fr->sampling_frequency];
        fr->framesize += fr->padding - 4;
        break;
      case 3:
        fr->do_layer = do_layer3;
        if(fr->lsf)
          ssize = (fr->stereo == 1) ? 9 : 17;
        else
          ssize = (fr->stereo == 1) ? 17 : 32;
        if(fr->error_protection)
          ssize += 2;
#ifdef VARMODESUPPORT
        if (varmode)
          playlimit = ((unsigned int) hbuf[6] << 8) | (unsigned int) hbuf[7];
          fr->framesize = ssize + 
                      (((unsigned int) hbuf[4] << 8) | (unsigned int) hbuf[5]);
        else {
#endif
          fr->framesize  = (long) tabsel_123[fr->lsf][2][fr->bitrate_index] * 144000;
          fr->framesize /= freqs[fr->sampling_frequency]<<(fr->lsf);
          fr->framesize = fr->framesize + fr->padding - 4;
#ifdef VARMODESUPPORT
        }
#endif
        break; 
      default:
        fprintf(stderr,"Sorry, unknown layer type.\n"); 
        return (0);
    }
    return 1;
}

#ifdef MPG123_REMOTE
void print_rheader(struct frame *fr)
{
	static char *modes[4] = { "Stereo", "Joint-Stereo", "Dual-Channel", "Single-Channel" };
	static char *layers[4] = { "Unknown" , "I", "II", "III" };
	static char *mpeg_type[2] = { "1.0" , "2.0" };

	/* version, layer, freq, mode, channels, bitrate, BPF */
	fprintf(stderr,"@I %s %s %ld %s %d %d %d\n",
			mpeg_type[fr->lsf],layers[fr->lay],freqs[fr->sampling_frequency],
			modes[fr->mode],fr->stereo,
			tabsel_123[fr->lsf][fr->lay-1][fr->bitrate_index],
			fr->framesize+4);
}
#endif

void print_header(struct frame *fr)
{
	static char *modes[4] = { "Stereo", "Joint-Stereo", "Dual-Channel", "Single-Channel" };
	static char *layers[4] = { "Unknown" , "I", "II", "III" };

	fprintf(stderr,"MPEG %s, Layer: %s, Freq: %ld, mode: %s, modext: %d, BPF : %d\n", 
		fr->mpeg25 ? "2.5" : (fr->lsf ? "2.0" : "1.0"),
		layers[fr->lay],freqs[fr->sampling_frequency],
		modes[fr->mode],fr->mode_ext,fr->framesize+4);
	fprintf(stderr,"Channels: %d, copyright: %s, original: %s, CRC: %s, emphasis: %d.\n",
		fr->stereo,fr->copyright?"Yes":"No",
		fr->original?"Yes":"No",fr->error_protection?"Yes":"No",
		fr->emphasis);
	fprintf(stderr,"Bitrate: %d Kbits/s, Extension value: %d\n",
		tabsel_123[fr->lsf][fr->lay-1][fr->bitrate_index],fr->extension);
}

void print_header_compact(struct frame *fr)
{
	static char *modes[4] = { "stereo", "joint-stereo", "dual-channel", "mono" };
	static char *layers[4] = { "Unknown" , "I", "II", "III" };
 
	fprintf(stderr,"MPEG %s layer %s, %d kbit/s, %ld Hz %s\n",
		fr->mpeg25 ? "2.5" : (fr->lsf ? "2.0" : "1.0"),
		layers[fr->lay],
		tabsel_123[fr->lsf][fr->lay-1][fr->bitrate_index],
		freqs[fr->sampling_frequency], modes[fr->mode]);
}

static void print_id3_tag(char *buf)
{
	struct id3tag {
		char tag[3];
		char title[30];
		char artist[30];
		char album[30];
		char year[4];
		char comment[30];
		unsigned char genre;
	};
	struct id3tag *tag = (struct id3tag *) buf;
	char title[31]={0,};
	char artist[31]={0,};
	char album[31]={0,};
	char year[5]={0,};
	char comment[31]={0,};

	if(param.quiet)
		return;

	strncpy(title,tag->title,30);
	strncpy(artist,tag->artist,30);
	strncpy(album,tag->album,30);
	strncpy(year,tag->year,4);
	strncpy(comment,tag->comment,30);

	fprintf(stderr,"Title  : %30s  Artist: %s\n",title,artist);
	fprintf(stderr,"Album  : %30s  Year: %4s, Genre: %d\n",album,year,(int)tag->genre);
	fprintf(stderr,"Comment: %30s \n",comment);
}

#if 0
/* removed the strndup for better portability */
/*
 *   Allocate space for a new string containing the first
 *   "num" characters of "src".  The resulting string is
 *   always zero-terminated.  Returns NULL if malloc fails.
 */
char *strndup (const char *src, int num)
{
	char *dst;

	if (!(dst = (char *) malloc(num+1)))
		return (NULL);
	dst[num] = '\0';
	return (strncpy(dst, src, num));
}
#endif

/*
 *   Split "path" into directory and filename components.
 *
 *   Return value is 0 if no directory was specified (i.e.
 *   "path" does not contain a '/'), OR if the directory
 *   is the same as on the previous call to this function.
 *
 *   Return value is 1 if a directory was specified AND it
 *   is different from the previous one (if any).
 */

int split_dir_file (const char *path, char **dname, char **fname)
{
	static char *lastdir = NULL;
	char *slashpos;

	if ((slashpos = strrchr(path, '/'))) {
		*fname = slashpos + 1;
		*dname = strdup(path); /* , 1 + slashpos - path); */
		if(!(*dname)) {
			perror("memory");
			exit(1);
		}
		(*dname)[1 + slashpos - path] = 0;
		if (lastdir && !strcmp(lastdir, *dname)) {
			/***   same as previous directory   ***/
			free (*dname);
			*dname = lastdir;
			return 0;
		}
		else {
			/***   different directory   ***/
			if (lastdir)
				free (lastdir);
			lastdir = *dname;
			return 1;
		}
	}
	else {
		/***   no directory specified   ***/
		if (lastdir) {
			free (lastdir);
			lastdir = NULL;
		};
		*dname = NULL;
		*fname = (char *)path;
		return 0;
	}
}

/* open the device to read the bit stream from it */

void open_stream(char *bs_filenam,int fd)
{
    int i;
    filept_opened = 1;

    if (!bs_filenam) {
		if(fd < 0) {
			filept = 0;
			filept_opened = 0;
		}
		else
			filept = fd;
	}
	else if (!strncmp(bs_filenam, "http://", 7)) 
		filept = http_open(bs_filenam);
	else if (!(filept = open(bs_filenam, O_RDONLY))) {
		perror (bs_filenam);
		exit(1);
	}

    for(i=0;;i++) {
      if(!readers[i].init) {
        rd = &readers[i];
        break;
      }
      if(readers[i].init(readers+i) >= 0) {
        rd = &readers[i];
        break;
      }
    }

}

#if 0
static void check_buffer_range(int size)
{
	int pos = (wordpointer-bsbuf) + (size>>3);

	if( pos >= fsizeold) {
		fprintf(stderr,"Pointer out of range (%d,%d)!\n",pos,fsizeold);
	}
}
#endif

#if !defined(I386_ASSEM) || defined(DEBUG_GETBITS)
#ifdef _gcc_
inline 
#endif
unsigned int getbits(int number_of_bits)
{
  unsigned long rval;

#ifdef DEBUG_GETBITS
fprintf(stderr,"g%d",number_of_bits);
#endif

  if(!number_of_bits)
    return 0;

#if 0
   check_buffer_range(number_of_bits+bitindex);
#endif

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

#ifdef DEBUG_GETBITS
fprintf(stderr,":%x ",rval);
#endif
  return rval;
}

#ifdef _gcc_
inline
#endif
unsigned int getbits_fast(int number_of_bits)
{
  unsigned long rval;

#ifdef DEBUG_GETBITS
fprintf(stderr,"g%d",number_of_bits);
#endif

#if 0
   check_buffer_range(number_of_bits+bitindex);
#endif

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


#ifdef DEBUG_GETBITS
fprintf(stderr,":%x ",rval);
#endif


  return rval;
}

#ifdef _gcc_
inline 
#endif
unsigned int get1bit(void)
{
  unsigned char rval;

#ifdef DEBUG_GETBITS
fprintf(stderr,"g%d",1);
#endif

#if 0
   check_buffer_range(1+bitindex);
#endif

  rval = *wordpointer << bitindex;

  bitindex++;
  wordpointer += (bitindex>>3);
  bitindex &= 7;

#ifdef DEBUG_GETBITS
fprintf(stderr,":%d ",rval>>7);
#endif

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
