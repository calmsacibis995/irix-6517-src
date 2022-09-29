#
# Makefile for libsk/fs
#
# This directory contains the standalone filesystems.
# The files in this directory are machine independent.
#
# $Revision: 1.13 $

SKDEPTH=..
include $(SKDEPTH)/libskdefs

#
# Set up to build objects. Even though these files are machine
# independent, objects are kept in machine-dependent .O directories
# to support bi-endian builds.
OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES= bootp.c dvh.c fs.c is_tpd.c tpd.c

include $(LIBSKRULES)
