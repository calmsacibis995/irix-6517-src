#!smake
#
# Make the RIP-routed deamon
#
# $Revision: 1.5 $

include	$(ROOT)/usr/include/make/commondefs

DIRS	=tools

CFILES	=if.c input.c main.c md5.c output.c parms.c radix.c rdisc.c table.c \
	trace.c
TARGETS	=routed

LCOPTS	=-signed -fullwarn -D_BSD_SIGNALS -D_BSD_TIME -DUSE_PASSIFNAME

default:$(TARGETS) $(DIRS)

COMMONPREF=routed

include $(COMMONRULES)

$(COMMONTARGS) install:$(COMMONPREF)$$@
	@for i in $(DIRS); do \
		cd $$i; $(MAKE) $@; cd ..; \
	done

$(COMMONPREF)install:default
	$(INSTALL) -F /usr/etc "$(TARGETS)"

$(DIRS):$(_FORCE)
	cd $@; $(MAKE) default

routed: $(OBJECTS) $@.fb
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
	$(CORD) $@ $@.fb
	mv $@.cord $@
