#!smake
#
# Makefile for /usr/include/arcs header file installation.
#

DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/sys/arcs
.PATH:$(KPATH)

FRIEND_HEADERS= debug_block.h dirent.h fat.h hinv.h io.h large.h \
	pvector.h signal.h spb.h time.h tvectors.h types.h kerncb.h

include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:


headers!
	${INSTALL} -m 444 -F /usr/include/arcs $(FRIEND_HEADERS) 
