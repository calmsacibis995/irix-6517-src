# Makefile for Interphase 4211 Peregrine FDDI firmware
#
# Copyright 1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
#
#	$Revision: 1.12 $


include ${ROOT}/usr/include/make/commondefs
SUBDIRS=tools
.NOTPARALLEL:

ASFILES	=prom.s eprom.s ipg.s

TARGETS	=prom.hex eprom.hex ipg.firm

# These flags are not understood by cc and cause problems for makedepend
# normal listing
AS29FLAGS=-s -l -fP4 -fo -fs -fg
# list everything including #include files
#AS29FLAGS=-s -l -fP4 -fo -fs -fg -fm -fi

TDIR	=tools
AS	=$(TDIR)/as29i
C2FIRM	=$(TDIR)/coff2firm
LDIRT	=*.lst

#build the tools first
#ifndef SRC_PRD
default branch:subdirs $(TARGETS)
#else
default:
#endif

COMMONPREF=fware
include ${COMMONRULES}

# Nothing here is installed.
install:default

${COMMONTARGS}:${COMMONPREF}$$@ ${_FORCE}
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
	done

subdirs $(AS) $(C2FIRM):
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE}"; \
		cd $$d; ${MAKE}; cd ..; \
	done

list:	$(OBJECTS:.o=.lst)

# dependencies, because makedepend does not recognize the non-cpp include
# files used by as29i
prom.lst prom.o:    prom.s std.h hard.h eprom.h
eprom.lst eprom.o:  eprom.s std.h hard.h eprom.h
ipg.lst ipg.o:    ipg.s std.h hard.h eprom.h if_ipg_s.h ipg.h

# rebuild the targets if the assembler changes
$(OBJECTS):$(AS)
ipg.firm:$(C2FIRM)
prom.hex:$(C2FIRM) prom.o

eprom.hex:$(C2FIRM) eprom.o
	$(C2FIRM) -xhC -i $< -o $*.hex
	


.SUFFIXES:.lst .firm .hex

.lst.o:
	@#done by the .lst rule

.s.lst:
	$(AS) $(AS29FLAGS) $< > $*.lst

.s.o:
	$(AS) $(AS29FLAGS) $< > $*.lst

.o.firm:
	$(C2FIRM) -xC -n $* -i $< -o $*.firm

.o.hex:
	$(C2FIRM) -xh -i $< -o $*.hex
