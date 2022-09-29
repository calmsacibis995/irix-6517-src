#!smake

BASELIB = libns_files_n32.a
include $(ROOT)/usr/include/make/librootdefs

CFILES= aliases.c bootparams.c capability.c clearance.c ethers.c generic.c \
	group.c hosts.c mac.c memory.c netgroup.c networks.c passwd.c \
	protocols.c rpc.c shadow.c services.c wrappers.c dstring.c htree.c
LCOPTS= -fullwarn
LCDEFS= -DNDEBUG -DMAPWAR
LLDLIBS= -lc
LLDOPTS= -woff 33

default all: $(TARGETS)

include $(ROOT)/usr/include/make/librootrules

install: default
	${INSTALL} -u root -g sys -m 644 -F /var/ns/lib \
	    -src libns_files_n32.so libns_files.so
	${INSTALL} -F /var/ns/lib -lns libns_files.so libns_file.so
	${INSTALL} -F /var/ns/lib -lns libns_files.so libns_local.so
