#!smake
# Make BSD sockets and IP suite
#
# $Revision: 3.37 $

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd
.PATH:$(KPATH)

SUBDIRS=net netinet netns socket misc mips uds sesmgr vsock
HEADERS_SUBDIRS=net netinet netns sys misc sesmgr

BSD_NAME=bsd.a

BSD_ALTS=socket.a
${BSD_ALTS}:${BSD_NAME} MAKEALTERNATE

NONALTTARGETS=$(BSD_NAME)
ALTTARGETS=$(BSD_ALTS)
TARGETS=$(NONALTTARGETS) $(ALTTARGETS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${BSD_NAME}
	${KNOMODEINSTALL} -lns ${BSD_NAME} ${BSD_ALTS}
	${KDEVINSTALL} ${BSD_NAME}
	${KNOMODEDEVINSTALL} -lns ${BSD_NAME} ${BSD_ALTS}

# install header files under ${ROOT}/usr/include
headers!
	${INSTALL} -m 444 -F /usr/include/sys tcp-param.h
	${HEADERS_SUBDIRS_MAKERULE}
