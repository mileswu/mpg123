/*
 * Mpeg Audio Layer 3 
 * ------------------
 * huffman table handler
 * copyrights (c) 1995,1996 by Michael Hipp
 */

#include "mpg123.h"
#include "huffman.h"
     
void huffman_decoder(int hi,int *i)
{  
  struct newhuff *h = ht+hi;
  register int x,y;

  {
    register short *val = h->table;
    while((y=*val++)<0)
      if (get1bit())
        val -= y;
    x = y >> 4;
    y &= 0xf;
  }

  if (h->linbits)
  {
    if (x == 15) {
      x += getbits(h->linbits);
      if(get1bit())
        *i++ = -x;
      else
        *i++ = x;
    }
    else if (x && get1bit()) 
      *i++ = -x;
    else
      *i++ = x;
    if (y == 15) {
      y += getbits(h->linbits);
      if(get1bit())
        *i = -y;
      else 
        *i = y;
    }
    else if (y && get1bit()) 
      *i = -y;
    else
      *i = y;
  }
  else
  {
    if (x && get1bit()) 
      *i++ = -x;
    else 
      *i++ = x;
    if (y && get1bit()) 
      *i = -y;
    else
      *i = y;
  }
}

void huffman_count1(int hi,int *i)
{  
  struct newhuff *h = htc+hi;
  register short *val = h->table,a;

  while((a=*val++)<0)
    if (get1bit())
      val -= a;

  if (a & 0x8)
    if (get1bit()) 
      *i++ = -1;
    else
      *i++ = 1;
  else
    *i++ = 0;

  if (a & 0x4)
    if (get1bit())
      *i++ = -1;
    else
      *i++ = 1;
  else
    *i++ = 0;

  if (a & 0x2)
    if (get1bit())
      *i++ = -1;
    else
      *i++ = 1;
  else
    *i++ = 0;

  if (a & 0x1)
    if (get1bit())
      *i = -1;
    else
      *i = 1;
  else
    *i = 0;
}


