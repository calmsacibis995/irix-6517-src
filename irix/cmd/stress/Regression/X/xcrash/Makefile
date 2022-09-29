#
# Makefile for xcrash
#

CC = cc
CFLAGS = -32 -nostdinc -I$(ROOT)/usr/include -O
LFLAGS = -32 -nostdlib -L$(ROOT)/lib -L$(ROOT)/usr/lib

all: xcrash

xcrash:
	$(CC) $(CFLAGS) -o $@ xcrash.c $(LFLAGS) -lX11

install: all
	/etc/install -idb "diag.sw.diag mach(MODE=32bit)"	\
	-F /usr/diags/regression/X xcrash

clean:
	rm -f xcrash.o core a.out

clobber: clean
	rm -f xcrash
