#!smake
#
# IRIX makefile for the sgi/src/test subdirectory of the FFSC software
#
# Test programs. This subdirectory should not normally be built as
# part of a formal build.
#

TARGETS = graph oob elsc
LDIRT   = *~
CFLAGS	= -g -I../sgi/include

default: $(TARGETS)


clean:
	rm -f *.o $(LDIRT)

clobber: clean
	rm -f $(TARGETS)
