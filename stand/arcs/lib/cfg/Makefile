# Makefile for lib/cfg
#
# This directory contains standalone configuration.
#
# $Revision: 1.11 $

DEPTH= ../..
include $(DEPTH)/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)
include $(CPUARCHDEFS)

DEF_CSTYLE=

# Set up to build machine dependent objects
LLCDEFS=-D$(CPUBOARD)
OBJECTDIR=$(PRODUCT).O
VPATH=$(OBJECTDIR)

# - $(CPUBOARD)conf.cf defines the hinv topology.  Is placed first so it
#	can override the other configuration files if necessary (ie they
#	will not get linked in).
#
# - fsconf.cf the default filesystem configuration, which is cpuboard
#	independent
#
# - deftune.cf: the default tuneable parameters for libsk are defined.
#

CFFILES= $(SCFG).cf fsconf.cf deftune.cf
CFILES= $(CFFILES:.cf=.c)

default: build

include $(DEPTH)/commonrules
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)

build: $(OBJECTDIR) $(CFILES) $(OBJECTS)

LLDIRT= $(ALLCFDIRT) *.O
