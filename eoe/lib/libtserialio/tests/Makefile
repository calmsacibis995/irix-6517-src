#!/usr/sbin/smake -J 1

default: testtserialio testdeck vidsony tsio

SHELL=/bin/sh

clobber clean:
	/bin/rm -f $(TARGETLIB)
	/bin/rm -f $(TARGETAPPS)
	/bin/rm -f *.pure
	/bin/rm -f *.o
	/bin/rm -f Makedepend
	/bin/rm -f core
	/bin/rm -f so_locations

# with debug
 DEBUG=-DDEBUG -UNDEBUG -g
# without debug
#DEBUG=-UDEBUG -DNDEBUG -O

CCF=$(DEBUG) \
	-nostdinc -I$(ROOT)/usr/include -I. -I../../kern/sys \
	-nostdlib -L$(ROOT)/usr/lib \
	-o32 -mips2 \
	-float -fullwarn \
	-woff 813,814,826 \
	-wlint,-fzaiv

INCS=util.h

.c.o: $(INCS)
	cc -o $@ -c $< $(CCF)

testtserialio: testtserialio.o util.o
	cc -o testtserialio testtserialio.o util.o $(CCF) -ltserialio -ldmedia

testdeck: testdeck.o util.o
	cc -o testdeck testdeck.o util.o $(CCF) -ltserialio -ldmedia

vidsony: vidsony.o
	cc -o vidsony vidsony.o $(CCF) -ltserialio -ldmedia -lvl -laudio -lm
	
tsio: tsio.o util.o
	cc -o tsio tsio.o util.o $(CCF) -ldmedia
