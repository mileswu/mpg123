
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include "mpg123.h"

#include <stdlib.h>
#include <os2.h>
#define  INCL_OS2MM
#include <os2me.h>
#define BUFNUM     20
#define BUFSIZE 16384

typedef struct {
    ULONG operation;
    ULONG operand1;
    ULONG operand2;
    ULONG operand3;
} ple;
MCI_WAVE_SET_PARMS msp;
MCI_PLAY_PARMS mpp;
int id, pos=0;
ple pl[BUFNUM+2];

int audio_open(struct audio_info_struct *ai)
{
  char *buf[BUFNUM];
  int i;
  ULONG rc;
  MCI_OPEN_PARMS mop;

  pl[0].operation = (ULONG)NOP_OPERATION;
  pl[0].operand1 = 0;
  pl[0].operand2 = 0;
  pl[0].operand3 = 0;
  for(i = 0; i < BUFNUM; i++) {
    buf[i] = (char*)malloc(BUFSIZE);
    memset(buf[i], 0, BUFSIZE);
    pl[i+1].operation = (ULONG)DATA_OPERATION;
    pl[i+1].operand1 = (ULONG)buf[i];
    pl[i+1].operand2 = BUFSIZE/2;
    pl[i+1].operand3 = 0;
  }
  pl[BUFNUM+1].operation = (ULONG)BRANCH_OPERATION;
  pl[BUFNUM+1].operand1 = 0;
  pl[BUFNUM+1].operand2 = 1;
  pl[BUFNUM+1].operand3 = 0;

  mop.pszDeviceType = (PSZ)MCI_DEVTYPE_WAVEFORM_AUDIO_NAME;
  mop.pszElementName = (PSZ)&pl;

  rc = mciSendCommand(0, MCI_OPEN, MCI_WAIT
                                 | MCI_OPEN_PLAYLIST, (PVOID)&mop, 0);
  if (rc) {
    puts("open audio device failed!");
    exit(1);
  }
  id = mop.usDeviceID;

  msp.usBitsPerSample = 16;
  rc = mciSendCommand(id, MCI_SET, MCI_WAIT | MCI_WAVE_SET_BITSPERSAMPLE, (PVOID)&msp, 0);
  return 0;
}

int audio_reset_parameters(struct audio_info_struct *ai)
{
  static int audio_initialized = FALSE, i;

  if (audio_initialized) {
    while (pl[pos].operand3 < pl[pos].operand2) _sleep2(125);
    for(i = 1; i <= BUFNUM; i++) {
      memset((char*)pl[i].operand1, 0, BUFSIZE);
      pl[i].operand2 = BUFSIZE;
      pl[i].operand3 = 0;
    }
    _sleep2(2000);
    mciSendCommand(id, MCI_STOP, MCI_WAIT, (PVOID)&mpp, 0);
    for(i = 1; i <= BUFNUM; i++) {
      memset((char*)pl[i].operand1, 0, BUFSIZE);
      pl[i].operand2 = BUFSIZE/2;
      pl[i].operand3 = 0;
    }
    pos = 0;
  }
  msp.ulSamplesPerSec = ai->rate;
  msp.usChannels = ai->channels;
  mciSendCommand(id, MCI_SET, MCI_WAIT 
                            | MCI_WAVE_SET_SAMPLESPERSEC 
                            | MCI_WAVE_SET_CHANNELS, (PVOID)&msp, 0);

  mciSendCommand(id, MCI_PLAY, 0, (PVOID)&mpp, 0);

  if(!audio_initialized) audio_initialized = TRUE;
  return 0;
}

int audio_rate_best_match(struct audio_info_struct *ai)
{
  return 0;
}

int audio_set_rate(struct audio_info_struct *ai)
{
  return 0;
}

int audio_play_samples(struct audio_info_struct *ai,unsigned char *buf,int len)
{
  pos = pos + 1;

  if (pos > BUFNUM) pos = 1;

  while (pl[pos].operand3 < pl[pos].operand2) _sleep2(125);

  memcpy((char*)pl[pos].operand1, buf, len);
  pl[pos].operand2 = len;
  pl[pos].operand3 = 0;

  return len;
}

int audio_close(struct audio_info_struct *ai)
{
  ULONG rc;

  pl[pos].operation = (ULONG)EXIT_OPERATION;
  pl[pos].operand2 = 0;
  pl[pos].operand3 = 0;

  pos = pos - 1;
  if(pos == 0) pos = BUFNUM;

  while (pl[pos].operand3 < pl[pos].operand2) _sleep2(250);
  _sleep2(2000);

  rc = mciSendCommand(id, MCI_CLOSE, MCI_WAIT, (PVOID)NULL, 0);
  return 0;
}
