CFLAGS= -so

.c.o:
	cc $(CFLAGS) -o $@ $*.c -iinclude -sb
.a68.o:
	as -o $@ $*.a68

all:	amipx.library

amipx.library:	libstart.o amipx_lib.o
	ln -t -v -o amipx.library libstart.o amipx_lib.o -lcl
