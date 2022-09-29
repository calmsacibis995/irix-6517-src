#!smake
# $Revision: 1.12 $

include $(ROOT)/usr/include/make/releasedefs

LCINCS = -I$(ROOT)/usr/include/gl
LLDLIBS = -lgl -lm
BASEVERSION=32bit
ALTVERSIONS=N32bit

include $(ROOT)/usr/include/make/cmdcommondefs

CVERSION=-cckr
OPTIMIZER=-g
IDBTAG= -idb "std.sw.unix noship"

COMMANDS = bounce flash winset badifred worms mode cbuf rbuf gsync \
	  rectwrite parallel
OTHERS=runtests killgfxstress

TARGETS+=$(OTHERS)

default all: $(TARGETS)

32bitinstall: all commoninstall
	$(INSTALL) $(IDBTAG) -F /usr/stress/Gfx '$(COMMANDS)' 
	$(INSTALL) $(IDBTAG) -F /usr/stress/Gfx logo.bin

N32bitinstall: all
	@for f in $(COMMANDS); do  \
		$(INSTALL) $(IDBTAG) -F /usr/stress/Gfx -src $$f $$f-N32 ; \
	done

commoninstall: $(OTHERS)
	$(INSTALL) $(IDBTAG) -F /usr/stress/Gfx '$(OTHERS)'

include $(CMDCOMMONRULES)

worms:	worms.o
	$(CC) $(LDOPTS) -o worms worms.o -L$(ROOT)/usr/lib  -lgutil $(LDLIBS)
