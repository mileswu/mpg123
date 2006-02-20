/*
 *   buffer.c
 *
 *   Oliver Fromme  <oliver.fromme@heim3.tu-clausthal.de>
 *   Thu Apr 10 14:10:27 MET DST 1997
 */

#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#ifdef AIX
#include <sys/select.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/errno.h>
extern int errno;

#include "mpg123.h"

static int intflag = FALSE;
static int player_fd[2];
static int player_pid;
#define PLAYBUFSIZE 32768

static void catch_interrupt(void)
{
  intflag = TRUE;
}

static void player_loop(struct audio_info_struct *ai)
{
  char buffer[PLAYBUFSIZE];
  int freeindex, readindex, bytes;
  int done = FALSE;

  if(outmode==DECODE_AUDIO) {
    if(audio_open(ai) < 0) {
      perror("audio");
      exit(1);
    }
    audio_set_rate (ai);
  }

  while (!done) {
    freeindex = readindex = 0;
    do  {
      if ((bytes = read(player_fd[0], buffer+freeindex, PLAYBUFSIZE - freeindex)) > 0)
        freeindex += bytes;
      else
        if (!(bytes == -1 && errno == EINTR))
          done = TRUE;
    } while (freeindex < PLAYBUFSIZE && !done);
    if (freeindex) do {
      if (outmode == DECODE_STDOUT)
        bytes = write(1, buffer+readindex, freeindex - readindex);
      else if (outmode == DECODE_AUDIO) {
        bytes = ((freeindex - readindex) >> 1) & (~ 1l);
        if (bytes)
          bytes = audio_play_samples(ai,
                  (short *) (buffer+readindex), bytes);
      }
      else
        bytes = freeindex;
      if (bytes > 0)
        readindex += bytes;
      else
        if (bytes == -1 && errno != EINTR)
          done = TRUE;
    } while (readindex+3 < freeindex && !done);
  }

  if(outmode==DECODE_AUDIO)
    audio_close(ai);
}

void buffer_loop(struct audio_info_struct *ai)
{
  unsigned char *buffer;
  fd_set read_fdset, write_fdset;
  unsigned long freeindex = 0, readindex = 0, bufsize = usebuffer * 1024;
  unsigned long bytesused = 0, bytesfree;
  int done = FALSE, read_eof = FALSE;

  bytesfree = bufsize - 4;
  if (!(buffer = (unsigned char *) malloc((size_t) bufsize))) {
    perror("malloc()");
    exit(1);
  }

  catchsignal (SIGINT, catch_interrupt);
  if (pipe(player_fd)) {
    perror ("pipe()");
    exit (1);
  }
  switch ((player_pid = fork())) {
    case -1: /* error */
      perror("fork()");
      exit(1);
    case 0: /* child */
      close (player_fd[1]);
      player_loop(ai);
      close (player_fd[0]);
      exit(0);
    default: /* parent */
      close (player_fd[0]);
  }
  fcntl (player_fd[1], F_SETFL, O_NONBLOCK);

  while (!done) {
    if (intflag) {
      freeindex = readindex = bytesused = 0;
      bytesfree = bufsize - 4;
      intflag = FALSE;
    }
    FD_ZERO (&read_fdset);
    FD_ZERO (&write_fdset);
    if (!read_eof && bytesfree)
      FD_SET (buffer_fd[0], &read_fdset);
    if (bytesused > 3)
      FD_SET (player_fd[1], &write_fdset);
    if (select(FD_SETSIZE, &read_fdset, &write_fdset, NULL, NULL) <= 0) {
      if (errno != EINTR) {
        read_eof = TRUE;
        if (bytesused < 4)
          done = TRUE;
      }
    }
    else
      if (FD_ISSET(buffer_fd[0], &read_fdset)) {
        size_t numread;
        if ((numread = bufsize - freeindex) > bytesfree)
          numread = bytesfree;
        if ((numread = read(buffer_fd[0], buffer+freeindex, numread)) > 0) {
          freeindex = (freeindex + numread) % bufsize;
          bytesused += numread;
          bytesfree -= numread;
        }
        else if (!(numread == -1 && errno == EINTR)) {
          read_eof = TRUE;
          if (bytesused < 4)
            done = TRUE;
        }
      }
      else if (FD_ISSET(player_fd[1], &write_fdset)) {
        size_t numwrite;
        if ((numwrite = bufsize - readindex) > bytesused)
          numwrite = bytesused;
        if ((numwrite = write(player_fd[1], buffer+readindex, numwrite)) > 0) {
          readindex = (readindex + numwrite) % bufsize;
          bytesfree += numwrite;
          bytesused -= numwrite;
          if (bytesused < 4 && read_eof)
            done = TRUE;
        }
      }
  }

  fcntl (player_fd[1], F_SETFL, 0);
  free (buffer);
  close (player_fd[1]);
  waitpid (player_pid, NULL, 0);
}

/* EOF */
