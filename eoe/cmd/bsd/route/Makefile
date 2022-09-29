#!smake

# Make the route command
#
#ident $Revision: 1.13 $

include	$(ROOT)/usr/include/make/commondefs

CFILES	=route.c
KWORDS	=keywords.h
LDIRT	=$(KWORDS)

TARGETS	=route

LCOPTS	+=-signed
LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

default:$(TARGETS)

include $(COMMONRULES)

install:default
	$(INSTALL) -m 04555 -u root -F /usr/etc $(TARGETS)

route:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

route.o:$(KWORDS)
$(KWORDS):keywords $(Makefile)
	sed -e '/^#/d' -e '/^$$/d'			\
		-e h					\
		-e y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/ \
		-e x -e G -e 's/\n/	/'  keywords	\
	    |  awk '{					\
		if (NF > 1)				\
			printf "#define\tK_%s\t%d\n\t{\"%s\", K_%s},\n", \
			    $$2, NR, $$1, $$2 }'	\
	    > $@
