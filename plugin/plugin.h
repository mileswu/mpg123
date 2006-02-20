
struct datainfo {
	int channels;	/* used channels */
	int freqbands;
	float *freqs;  /* [channels][576] */
	short *pcm;    /* [channels][576] */
};

struct config {
	void *priv;			 /* optional set by plugin */
	int (*init)(struct config *c);   /* set by plugin */
	int (*exit)(struct config *c);   /* set by plugin */
	int (*action)(struct config *c); /* set by plugin */
	unsigned long flags;
	char *name;			 /* set by plugin */
	struct datainfo *di;	/* set by mpg123 */
};

