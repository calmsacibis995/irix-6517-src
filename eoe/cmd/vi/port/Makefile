#!smake
#ident	"$Revision: 1.8 $"
#
# Notes:
#
.MAKEOPTS:-P

BASEVERSION=n32bit
ALTVERSIONS=troot
WANTPARALLEL=yes-please
#
# Common Definitions
#
include	$(ROOT)/usr/include/make/cmdcommondefs

#
# Compile Time Options
#
LCDEFS+=-DCRYPT -DUSG -DSTDIO -DTABS=8 -DSINGLE -DVMUNIX -D_BSD_SIGNALS \
	-D_OLD_TERMIOS

#
# Local Definitions
#
USRBIN=	/usr/bin
USRLIB=	/usr/lib
MAINS=	ex expreserve exrecover 
EXCFILES =  ex.c ex_addr.c ex_cmds.c ex_cmds2.c \
	ex_cmdsub.c ex_data.c ex_extern.c ex_get.c \
	ex_io.c ex_put.c ex_re.c ex_set.c ex_subr.c \
	ex_temp.c ex_tty.c ex_unix.c ex_v.c ex_vadj.c \
	ex_vget.c ex_vmain.c ex_voper.c ex_vops.c \
	ex_vops2.c ex_vops3.c ex_vput.c ex_vwind.c \
	printf.c textmem.c

EXOBJECTS = $(EXCFILES:.c=.o)

CFILES = $(EXCFILES) exrecover.c expreserve.c

default:	$(TARGETS)

include $(CMDCOMMONRULES)

n32bitinstall: default 
	$(INSTALL) -m 01755 -F $(USRBIN) ex
	$(INSTALL) -ln ex -F $(USRBIN) "vi view edit vedit"
	$(INSTALL) -F $(USRLIB) exrecover
	$(INSTALL) -m 02755 -F $(USRLIB) expreserve
	$(INSTALL) -m 0775 -dir /var/preserve
	$(INSTALL) -F /usr -lns ../var/preserve preserve

trootinstall: default
	$(INSTALL) -m 01755 -F $(USRBIN) ex

ex:	$(EXOBJECTS)
	$(CCF) -o $@ $(EXOBJECTS) $(LDFLAGS) $(EXLIBS)

exrecover:	exrecover.o 
	$(CCF) -o exrecover  exrecover.o ex_extern.o textmem.o $(LDFLAGS) -lcrypt
