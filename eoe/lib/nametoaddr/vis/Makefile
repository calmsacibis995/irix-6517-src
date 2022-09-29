#!smake

#ident "$Revision: 1.2 $"

DEPTH	= ..
CFILES	= vis.c
LIB	= vis_n32_M3.a

include $(DEPTH)/nametoaddrcommondefs

install exports:	default
	$(INST_N32) -m 444 -src vis_n32_M3.so vis.so
