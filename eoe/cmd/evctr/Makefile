#!smake
#ident "$Revision: 1.5 $"

include $(ROOT)/usr/include/make/commondefs

CFILES	= evctr.c evctr_util.c ecadmin.c ecstats.c disp.c report.c
TARGETS	= ecadmin ecstats

LCOPTS	= -fullwarn $(INCDIRS)
LLDLIBS	= -lgen

default:	$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -idb "mach(CPUARCH=R10000)" -m 555 -F /usr/etc ecadmin ecstats ecfind

evctr:        evctr.o evctr_util.o
	$(CC) $(COPTS) $(LDOPTS) -o $@ evctr.o evctr_util.o $(LDLIBS)

ecadmin:        ecadmin.o evctr_util.o
	$(CC) $(COPTS) $(LDOPTS) -o $@ ecadmin.o evctr_util.o $(LDLIBS)

ecstats:        ecstats.o evctr_util.o disp.o report.o
	$(CC) $(COPTS) $(LDOPTS) -o $@ ecstats.o evctr_util.o disp.o report.o $(LDLIBS) -lcurses

refresh:
	rm -f procfs.h
	rcp -p babylon.engr:/build/spb/t513/irix/kern/fs/procfs/procfs.h .
	rm -f syssgi.h
	rcp -p babylon.engr:/build/spb/t513/irix/kern/sys/syssgi.h .
	rm -f hwperfmacros.h
	rcp -p babylon.engr:/build/spb/t513/irix/kern/sys/hwperfmacros.h .
	rm -f hwperftypes.h
	rcp -p babylon.engr:/build/spb/t513/irix/kern/sys/hwperftypes.h .
