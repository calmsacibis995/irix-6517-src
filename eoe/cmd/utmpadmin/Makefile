#ident	"$Revision: 1.5 $"

include	${ROOT}/usr/include/make/commondefs

TARGETS= viutmp.sher viutmp.irix4 
CFILES = viutmp.c
LCOPTS=-fullwarn

default: chkutent

include $(COMMONRULES)

chkutent: chkutent.c
	$(CCF) -O -o chkutent chkutent.c

viutmp.sher: $(CFILES)
	$(CCF) -O -o viutmp.sher viutmp.c

viutmp.irix4: $(CFILES)
	$(CCF) -D_IRIX_4 -o viutmp.irix4 viutmp.c

install: chkutent 
	${INSTALL} -F /usr/sbin -m 0500 -u root -g sys chkutent
