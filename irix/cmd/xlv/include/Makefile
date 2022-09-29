#!smake
#ident "$Revision: 1.5 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS = xlv_error.h

default debug install lint: $(TARGETS)

include $(COMMONRULES)

xlv_error.h: ../lib/xlv.err
	awk -f ../lib/xlv_h.awk ../lib/xlv.err > xlv_error.h
