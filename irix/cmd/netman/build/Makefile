#
# Makefile to build a simple ism for netman.
# There is only one part:
#	netman.sw	- Courtesy binary for pcnfsd plus README.
#
#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/ismcommondefs
ISM_NAME=netman netman_root netman_toolroot snmp_root netman_display netman_data
RAWIDB=IDB
SRCIDB=idb
SPEC=spec
SRC=`pwd`/..

#
# names of images in this ism 
# These MUST correspond to the product names in $(SPEC)
#

ALLIMAGES=$(ISM_NAME)

default install:

#
# The only real rules are 'buildism' & 'buildimages' which are defined
# in ismcommonrules
#
include $(ISMCOMMONRULES)

