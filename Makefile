
### define REAL_IS_FLOAT, if float-precision is enough for you

### for Linux
#CC = gcc
#
### Uncomment the following 3 defines for full quality and best performance
#
#CFLAGS = -DI386_ASSEM -DREAL_IS_FLOAT -DLINUX -Wall -O2 -m486 -fomit-frame-pointer -funroll-all-loops -finline-functions -ffast-math
#LDFLAGS=
#OBJECTS = decode_i386.o getbits.o
#
#### the following defines are for experimental use ... 
#
#CFLAGS = -DI386_ASSEM -O2 -DREAL_IS_FLOAT -DLINUX -Wall -g
#CFLAGS = -pg -DI386_ASSEM -DREAL_IS_FLOAT -DLINUX -Wall -O2 -m486 -funroll-all-loops -finline-functions -ffast-math
#CFLAGS = -DI386_ASSEM -DREAL_IS_FLOAT -DLINUX -Wall -O2 -m486 -fomit-frame-pointer -funroll-all-loops -finline-functions -ffast-math -malign-loops=2 -malign-jumps=2 -malign-functions=2
#
#### decode_int => slightly reduces quality. Better performance
#
#OBJECTS = decode_int.o getbits.o
#
#### decoce_lp => 22Khz 'downsampling' .. Best performance .. 
#
#OBJECTS = decode_lp.o getbits.o

### for FreeBSD
#CC = cc
#CFLAGS = -Wshadow -O4 -m486 -fomit-frame-pointer -funroll-all-loops -ffast-math -DROT_I386 -DI386_ASSEM -DREAL_IS_FLOAT
#LDFLAGS=
#OBJECTS = decode_i386.o getbits_.o

### for Sun SPARCstation running Solaris 2.x
### Doesn't seem to work with Sun's SparcWorks cc, therefore use gcc.
#CC = gcc
#CFLAGS = -O2 -Wall -DSOLARIS -DREAL_IS_FLOAT -funroll-all-loops -finline-functions
#LDFLAGS=
#OBJECTS = decode.o 

### for Sun SPARCstation running SunOS 4.x
#CC = gcc
#CFLAGS = -O2 -Wall -DSUNOS -DREAL_IS_FLOAT -funroll-loops
#LDFLAGS=
#OBJECTS = decode.o 

### for HPUX
#CC = cc
#CFLAGS = -DREAL_IS_FLOAT -Aa +O3 -D_HPUX_SOURCE -DHPUX 
#LDFLAGS=
#OBJECTS = decode.o

### for SGI IRIX
#CC = cc
#CFLAGS = -O2 -DSGI -DREAL_IS_FLOAT 
#AUDIO_LIB = -laudio
#LDFLAGS=
#OBJECTS = decode.o 

### for DEC Alpha AXP / Digital Unix
### Note: currently this supports decoding to stdout ONLY,
###       there is no support for DEC's "AudioFile" yet!
###       However, it is possible to play mp3 files by piping
###       the output of mp123 into aplay or something like that.
### Note: Do not use -DREAL_IS_FLOAT, because it actually makes
###       the decoder slower (Alphas are 64 bit CPUs).
#CC = cc
#CFLAGS = -O4
#LDFLAGS=
#OBJECTS = decode.o 



PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/man
SECTION=1

all: verify_Makefile mpg123

verify_Makefile:
	@if [ "x$(OBJECTS)" = "x" ]; then \
		echo "***"; \
		echo "***  First edit the Makefile and uncomment the section"; \
		echo "***  corresponding to your hardware and operating system!"; \
		echo "***"; \
		exit 1; \
	fi

mpg123: mpg123.o common.o $(OBJECTS) audio.o layer1.o layer2.o layer3.o huffman.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS)  mpg123.o common.o layer1.o layer2.o layer3.o \
			huffman.o audio.o $(OBJECTS) -o mpg123 -lm $(AUDIO_LIB)

tst:
	gcc $(CFLAGS) -S decode.c
	gcc $(CFLAGS) -S huffman.c

huffman.o:	mpg123.h huffman.h
layer1.o:	mpg123.h
layer2.o:	mpg123.h
layer3.o:	mpg123.h
decode.o:	mpg123.h
common.o:	mpg123.h tables.h
musicout.o:	mpg123.h
audio.o:	mpg123.h

clean:
	rm -f *.o *core *~ mpg123

install:	verify_Makefile mpg123
	strip mpg123
	if [ -x /usr/ccs/bin/mcs ]; then /usr/ccs/bin/mcs -d mpg123; fi
	cp -f mpg123 $(BINDIR)
	chmod 755 $(BINDIR)/mpg123
	cp -f mpg123.1 $(MANDIR)/man$(SECTION)
	chmod 644 $(MANDIR)/man$(SECTION)/mpg123.1
	if [ -r $(MANDIR)/windex ]; then catman -w -M $(MANDIR) $(SECTION); fi

dist:	clean
	DISTNAME="`basename \`pwd\``" ; \
	cd .. ; \
	rm -f "$$DISTNAME".tar.gz "$$DISTNAME".tar ; \
	tar cvf "$$DISTNAME".tar "$$DISTNAME" ; \
	gzip -9 "$$DISTNAME".tar

