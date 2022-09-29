# Makefile for libsk/cmd
#
# This directory contains commands that can only be used by the PROM.
# The files in this directory may be machine dependent.
#
# $Revision: 1.21 $

SKDEPTH=..
include $(SKDEPTH)/libskdefs

#
# Set up to build machine dependent objects
LLCDEFS=-D$(CPUBOARD)
OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES=passwd_cmd.c pmenu_cmd.c spin_cmd.c mem_cmd.c dump_cmd.c endian_cmd.c \
	reboot_cmd.c date_cmd.c play_cmd.c poweroff_cmd.c \
	sn0hinv_cmd.c flash_cmd.c flash_prom_cmd.c ping_cmd.c \
	setpart_cmd.c

include $(LIBSKRULES)
