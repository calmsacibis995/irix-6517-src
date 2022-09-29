CFILES= y1.c y2.c y3.c y4.c
TARGETS= myacc
OBJECTS= $(CFILES:.c=.o)
CFLAGS= -32 -woff 1685,515,608,658,799,803,852,1048,1233,1499
LDFLAGS= -32

default: $(TARGETS)

myacc: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm -f $(OBJECTS)

clobber: clean
	rm -f $(TARGETS)
