
all: dltst plugin.so

plugin.so: plugin.h plugin.c
	$(CC) -o plugin.so -shared plugin.c

dltst: dltst.o
	$(CC) -o $@ dltst.o -ldl

dltst.o: dltst.c plugin.h

clean:
	rm *.o plugin.so dltst

