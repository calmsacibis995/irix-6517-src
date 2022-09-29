#
# Makefile for xstress
#

CC = cc
CFLAGS = -32 -nostdinc -I$(ROOT)/usr/include -O
LFLAGS = -32 -nostdlib -L$(ROOT)/lib -L$(ROOT)/usr/lib

all: xstress

xstress:
	$(CC) $(CFLAGS) -o $@ xstress.c $(LFLAGS) -lX11 -lm

install: all
	/etc/install -idb "diag.sw.diag mach(MODE=32bit)"	\
	-F /usr/diags/regression/X xstress

clean:
	rm -f xstress.o core a.out

clobber: clean
	rm -f xstress
