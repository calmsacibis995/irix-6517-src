#
#
DIAGDEPTH=../..

# Always build for all.
ALLCOMP = rad

include $(DIAGDEPTH)/Makedefs

COMMONPREF = dmedia

_lib ${COMMONTARGS} _plib: ${COMMONPREF}$$@
	@for dirs in $(ALLCOMP); do \
		echo "=======\tcd $$dirs; $(MAKE) $@"; \
		(cd $$dirs; exec $(MAKE) $@) \
	done

include ${DIAGDEPTH}/Makerules
