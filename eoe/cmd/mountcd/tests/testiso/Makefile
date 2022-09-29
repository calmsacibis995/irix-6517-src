#!smake
include ${ROOT}/usr/include/make/commondefs

C++FILES=Block.c++ Directory.c++ DirEntry.c++ main.c++ PathTable.c++ util.c++
CFILES=testcd_prot.c
HFILES=Block.h Directory.h DirEntry.h PathTable.h util.h
CFILES=testcd_prot.c
LDIRT=testcd_prot.c testcd_prot.h

TARGETS=testcd

default: ${TARGETS}

include ${COMMONRULES}

testcd: ${CFILES} ${OBJECTS}
	${C++F} -o $@ ${OBJECTS} ${LDFLAGS}

install: default
	${INSTALL} -m 755 -F /usr/etc testcd

depend incdepend: testcd_prot.c testcd_prot.h

testcd_prot.c: testcd_prot.h ../../testcd_prot.x
	${TOOLROOT}/usr/bin/rpcgen -C "${CCF} -E" -c -o $@ ../../testcd_prot.x

testcd_prot.h: ../../testcd_prot.x
	${TOOLROOT}/usr/bin/rpcgen -C "${CCF} -E" -h -o $@ ../../testcd_prot.x
