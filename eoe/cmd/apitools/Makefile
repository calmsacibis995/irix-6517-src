#!smake

#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

LLDLIBS=-lmangle

DEM_NAME=exportsdem
DEM_CSRCS=exportsdem.c
DEM_OBJS=$(DEM_CSRCS:.c=.o)

TARGETS=showapi showrefs $(DEM_NAME)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)

$(DEM_NAME):$(DEM_NAME)($(DEM_OBJS)) 
