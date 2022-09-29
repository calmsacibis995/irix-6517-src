#
# Makefile for bdftohtp(1).
#
#ident	"$Revision: 1.5 $"

ROOT = /

include ${ROOT}/usr/include/make/commondefs

CFILES = bdftohtp.c xbmtohtpbm.c graytospans.c topcbm.c graytoxbm.c xbmtospans.c
TARGETS = xbmtohtpbm bdftohtp graytospans topcbm graytoxbm xbmtospans

LCINCS = -I$(INCLDIR)/gl
LLDLIBS = -limage -lXmu

default:$(TARGETS)

include ${COMMONRULES}
