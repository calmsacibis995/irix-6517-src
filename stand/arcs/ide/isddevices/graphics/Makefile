#
#
DIAGDEPTH=../..

include $(DIAGDEPTH)/Makedefs

# should really shut
IP22_X=\#
IP28_X=\#
IP30_X=\#
$(CPUBOARD)_X=
$(CPUBOARD)_$(COMPILATION_MODEL)X=

$(IP22_X)ALLGFX=MGRAS
$(IP28_X)ALLGFX=MGRAS
$(IP30_X)ALLGFX=MGRAS ODYSSEY

COMMONPREF = graphics

_lib ${COMMONTARGS} _plib: ${COMMONPREF}$$@
	@if [ "$(ALLGFX)" != "xxx" ]; then \
	for dirs in $(ALLGFX); do \
		echo "=======\tcd $$dirs; $(MAKE) $@"; \
		(cd $$dirs; exec $(MAKE) $@) \
	done; fi

include ${DIAGDEPTH}/Makerules
