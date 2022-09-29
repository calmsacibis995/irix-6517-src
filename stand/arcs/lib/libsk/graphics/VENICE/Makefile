# Makefile libsk/graphics/VENICE
#
# This directory contains the graphics driver for Reality Engine
#
# $Revision: 1.4 $

SKDEPTH= ../..
include ${SKDEPTH}/libskdefs

#
# Set up to build machine dependent objects
LLCDEFS= -D$(CPUBOARD)
LIBPATH=$(LIBNAME)
OBJECTDIR=$(TARGETDIR)
LLCINCS=-I$(ROOT)/usr/include/VENICE/gl -I$(ROOT)/usr/include/gl \
	-I$(ROOT)/usr/include/VENICE -I$(ROOT)/usr/include/sys

CFILES= eve_gfx.c init_graphics.c promlib.c venice_init.c \
	venice_tp.c vs2prom.c xilinx.c logo.c fcg_cmdmap.c

VENICEDIRT=fcg_gen_c fcg_cmdmap.c

include $(LIBSKRULES)

clobber: VENICEclobber

VENICEclobber:
	rm -rf $(VENICEDIRT)

fcg_gen_c : fcg_gen_c.c
	$(HOST_CC) -o fcg_gen_c fcg_gen_c.c

fcg_cmdmap.c : fcg_gen_c $(ROOT)/usr/include/VENICE/gl/fcg_cpcmds.B
	./fcg_gen_c $(ROOT)/usr/include/VENICE/gl/fcg_cpcmds.B  fcg_cmdmap.c

