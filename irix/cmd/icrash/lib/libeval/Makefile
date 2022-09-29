#!smake
#
# Makefile for IRIX crash EVAL library
#
#ident "$Header: "
#

DEPTH?=../..
include ${DEPTH}/icrash_commondefs

GCDEFS    = -DICRASH_DEBUG -DKLIB_DEBUG -DI_TEMP
TARGETS   = ../../libeval.a
CFILES    = eval.c variable.c whatis.c 

OFILES    = $(CFILES:.c=.o)

OSVERS   != export PATH || exit 1 ; echo $${OSVERSION} | \
			sed 'y/\./\_/' | cut -c1-3

COMMONPREF=eval_

all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS): $(OFILES)
	ar ccurl $(TARGETS) $(OFILES)
