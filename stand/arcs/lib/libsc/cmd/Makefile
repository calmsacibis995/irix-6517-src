# Makefile libsc/cmd
# 
# This directory contains commands that can be made available to
# standalone programs.
#
# $Revision: 1.24 $

SCDEPTH=..

include $(SCDEPTH)/libscdefs

#
# Set up to build cpu architecture and endian dependent objects
OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES= auto_cmd.c boot_cmd.c cat_cmd.c check_cmd.c \
	copy_cmd.c env_cmd.c go_cmd.c goto_cmd.c ls_cmd.c \
	memdb_cmd.c mrboot_cmd.c menu_cmd.c \
	dev_cmd.c hinv_cmd.c misc_cmd.c single_cmd.c \
	memlist_cmd.c rb_cmd.c mount_cmd.c \
	echo_cmd.c

include $(LIBSCRULES)
