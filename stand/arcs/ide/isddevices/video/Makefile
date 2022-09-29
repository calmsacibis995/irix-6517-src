# common video cards
#
DIAGDEPTH=../..

include $(DIAGDEPTH)/Makedefs

IP20_X=\#
IP22_X=\#
IP26_X=\#
IP28_X=\#
IP30_X=\#
$(CPUBOARD)_X=

$(IP20_X)ALLVID=GALILEO
$(IP22_X)ALLVID=GALILEO GALILEO15 MCO
$(IP26_X)ALLVID=GALILEO 
$(IP28_X)ALLVID=GALILEO GALILEO15 MCO
$(IP30_X)ALLVID=GALILEO15 MCO

_lib default install $(COMMONTARGS): ;
	@for i in $(ALLVID); do \
		echo "=====\tcd $$i; $(MAKE) $@"; \
		( cd $$i; $(MAKE) $@ ); \
	done

include ${DIAGDEPTH}/Makerules
