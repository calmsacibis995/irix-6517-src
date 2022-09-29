#
# Makefile for ps.
#
#ident	"$Revision: 1.35 $"

include	${ROOT}/usr/include/make/commondefs

LCDEFS=-D_KMEMUSER -D_SHAREII -DUNS
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
CFILES=ps.c
TARGETS=$(CFILES:.c=)

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -m 2755 -u root -g sys -F /sbin \
		-idb 'postop("rm -rf $$rbase/tmp/.ps_data >/dev/null 2>&1 || exit 0")' \
		${TARGETS}
	${INSTALL} -F /usr/bin -lns ../../sbin/${TARGETS} ${TARGETS}
