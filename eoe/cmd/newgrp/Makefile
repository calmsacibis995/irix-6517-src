#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=newgrp.c
TARGETS=newgrp

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin -u root -g sys $(TARGETS)
	${INSTALL} -ln newgrp -F /usr/bin multgrps
