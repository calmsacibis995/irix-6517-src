#! smake
#*************************************************************************
#
# 		  Copyright 1996 Silicon Graphics, Inc.
#			ALL RIGHTS RESERVED
#
# UNPUBLISHED -- Rights reserved under the copyright laws of the United
# States.   Use of a copyright notice is precautionary only and does not
# imply publication or disclosure.
# 
# U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
# Use, duplication or disclosure by the Government is subject to restrictions
# as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
# in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
# in similar or successor clauses in the FAR, or the DOD or NASA FAR
# Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
# 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
# THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
# INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
# DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
# PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
# GRAPHICS, INC.
# 
#*************************************************************************
#
# Makefile - [describe what this Makefile builds]
#
# $Revision: 1.2 $
#
#*************************************************************************

# Set DEPTH to the path to the ISM top level directoy. For example if
# we are in the cmd subdir below the ISM top level we set DEPTH = ..
#
DEPTH = ../../..

# Include product specific definitions
#
include $(DEPTH)/ismdefs

# The executable to build
TARGETS	= $(OBJECTS)

# Uncomment this if the executable is to be tagged for the destop. You
# will need to ask the desktop group for a tag number for the executable
#DESKTOP_TAG = 

# C source files
CFILES = dhcp_dnsupd.c

# C++ source files
# C++FILES = foobar.c++ morefoobar.c++

# Directories to search for include files - change as needed. Be sure
# to use '+=' instead of '=' so that you pick up definitions from ismdefs
# file
#
LC++INCS += -I.
#LCINCS += -I. -I${ROOT}/usr/freeware/lib/bind/include
LCINCS += -I. -I.. -I${ROOT}/usr/freeware/include/bind

# Directories to search for libraries and the libraries themselves
LLDOPTS = -L${ROOT}/usr/freeware/lib/bind
LLDLIBS = -lbind

# Additional cruft to be removed during make clobber and clean
LDIRT = ../dhcp_dnsupd.o
NOUPDATE = -idb "config(noupdate) EDHCP_EOE"
SUGGEST = -idb "config(suggest) EDHCP_EOE"

# uncomment the following line if there is just a single file being compiled
# LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)


default: $(TARGETS)

include $(COMMONRULES)

install:

# If foo is to be built using C
#foo: $(OBJECTS)
#	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

# If foo is to be built using C++
#foo: $(OBJECTS)
#	$(C++F) $(OBJECTS) $(LDFLAGS) -o $@
# If the executable is to be tagged for the destop, uncomment this
# tag $(DESKTOP_TAG) $@

# Specify an IDB tag that corresponds to one of the IDB tags defined
# in the spec file. Specify an installation directory appropriate for
# the application.
install: default
	$(INSTALL) -m 600 -F /var/dhcp/config ${NOUPDATE} \
		dhcp_ddns.conf

