#!smake
#ident "$Revision: 2.10 $"

BASEVERSION=n32bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

LCINCS	+= -I. -I../lib/include  -I../lib/lst
#LCDEFS	= -D_BSD_SIGNALS -DSUNSCCS -DGLOBALJOBS -DMALLOPT
LCDEFS	+= -D_BSD_SIGNALS -DGLOBALJOBS -DMALLOPT
LCOPTS  = -signed -cckr
#LLDLIBS	= -lmalloc
LLDLIBS += -lmalloc -llst -lsprite
# don't whine about -lmalloc
LLDOPTS += -Wl,-woff,85 -L../lib/lst -L../lib/sprite
MLIBS=../lib/lst/liblst.a ../lib/sprite/libsprite.a

CFILES= arch.c compat.c cond.c dir.c make.c job.c main.c parse.c \
	suff.c targ.c str.c var.c rmt.c

INSTALL_DIR = /usr/sbin

COMMANDS = pmake

default all: $(TARGETS)

include $(CMDCOMMONRULES)

pmake: $(OBJECTS) $(MLIBS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

$(MLIBS):
	cd $(@D); $(MAKE) $(@F)

n32bitinstall: default
	$(INSTALL) -F $(INSTALL_DIR) $(COMMANDS)
	$(INSTALL) -F $(INSTALL_DIR) -ln $(COMMANDS) smake
