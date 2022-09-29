#!smake
#
# Makefile for libds.
#
#ident	"$Revision: 1.19 $"

BASELIB=libds.a
ALTLIBS=libds_ns.a libds_n32_ns.a libds_n32.a libds_64_ns.a libds_64.a

include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)

default: $(TARGETS)

include $(LIBROOTRULES)

install: headers exports
	${INSTALL} -F /usr/share/src/irix/examples/scsi/lib src/*.c
	${INSTALL} -F /usr/share/src/irix/examples/scsi/lib/src -m 444 -src src/Makefile.gifts Makefile

exports: default $(_FORCE)
	$(INST_32_NS) -m 444 -idb "nostrip" -src libds_ns.a libds.a
	$(INST_32) -m 444 -idb "nostrip" -src libds.a libds.a
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libds_n32_ns.a libds.a
	$(INST_N32) -m 444 -idb "nostrip" -src libds_n32.a libds.a
	$(INST_64_NS) -m 444 -idb "nostrip" -src libds_64_ns.a libds.a
	$(INST_64) -m 444 -idb "nostrip" -src libds_64.a libds.a

headers: $(_FORCE)
	$(INSTALL) -F /usr/include -m 444 -src src/dslib.h dslib.h 

