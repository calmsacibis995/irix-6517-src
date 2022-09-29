#
#
DIAGDEPTH=../..

include $(DIAGDEPTH)/Makedefs

IP20_X=\#
IP22_X=\#
IP22_64X=\#
IP26_X=\#
IP28_X=\#
$(CPUBOARD)_X=
$(CPUBOARD)_$(COMPILATION_MODEL)X=

$(IP20_X)ALLGFX=LIGHT EXPRESS
$(IP22_X)ALLGFX=EXPRESS NEWPORT
$(IP22_64X)ALLGFX=EXPRESS  
$(IP26_X)ALLGFX=EXPRESS 
$(IP28_X)ALLGFX=EXPRESS

ASFILES = utlbasm.s
CFILES = pon.c dmautil.c

COMMONPREF = graphics

_lib ${COMMONTARGS} _plib: ${COMMONPREF}$$@
	@for dirs in $(ALLGFX); do \
		echo "=======\tcd $$dirs; $(MAKE) $@"; \
		(cd $$dirs; exec $(MAKE) $@) \
	done

_core: ${COMMONPREF}$$@

# overide common COBJECTS here for core IDE
CCFILES= pon.c
COBJECTS= ${CCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules
