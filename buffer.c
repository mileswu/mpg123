/*
 * Buffer functions for MPEG audio decoder
 * ---------------------------------------
 * copyright (c) 1997 by Oliver Fromme & Michael Hipp, All rights reserved.
 * See also 'README'!
 */

#include <stdlib.h>
#include <fcntl.h>
#ifdef AIX
#include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/errno.h>
extern int errno;

#include "mpg123.h"

int usebuffer;

void buffer_loop(struct audio_info_struct *ai)
{
  unsigned char *buffer;
  fd_set read_fdset, write_fdset;
  int write_fd;
  unsigned long freeindex = 0, readindex = 0, bufsize = usebuffer * 1024;
  unsigned long bytesused = 0, bytesfree;
  int done = FALSE, read_eof = FALSE;

  bytesfree = bufsize - 4;
  if (!(buffer = (unsigned char *) malloc((size_t) bufsize))) {
    perror("malloc()");
    exit(1);
  }
  if(outmode==DECODE_AUDIO)
    if(audio_open(ai) < 0) {
      perror("audio");
      exit(1);
    }
#ifdef AUDIO_USES_FD
  if (outmode == DECODE_AUDIO) {
    write_fd = ai->fn;
    fcntl (write_fd, F_SETFL, O_NONBLOCK);
  }
  else
#endif
    write_fd = 1;

  while (!done) {
    FD_ZERO (&read_fdset);
    FD_ZERO (&write_fdset);
    if (!read_eof && bytesfree)
      FD_SET (buffer_fd[0], &read_fdset);
    if (bytesused > 3)
      FD_SET (write_fd, &write_fdset);
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
        else {
          read_eof = TRUE;
          if (bytesused < 4)
            done = TRUE;
        }
      }
      if (FD_ISSET(write_fd, &write_fdset)) {
        size_t numwrite;
        if ((numwrite = bufsize - readindex) > bytesused)
          numwrite = bytesused;
        if (outmode == DECODE_STDOUT)
          numwrite = write(write_fd, buffer+readindex, numwrite);
        else if (outmode == DECODE_AUDIO) {
          numwrite = (numwrite >> 1) & (~ 1l);
          if (numwrite)
            numwrite = audio_play_samples(ai,
                    (short *) (buffer+readindex), numwrite);
        }
        if (numwrite) {
          readindex = (readindex + numwrite) % bufsize;
          bytesfree += numwrite;
          bytesused -= numwrite;
          if (bytesused < 4 && read_eof)
            done = TRUE;
        }
      }
  }

  if(outmode==DECODE_AUDIO)
    audio_close(ai);
  free (buffer);
}


