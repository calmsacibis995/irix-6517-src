#!smake
#
# Makefile for uts/mips/bsd/sys header file installation.
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/sys
.PATH:$(KPATH)

TARGETS=

LINTNAME=bsdsys
CFILES=

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	${INSTALL} -m 444 -F /usr/include/sys *.h
