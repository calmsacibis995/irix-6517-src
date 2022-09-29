#!smake

#ident	"$Revision: 1.6 $"

BASELIB=libfam_32.a
ALTLIBS=libfam_n32_M3.a

NOSO=no-thank-you

include	$(ROOT)/usr/include/make/librootdefs

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
	$(INST_32) -m 444 -src libfam_32.a libfam.a
	$(INST_N32) -m 444 -src libfam_n32_M3.a libfam.a
