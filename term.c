
#ifdef TERM_CONTROL

#include <termios.h>

#include "mpg123.h"

static int term_enable = 0;
/* initialze terminal */
void term_init(void)
{
  struct termios tio;

  term_enable = 0;

  if(tcgetattr(0,&tio) < 0) {
    fprintf(stderr,"Can't get terminal attributes\n");
    return;
  }
  tio.c_lflag &= ~(ICANON|ECHO);
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 0;

  if(tcsetattr(0,TCSANOW,&tio) < 0) {
    fprintf(stderr,"Can't set terminal attributes\n");
    return;
  }

  term_enable = 1;
 
}

void term_control(void)
{
  int n = 1;

  if(!term_enable)
    return;

  while(n > 0) {
    fd_set r;
    struct timeval t = {0,0};
    char val;

    FD_ZERO(&r);
    FD_SET(0,&r);
    n = select(1,&r,NULL,NULL,&t);
    if(n > 0 && FD_ISSET(0,&r)) {
      if(read(0,&val,1) <= 0)
        break;
      switch(val) {
        case 'B':
        case 'b':
          rd->rewind(rd);
          break;
      }
    }
  }

}

#endif

