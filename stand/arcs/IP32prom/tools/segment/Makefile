SHELL = /bin/sh
CFLAGS = -O -g -DDEBUG
OBJECTS = \
	  segment.o

BINARIES = \
	   findseg \
	   main

SUBDIR	 = hdrlength

default: all

all: $(SUBDIR:=DIR) $(BINARIES)

clobber: clean
clean: $(SUBDIR:=CLEAN)
	rm -f $(BINARIES) *.o

$(SUBDIR:=DIR):
	cd $(@:DIR=) ; make

$(SUBDIR:=CLEAN):
	cd $(@:CLEAN=) ; make clean

findseg: findseg.c $(OBJECTS)
	$(CC) $(CFLAGS) -o findseg findseg.c $(OBJECTS)

main: main.c $(OBJECTS)
	$(CC) $(CFLAGS) -o main main.c $(OBJECTS)

.c.o:
	${CC} $(CFLAGS) -c $<

segment.o: segment.c segment.h

install: default
