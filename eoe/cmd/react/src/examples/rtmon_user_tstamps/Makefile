#!smake

include $(ROOT)/usr/include/make/commondefs

LLDLIBS= -lrtmon -lc

CFILES= rtmon_user_tstamps.c
EXTRAS= Makefile 
TARGETS= rtmon_user_tstamps
default:	${CFILES:.c=.o}
	$(CCF) -o rtmon_user_tstamps ${CFILES:.c=.o} $(LDOPTS) $(LLDLIBS)

include $(COMMONRULES)

install:	default	
	$(INSTALL) -idb "react.sw.src" -F /usr/share/src/react/examples/rtmon_user_tstamps  ${CFILES} ${HFILES} ${EXTRAS}
	$(INSTALL)  -idb "react.sw.demos" -F /usr/react/bin $(TARGETS)


