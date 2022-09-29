#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

SHFILES=mkdepend.sh
TARGETS=${SHFILES:.sh=}

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
