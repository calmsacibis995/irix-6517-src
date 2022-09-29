#
#
DIAGDEPTH=../..

# Always build for all.
ALLGFX =LIGHT

include $(DIAGDEPTH)/Makedefs

COMMONPREF = video

_lib ${COMMONTARGS} _plib: ${COMMONPREF}$$@
	@for dirs in $(ALLGFX); do \
		echo "=======\tcd $$dirs; $(MAKE) $@"; \
		(cd $$dirs; exec $(MAKE) $@) \
	done

include ${DIAGDEPTH}/Makerules
