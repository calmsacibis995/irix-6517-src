#!smake
#
# Makefile for ismname/build to build the ismname isms
#
# IF YOU USE spec.proto THEN:
# Four products are built:
#    fwXXX_ismname        - files to ship to customers
#    FwXXX_ismname - links from /usr/freeware to standard installation places
#    fwXXX_ismname_dev    - files to ship to developers
#    fwXXX_ismname_noship - file for use only at SGI
#
#
# otherwise your Makefile install lines imply a default spec/idb structure
# "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/ismcommondefs
RAWIDB=IDB
SRCIDB=idb
SPEC=spec
LIDBJOINFLAGS=-m
LDIRT= *~

src_default:

#
# The real rules are defined in ismcommonrules, for us buildimages and finalidb

include $(ISMCOMMONRULES)

