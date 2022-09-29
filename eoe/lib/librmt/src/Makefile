#!smake
#
#ident	"$Revision: 1.2 $"
#

DEPTH=.

CFILES= rmtaccess.c rmtclose.c rmtcreat.c \
	rmtfstat.c rmtioctl.c rmtisatty.c rmtlseek.c rmtread.c \
	rmtwrite.c isrmt.c rmtopen.c rmtcommand.c rmtdev.c \
	rmtstatus.c rmtabort.c

SUBDIR_COPTS=-D_SGI_MP_SOURCE

include $(ROOT)/usr/include/make/libleafdefs
include $(LIBLEAFRULES)
