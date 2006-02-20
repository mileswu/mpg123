###
###   mpg123  Makefile
###

# Where to install binary and manpage on "make install":

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/man
SECTION=1

###################################################
######                                       ######
######   End of user-configurable settings   ######
######                                       ######
###################################################

nothing-specified:
	@echo ""
	@echo "You must specify the system which you want to compile for:"
	@echo ""
	@echo "make linux           Linux"
	@echo "make freebsd         FreeBSD"
	@echo "make solaris         Solaris 2.x (tested: 2.5 and 2.5.1) using SparcWorks CC"
	@echo "make solaris-gcc     Solaris 2.x using GNU CC (somewhat slower)"
	@echo "make sunos           SunOS 4.x (tested: 4.1.4)"
	@echo "make hpux            HP/UX 7xx"
	@echo "make sgi             SGI running IRIX"
	@echo "make dec             DEC Unix (tested: 3.2 and 4.0), OSF/1"
	@echo "make aix             IBM AIX (tested: 4.1)"
	@echo "make generic         try this one if your system isn't listed above"
	@echo ""
	@echo "Please read the file INSTALL for additional information."
	@echo ""

linux:
	$(MAKE) OBJECTS='decode_i386.o getbits.o' linux-generic

linux-generic:
	$(MAKE) CC=gcc LDFLAGS= \
		CFLAGS='-DI386_ASSEM -DREAL_IS_FLOAT -DLINUX -Wall -O2 -m486 \
			-fomit-frame-pointer -funroll-all-loops \
			-finline-functions -ffast-math' \
		mpg123
#### the following defines are for experimental use ... 
#
#CFLAGS='-DI386_ASSEM -O2 -DREAL_IS_FLOAT -DLINUX -Wall -g'
#CFLAGS='-pg -DI386_ASSEM -DREAL_IS_FLOAT -DLINUX -Wall -O2 -m486 -funroll-all-loops -finline-functions -ffast-math'
#CFLAGS='-DI386_ASSEM -DREAL_IS_FLOAT -DLINUX -Wall -O2 -m486 -fomit-frame-pointer -funroll-all-loops -finline-functions -ffast-math -malign-loops=2 -malign-jumps=2 -malign-functions=2'

freebsd:
	$(MAKE) OBJECTS='decode_i386.o getbits_.o' freebsd-generic

freebsd-generic:
	$(MAKE) CC=cc LDFLAGS= \
		CFLAGS='-Wall -ansi -pedantic -O4 -m486 -fomit-frame-pointer \
			-funroll-all-loops -ffast-math -DROT_I386 \
			-DI386_ASSEM -DREAL_IS_FLOAT' \
		mpg123

solaris:
	$(MAKE) CC=cc LDFLAGS='-lsocket -lnsl' OBJECTS=decode.o \
		CFLAGS='-fast -native -xO5 -DSOLARIS -DREAL_IS_FLOAT' \
		mpg123

solaris-gcc:
	$(MAKE) CC=gcc LDFLAGS='-lsocket -lnsl' OBJECTS=decode.o \
		CFLAGS='-O2 -Wall -DSOLARIS -DREAL_IS_FLOAT \
			-funroll-all-loops -finline-functions' \
		mpg123

sunos:
	$(MAKE) CC=gcc LDFLAGS= OBJECTS=decode.o \
		CFLAGS='-O2 -DSUNOS -DREAL_IS_FLOAT -funroll-loops' \
		mpg123

hpux:
	$(MAKE) CC=cc LDFLAGS= OBJECTS=decode.o \
		CFLAGS='-DREAL_IS_FLOAT -Aa +O3 -D_HPUX_SOURCE -DHPUX' \
		mpg123

sgi:
	$(MAKE) CC=cc LDFLAGS= OBJECTS=decode.o AUDIO_LIB=-laudio \
		CFLAGS='-O2 -DSGI -DREAL_IS_FLOAT' \
		mpg123

dec:
	$(MAKE) CC=cc LDFLAGS= OBJECTS=decode.o \
		CFLAGS='-std1 -warnprotos -O4' \
		mpg123

aix:
	$(MAKE) LDFLAGS= OBJECTS=decode.o \
		CFLAGS='-O -DAIX' \
		mpg123

generic:
	$(MAKE) LDFLAGS= OBJECTS=decode.o \
		CFLAGS='-O' \
		mpg123

mpg123: mpg123.o common.o $(OBJECTS) decode_2to1.o dct64.o decode_4to1.o tabinit.o audio.o layer1.o layer2.o layer3.o buffer.o getlopt.o httpget.o Makefile
	$(CC) $(CFLAGS) $(LDFLAGS)  mpg123.o tabinit.o common.o layer1.o \
		layer2.o layer3.o audio.o buffer.o decode_2to1.o \
		decode_4to1.o getlopt.o httpget.o dct64.o $(OBJECTS) \
		-o mpg123 -lm $(AUDIO_LIB)

tst:
	gcc $(CFLAGS) -S decode.c

layer1.o:	mpg123.h
layer2.o:	mpg123.h
layer3.o:	mpg123.h huffman.h
decode.o:	mpg123.h
decode_int.o:	mpg123.h
decode_2to1.o:	mpg123.h
decode_4to1.o:	mpg123.h
decode_i386.o:	mpg123.h
common.o:	mpg123.h tables.h
mpg123.o:	mpg123.h getlopt.h
audio.o:	mpg123.h
buffer.o:	mpg123.h
getbits.o:	mpg123.h
getbits_.o:	mpg123.h
tabinit.o:	mpg123.h
getlopt.o:	getlopt.h
httpget.o:	mpg123.h
dct64.o: mpg123.h

clean:
	rm -f *.o *core *~ mpg123

prepared-for-install:
	@if [ ! -x mpg123 ]; then \
		echo '###' ; \
		echo '###  Before doing "make install", you have to compile the software.' ; \
		echo '### Type "make" for more information.' ; \
		echo '###' ; \
		exit 1 ; \
	fi

install:	prepared-for-install
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

