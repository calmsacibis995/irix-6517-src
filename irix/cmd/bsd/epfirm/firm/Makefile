#!smake

# Makefile for E-Plex 8-port Ethernet firmware
#
# Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
#
#	$Revision: 1.12 $

include ${ROOT}/usr/include/make/commondefs

TDIR	=tools
SUBDIRS	=$(TDIR)

# since the same intermediate ep.o is used for both ep_b.o and ep_c.o
.NOTPARALLEL:

ASFILES	=ep.s ep_flash.s
OBJECTS	=ep_b.o ep_c.o ep_flash.o
LISTS	=$(OBJECTS:.o=.lst)
TARGETS	=$(OBJECTS:.o=.firm)

#give assembler a series of decimal, not octal, numbers for a version
VERS	=`date '+-DVERS_Y=%Y -DVERS_M=%m -DVERS_D=%d -DVERS_H=%H' \
	    | sed -e 's/=0/=/g'`

# These flags are not understood by cc and cause problems for makedepend
# normal listing
AS29FLAGS= $(VERS) -s -l -fP4 -fo -fs -fg
# list everything including #include files
#AS29FLAGS= $(VERS) -s -l -fP4 -fo -fs -fg -fm -fi
# everything including #include files and the 1st pass in case as298 is sick
#AS29FLAGS= $(VERS) -s -l -fP4 -fo -fs -fg -fm -fi -Z

AS	=$(TDIR)/as29i
C2FIRM	=$(TDIR)/coff2firm
LDIRT	=*.lst *.hex

default:sub_firm $(TARGETS)

COMMONPREF=firm
include ${COMMONRULES}

# Nothing here is installed.
install:default

${COMMONTARGS}:${COMMONPREF}$$@ .RECURSIVE
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
	done

$(AS) $(C2FIRM):sub_firm
	@#dummy rule to avoid linking coff2firm here

sub_firm $(SUBDIRS)!.RECURSIVE
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE}"; \
		cd $$d; ${MAKE}; cd ..; \
	done

list:	$(OBJECTS:.o=.lst)

# dependencies, because makedepend does not recognize the non-cpp include
# files used by as29i
$(LISTS) $(OBJECTS):std.h ephwd.h if_ep_s.h ep.h

# rebuild the targets if the assembler changes
$(OBJECTS):$(AS)
$(TARGETS):$(C2FIRM)


.SUFFIXES:.lst .firm .hex

.lst.o:
	@#done by the .lst rule

.s.lst:
	$(AS) $(AS29FLAGS) $< > $*.lst

ep_c.o ep_b.o:ep.s
	rm -f ep.o
	$(AS) $(AS29FLAGS) -D$@ ep.s > $*.lst
	mv -f ep.o $@

.s.o:
	$(AS) $(AS29FLAGS) $< > $*.lst

.o.firm:
	$(C2FIRM) -dc -n $* -t "E-Plex firmware" -b 0x080000 -i $< -o $*.firm

# download the firmware using the STEP box using "yt 20000 : x:ep_flash.hex"
# download the flash using "yt : x:ep.hex"
# execute the flash code manually at 9f800.
# do not forget to "make ep.hex ep_flash.hex"
hex ${COMMONPREF}hex:$(TARGETS:.firm=.hex)
.o.hex:
	$(C2FIRM) -h -b 0x080000 -i $< -o $*.hex
