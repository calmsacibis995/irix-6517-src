#!smake

# Makefile for firmware for GIO Bus (including DANG) FDDI boards
#
# Copyright 1991,1992 Silicon Graphics, Inc.  All rights reserved.
#
#	$Revision: 1.12 $

PDIR	=..
DEPTH	=../../$(PDIR)
TDIR	=$(PDIR)/tools
include ${DEPTH}/Makedefs
.NOTPARALLEL:

ASFILES	=xpi.s lc_flash.s mez_flash.s
TARGETS	=lc.firm mez_s.firm mez_d.firm lc_flash.firm mez_flash.firm

#give assembler a series of decimal, not octal, numbers for a version
VERS	=`date '+-DVERS_Y=%Y -DVERS_M=%m -DVERS_D=%d -DVERS_H=%H \
	    -DVERS_MM=%M' | sed -e 's/=0/=/g'`

# These flags are not understood by cc and cause problems for makedepend
# normal listing
AS29FLAGS=$(VERS) -s -l -fP4 -fo -fs -fg
# list everything including #include files
#AS29FLAGS=$(VERS) -s -l -fP4 -fo -fs -fg -fm -fi
# everything including #include files and the 1st pass in case as298 is sick
#AS29FLAGS=-$(VERS) -s -l -fP4 -fo -fs -fg -fm -fi -Z

AS	=$(TDIR)/as29i
C2FIRM	=$(TDIR)/coff2firm
LDIRT	=*.lst *.hex

# remember to run the makefile in the parent directory to build the tools
default:$(TARGETS)

include ${COMMONRULES}

# Nothing here is installed.
install:default

list:	$(OBJECTS:.o=.lst)

# Dependencies, because makedepend does not recognize the non-cpp include
#   files used by as29i
BASE_HDRS=std.h gio.h if_xpi_s.h
lc_flash.lst lc_flash.o:$(BASE_HDRS)
mez_flash.lst mez_flash.o:$(BASE_HDRS)

HDRS=$(BASE_HDRS) xpi.h
lc.lst lc.o:$(HDRS)
mez_d.lst mez_d.o:$(HDRS)
mez_s.lst mez_s.o:$(HDRS)

# rebuild the targets if the assembler changes
$(OBJECTS):$(AS)
$(TARGETS):$(C2FIRM)


.SUFFIXES:.lst .firm .hex

.lst.o:
	@#done by the .lst rule

.s.lst:
	$(AS) $(AS29FLAGS) $< > $*.lst

mez_d.o mez_s.o lc.o:xpi.s
	rm -f xpi.o
	$(AS) $(AS29FLAGS) -D$@ xpi.s > $*.lst
	mv -f xpi.o $@

.s.o:
	$(AS) $(AS29FLAGS) $< > $*.lst

.o.firm:
	$(C2FIRM) -dc -n $* -b 0x400000 -i $< -o $*.firm
	rm -f $(PDIR)/$*.firm; ln $*.firm $(PDIR)

# download the firmware .hex file using the STEP box by "yt 20000 : file"
# download the flash .hex file using "yt : file".  Execute the flash
# code manually at 41f800.
.o.hex:
	$(C2FIRM) -h -b 0x400000 -i $< -o $*.hex
