#! smake
# Makefile to build the irix ism images
# There are 3:
#	irix_root - development environment (installed in either / or $ROOT)
#	irix_toolroot - files that go into a TOOLROOT
#	irix_eoe - execution only environment (does not include irix_dev)
#ident "$Revision: 1.5 $"

include $(ROOT)/usr/include/make/ismcommondefs
ISM_NAME=irix
RAWIDB=IDB

SRCIDB=idb

#if defined(OVERLAYIMAGES) && $(OVERLAYIMAGES) == "o256p"
FOOBAR !=cat $(RAWIDB) >> $(RAWIDB).256p
SRCIDB = idb idb.256p
RAWIDB = IDB.256p
#endif

SPEC=spec
SRC!=echo `pwd`/..
LDIRT=IDB.prev
#Turning strip back on.
#LGENDISTFLAGS=-nostrip

#
# names of images in this ism 
# These MUST correspond to the product names in $(SPEC)
#

ALLIMAGES=$(ISM_NAME)_root $(ISM_NAME)_toolroot \
	$(ISM_NAME)_eoe $(ISM_NAME)_noship dba dfs

default install:

#
# The only real rules are 'buildism' & 'buildimages' which are defined
# in ismcommonrules
#
include $(ISMCOMMONRULES)
