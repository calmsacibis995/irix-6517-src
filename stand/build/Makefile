#! smake
#
# Makefile to build stand ism images:
#	stand_dev
#	stand_eoe
#
# $Revison$
#

include $(ROOT)/usr/include/make/ismcommondefs
ISM_NAME=stand
RAWIDB=IDB
SRCIDB=idb

#if defined(OVERLAYIMAGES) && $(OVERLAYIMAGES) == "o256p"
FOOBAR !=cat $(RAWIDB) >> $(RAWIDB).256p
SRCIDB = idb idb.256p
RAWIDB = IDB.256p
#endif

SPEC=spec
SRC!=echo `pwd`/..

ALLIMAGES=$(ISM_NAME)_root $(ISM_NAME)_eoe

default install:

include $(ISMCOMMONRULES)
