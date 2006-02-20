/*
 * mpg123 defines 
 * used source: musicout.h from mpegaudio package
 */

#include        <stdio.h>
#include        <string.h>
#include        <math.h>

#include        <unistd.h>

#ifdef SGI
#include <audio.h>
#endif

#include "xfermem.h"

#ifdef SUNOS
#define memmove bcopy
#endif

#ifdef REAL_IS_FLOAT
#  define real float
#elif defined(REAL_IS_LONG_DOUBLE)
#  define real long double
#else
#  define real double
#endif

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

#if defined(LINUX) || defined(__FreeBSD__)
#define VOXWARE
#endif

/* AUDIOBUFSIZE = n*64 with n=1,2,3 ...  */
#define		AUDIOBUFSIZE		16384

#define         FALSE                   0
#define         TRUE                    1

#define         MAX_NAME_SIZE           81
#define         SBLIMIT                 32
#define         SCALE_BLOCK             12
#define         SSLIMIT                 18

#define         MPG_MD_STEREO           0
#define         MPG_MD_JOINT_STEREO     1
#define         MPG_MD_DUAL_CHANNEL     2
#define         MPG_MD_MONO             3

enum { AUDIO_OUT_HEADPHONES,AUDIO_OUT_INTERNAL_SPEAKER,AUDIO_OUT_LINE_OUT };
enum { DECODE_TEST, DECODE_AUDIO, DECODE_STDOUT, DECODE_BUFFER };

struct al_table 
{
  short bits;
  short d;
};

struct frame {
    struct al_table *alloc;
    int (*synth)(real *,int,short *);
    int stereo;
    int jsbound;
    int single;
    int II_sblimit;
    int version;
    int lay;
    int error_protection;
    int bitrate_index;
    int sampling_frequency;
    int padding;
    int extension;
    int mode;
    int mode_ext;
    int copyright;
    int original;
    int emphasis;
};

#if defined(HPUX) || defined(SUNOS) || defined(SOLARIS) || defined(VOXWARE)
#define AUDIO_USES_FD
#endif

struct audio_info_struct
{
#ifdef AUDIO_USES_FD
  int fn; /* filenumber */
#endif
#ifdef SGI
  ALconfig config;
  ALport port;
#endif
  long rate;
  int gain;
  int output;
  char *device;
  int channels;
};

extern int outmode;  
extern int tryresync;
extern int quiet;
extern int halfspeed;
extern int usebuffer;
extern int buffer_fd[2];
extern txfermem *buffermem;
extern char *prgName, *prgVersion;

extern int audio_play_samples(struct audio_info_struct *,short *,int);
extern void buffer_loop(struct audio_info_struct *ai);

/* ------ Declarations from "httpget.c" ------ */

extern char *proxyurl;
extern unsigned long proxyip;
extern FILE *http_open (char *url);

/* ------ Declarations from "common.c" ------ */

extern void audio_flush(int, struct audio_info_struct *);
extern void (*catchsignal(int signum, void(*handler)()))();
extern unsigned int   get1bit(void);
extern unsigned int   getbits(int);
extern unsigned int   getbits_fast(int);

extern short *pcm_sample;
extern int pcm_point;
extern int audiobufsize;

#ifdef VARMODESUPPORT
extern int varmode;
extern int playlimit;
#endif

struct III_sideinfo
{
  unsigned main_data_begin;
  unsigned private_bits;
  struct {
    struct gr_info_s {
      int scfsi;
      unsigned part2_3_length;
      unsigned big_values;
      unsigned scalefac_compress;
      unsigned block_type;
      unsigned mixed_block_flag;
      unsigned table_select[3];
      unsigned subblock_gain[3];
      unsigned maxband[3];
      unsigned maxbandl;
      unsigned region1start;
      unsigned region2start;
      unsigned preflag;
      unsigned scalefac_scale;
      unsigned count1table_select;
      real *full_gain[3];
      real *pow2gain;
    } gr[2];
  } ch[2];
};

extern void open_stream(char *);
extern void close_stream(char *);
extern int read_frame(struct frame *fr);
extern int do_layer3(struct frame *fr,int,struct audio_info_struct *);
extern int do_layer2(struct frame *fr,int,struct audio_info_struct *);
extern int do_layer1(struct frame *fr,int,struct audio_info_struct *);
extern void print_header(struct frame *);
extern void set_pointer(long);
extern int synth_1to1 (real *,int,short *);
extern int synth_2to1 (real *,int,short *);
extern int synth_4to1 (real *,int,short *);
extern void rewindNbits(int bits);
extern int  hsstell(void);
extern void set_pointer(long);
extern void huffman_decoder(int ,int *);
extern void huffman_count1(int,int *);

extern void init_layer3(void);
extern void init_layer2(void);
extern void make_decode_tables(long scale);
extern void dct64(real *,real *,real *);

extern int audio_open(struct audio_info_struct *);
extern int audio_set_rate(struct audio_info_struct *);
extern int audio_set_channels(struct audio_info_struct *);
extern int audio_write_sample(struct audio_info_struct *,short *,int);
extern int audio_close(struct audio_info_struct *);

extern long freqs[4];
extern real muls[27][64];
extern real decwin[512+32];
extern real *pnts[5];




