#
#
DIAGDEPTH=../..

include $(DIAGDEPTH)/Makedefs

IP20_X=\#
IP22_X=\#
IP26_X=\#
IP28_X=\#
IP30_X=\#
$(CPUBOARD)_X=

$(IP20_X)ALLCOMP=COSMO
$(IP22_X)ALLCOMP=COSMO COSMO20/src
$(IP26_X)ALLCOMP=COSMO 
$(IP28_X)ALLCOMP=COSMO
$(IP30_X)ALLCOMP=COSMO20/src

COMMONPREF = dmedia

_lib ${COMMONTARGS} _plib: ${COMMONPREF}$$@
	@for dirs in $(ALLCOMP); do \
		echo "=======\tcd $$dirs; $(MAKE) $@"; \
		(cd $$dirs; exec $(MAKE) $@) \
	done

include ${DIAGDEPTH}/Makerules
