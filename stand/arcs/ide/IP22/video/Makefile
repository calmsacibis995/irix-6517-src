#
#
include ../../../commondefs

DIRS=VINO

_lib default install $(COMMONTARGS): ;
	@for i in $(DIRS); do \
		echo "=====\tcd $$i; $(MAKE) $@"; \
		( cd $$i; $(MAKE) $@ ); \
	done
