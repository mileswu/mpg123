/*
 *   buffer.c
 *
 *   Oliver Fromme  <oliver.fromme@heim3.tu-clausthal.de>
 *   Mon Apr 14 03:53:18 MET DST 1997
 */

#include <signal.h>

#include "mpg123.h"

#define MAXOUTBURST 32768

static int intflag = FALSE;

static void catch_interrupt(void)
{
  intflag = TRUE;
}

void buffer_loop(struct audio_info_struct *ai)
{
	int bytes;
	int my_fd = buffermem->fd[XF_READER];
	txfermem *xf = buffermem;
	int done = FALSE;

	if(outmode==DECODE_AUDIO) {
		if(audio_open(ai) < 0) {
			perror("audio");
			exit(1);
		}
		audio_set_rate (ai);
	}

	catchsignal (SIGINT, catch_interrupt);
	for (;;) {
		if (intflag) {
#ifdef SOLARIS
			audio_queueflush (ai);
#endif
			xf->readindex = xf->freeindex;
			intflag = FALSE;
		}
		if (!(bytes = xfermem_get_usedspace(xf))) {
			if (done)
				break;
			if (xfermem_block(XF_READER, xf) != XF_CMD_WAKEUP)
				done = TRUE;
			continue;
		}
		if (bytes > xf->size - xf->readindex)
			bytes = xf->size - xf->readindex;
		if (bytes > MAXOUTBURST)
			bytes = MAXOUTBURST;
		if (outmode == DECODE_STDOUT)
			bytes = write(1, xf->data + xf->readindex, bytes);
		else if (outmode == DECODE_AUDIO) {
#if 0
			if (!(bytes = (bytes >> 1) & (~ 1l)))
				continue;
#endif
			bytes = audio_play_samples(ai,
				(unsigned char *) (xf->data + xf->readindex), bytes);
		}
		xf->readindex = (xf->readindex + bytes) % xf->size;
		if (xf->wakeme[XF_WRITER])
			xfermem_putcmd(my_fd, XF_CMD_WAKEUP);
	}

	if(outmode==DECODE_AUDIO)
		audio_close(ai);
}

/* EOF */
