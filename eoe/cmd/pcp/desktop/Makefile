#!smake
#
# $Id: Makefile,v 1.15 1997/12/11 04:45:29 markgw Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

# For irix build in IRIX 6.5, need only irix desktop stuff
# For pcp build in IRIX 6.5, need only pcp desktop stuff
# For pcp build before 6.5, need both irix and pcp stuff
# also make sure ftr rules are only remade ONCE in exitops!

#if $(ISM_NAME) == "irix"
SUBDIRS	= irix
#elif $(BEFORE_IRIX6_5)
SUBDIRS = pcp irix
#else
SUBDIRS = pcp
#endif

default:
	@for d in $(SUBDIRS); do $(SUBDIR_MAKERULE); done

include $(COMMONRULES)

install: default
	$(SUBDIRS_MAKERULE)
