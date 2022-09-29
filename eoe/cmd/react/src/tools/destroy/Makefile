#
# Makefile for destroy
#

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs


LLDLIBS= -lm -lfrs

CFILES= destroy.c

CLIENTS= destroy

TARGETS= ${CLIENTS}


default: ${TARGETS}

include ${COMMONRULES}

${CLIENTS}: $$@.c 

install: default
	$(INSTALL) -idb "react.sw.frs" -F /usr/share/src/react/tools/destroy Makefile ${CFILES}
	$(INSTALL) -idb "react.sw.frs" -F /usr/react/bin ${CLIENTS}

${_FORCE}:
