#!smake
#
# $Id: Makefile,v 2.23 1998/12/10 03:40:44 kenmcd Exp $
#
# To link with a DSO PMDA inline, assuming the PMDA "name" is "yyy"
# and the initialization routine is yyy_init() ...
#
#	cd to the correct directory for the style of kernel ... 32, N32 or 64
#	rm pmcd
#	setenv DSOINLINES "-u yyy_init mips_XXX.libyyypmda.so"
#	cp some_place_else/mips_XXX.libyyypmda.so .
#	make
#	elfdump -Dl pmcd		# make sure libyyypmda.so appears
#	nm pmcd | grep yyy_init		# make sure yyy_init appears
#
# To run/debug the resulting pmcd, assuming you have this copied to a
# working directory on the target system
#
#	ln -s some_place_else/mips_XXX.libyyypmda.so libyyypmda.so
#	edit /etc/pmcd.conf and replace the library pathname for the
#		yyy PMDA by a single hyphen, e.g.
#			irix    1       dso     irix_init       -
#		instead of
#			irix    1       dso     irix_init       libirixpmda.so
#	setenv LD_LIBRARY_PATH .
#	./pmcd -f
#	
# This recipe is known to work for execution of pmcd under the control of
# either purify(1) or dbx(1).

TARGETS	= pmcd
CFILES	= pmcd.c config.c dofetch.c dopdus.c util.c dostore.c \
	  client.c agent.c trace.c

PCP_SRC_DEPTH = ../..

include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs
include versiondefs

LCDEFS	= $(DEBUG)
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_dev -lpcp -lgen -llmsgi

#if ! $(BEFORE_IRIX6_5)
# for trace_back_stack()
#
LLDLIBS	+= -lexc
LCDEFS	+= -DHAVE_TRACE_BACK_STACK=1
#endif

default: $(TARGETS)

include $(COMMONRULES)

pmcd:	$(OBJECTS)
	rm -f $@
	$(CCF) $(OBJECTS) $(LDOPTS) -o $@ $(DSOINLINES) $(LDLIBS) $(VERSION_LDLIBS)
	$(TAG) 0x00010D00 $@
