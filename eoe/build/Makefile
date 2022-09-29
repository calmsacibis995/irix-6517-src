#! smake
#
# Makefile to build the eoe ism images
# There are 3:
#	eoe_root - development environment (installed in either / or $ROOT)
#	eoe_toolroot - files that go into a TOOLROOT
#	eoe_eoe - execution only environment (does not include irix_dev)
#ident "$Revision: 1.16 $"

include $(ROOT)/usr/include/make/ismcommondefs
ISM_NAME=eoe
RAWIDB=IDB
SRCIDB=idb idb.man IDB.books
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
	$(ISM_NAME)_eoe $(ISM_NAME)_noship dba pcp_eoe_root

default install:

#
# The only real rules are 'buildism' & 'buildimages' which are defined
# in ismcommonrules
#
include $(ISMCOMMONRULES)
