#!smake

BASELIB = libns_ldap_n32.a
include $(ROOT)/usr/include/make/librootdefs

LCOPTS=

# SSL
# CFILES= bind.c search.c result.c lookup.c init.c
# LCDEFS= -DUNS -D_SSL -I../ssl -I../bsafe_n32
# LLDLIBS= -L../ssl -L../bsafe_n32 -lssl -lbsafe -ldb -lc

# NO SSL
CFILES= bind.c search.c result.c lookup.c init.c lber.c
LCDEFS= -DUNS
LLDLIBS= -lc

OPTIMIZER= -g

default all: $(TARGETS)

include $(ROOT)/usr/include/make/librootrules

install: default
	${INSTALL} -u root -g sys -m 644 -F /var/ns/lib -src libns_ldap_n32.so \
	    libns_ldap.so
	${INSTALL} -u root -g sys -m 600 -F /var/ns -idb "config(suggest)" ldap.conf
	${INSTALL} -u root -g sys -m 755 -F /var/ns ldif_parse

exports: default
	${INSTALL} -F /usr/lib32 -src libns_ldap_n32.a libns_ldap.a
