#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

.MAKEOPTS:-P
TARGETS=gencat
CFILES=gencat.c msg_conv.c cat_misc.c cat_build.c cat_mmp_dump.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin $(TARGETS)

$(TARGETS):$(OBJECTS)
	$(CCF) -o $(TARGETS) $(LDFLAGS) $(OBJECTS)
