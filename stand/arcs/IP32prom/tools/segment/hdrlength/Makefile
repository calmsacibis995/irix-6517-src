CFLAGS = -O -g -DDEBUG
OBJECTS = \
	  segment.o

BINARIES = \
	   findseg \
	   main \
	   hdrlength

default: all

all: $(BINARIES)

clobber: clean

clean:
	rm -f $(BINARIES) *.o

hdrlength: hdrlength.o $(OBJECTS)
	$(CC) -o $@ $@.o $(OBJECTS)

findseg: findseg.c $(OBJECTS)
	$(CC) $(CFLAGS) -o findseg findseg.c $(OBJECTS)

main: main.c $(OBJECTS)
	$(CC) $(CFLAGS) -o main main.c $(OBJECTS)

.c.o:
	${CC} $(CFLAGS) -c $<

segment.o: segment.c segment.h

install: default
