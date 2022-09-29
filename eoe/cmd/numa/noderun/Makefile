#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

SHFILES=noderun.sh

TARGETS= noderun

default: $(TARGETS)

noderun: noderun.sh
	cp noderun.sh noderun
	chmod +x noderun

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin noderun
