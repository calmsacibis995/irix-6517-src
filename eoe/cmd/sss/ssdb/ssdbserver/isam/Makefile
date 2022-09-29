#!smake
# Makefile for isam under mysql
#	"$Revision"
#

OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs
include ../../../include/ssscommondefs

SRCDIR = .

LDLIBS =  -nostdlib -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32 \
	-L$(ROOT)/usr/lib32/internal \
	-L. -L../mysys -L../strings  \
	-lnisam -lmysys -lmystrings -lpthread 

LCINCS = -nostdinc -I$(SRCDIR)/../include -I$(SRCDIR)/.. -I../include -I$(ROOT)/usr/include

LCDEFS  = -DDBUG_OFF -DHAVE_CONFIG_H

OPTIMZER = -O

TARGETS = libnisam.a	pack_isam 

CFILES  =  _cache.c _check.c _dbug.c _dynrec.c _key.c _locking.c _packrec.c \
	_page.c _search.c _statrec.c changed.c close.c create.c delete.c \
	extra.c info.c pack_isam.c isamlog.c log.c open.c panic.c range.c \
	rfirst.c rkey.c rlast.c rnext.c rprev.c rrnd.c rsame.c rsamepos.c \
	sort.c static.c update.c write.c

OFILES  = $(CFILES:.c=.o)

LIBISAM_OBJS = open.o extra.o info.o rkey.o rnext.o _search.o \
_page.o _key.o _locking.o rrnd.o _cache.o _statrec.o _packrec.o \
_dynrec.o update.o write.o delete.o rprev.o rfirst.o rlast.o rsame.o \
rsamepos.o panic.o close.o create.o range.o _dbug.o _check.o log.o \
changed.o static.o


COMMONPREF = SSS

$(COMMONPREF)install default: $(TARGETS)
		$(INSTALL) -idb $(SSS_EOE) -F /usr/sbin pack_isam

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

pack_isam: $(OFILES)
	$(CCF) pack_isam.o sort.o $(LDLIBS) -o pack_isam

libnisam.a : $(LIBISAM_OBJS)
		ar ccurl libnisam.a $(LIBISAM_OBJS)

