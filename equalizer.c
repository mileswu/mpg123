
#include "mpg123.h"

real equalizer[2][32];
real equalizer_sum[2][32];
int equalizer_cnt;

void do_equalizer(real *bandPtr,int channel) 
{
	int i;

	if(flags.equalizer & 0x1) {
		for(i=0;i<32;i++)
			bandPtr[i] *= equalizer[channel][i];
	}

	if(flags.equalizer & 0x2) {
		equalizer_cnt++;
		for(i=0;i<32;i++)
			equalizer_sum[channel][i] += bandPtr[i];
	}
}

