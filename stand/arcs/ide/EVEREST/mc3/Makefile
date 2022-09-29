#
# Makefile for the IDE libraries.
#
#ident  "arcs/ide/EVEREST/mc3/Makefile $Revision: 1.19 $"
#
DIAGDEPTH=../..
include ${DIAGDEPTH}/Makedefs

ASFILES = misc_s.s kh_dw.s

CFILES= memutils.c memtests.c addru.c cachemem.c connect.c \
	march_x.c march_y.c mem_ecc.c mem_pat.c \
	read_write.c showcfg_reg.c walk_address.c \
	Catalog_mc3.c kh.c 
HFILES=

LINCLUDES=-I../include
#LINCLUDES=-I -I../../../IP19prom
LDEFINES=
#LDEFINES=-DDEBUG -DDEBUG2 -DDEBUG7 -DDEBUG8 -DSMALLMEMSIZE \
#	-DDEBUGPILOT -DDEBUGECC

include ${DIAGDEPTH}/Makerules
# DO NOT DELETE THIS LINE -- make depend depends on it.
