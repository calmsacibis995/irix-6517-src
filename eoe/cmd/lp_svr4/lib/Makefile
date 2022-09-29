#
#**************************************************************************
#*                                                                        *
#*            Copyright (c) 1991 Silicon Graphics, Inc.                   *
#*                      All Rights Reserved                               *
#*                                                                        *
#*         THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI             *
#*                                                                        *
#* The copyright notice above does not evidence any actual of intended    *
#* publication of such source code, and is an unpublished work by Silicon *
#* Graphics, Inc. This material contains CONFIDENTIAL INFORMATION that is *
#* the property of Silicon Graphics, Inc. Any use, duplication or         *
#* disclosure not specifically authorized by Silicon Graphics is strictly *
#* prohibited.                                                            *
#*                                                                        *
#* RESTRICTED RIGHTS LEGEND:                                              *
#*                                                                        *
#* Use, duplication or disclosure by the Government is subject to         *
#* restrictions as set forth in subdivision (c)(1)(ii) of the Rights in   *
#* Technical Data and Computer Software clause at DFARS 52.227-7013,      *
#* and/or in similar or successor clauses in the FAR, DOD or NASA FAR     *
#* Supplement. Unpublished - rights reserved under the Copyright Laws of  *
#* the United States. Contractor is SILICON GRAPHICS, INC., 2011 N.       *
#* Shoreline Blvd., Mountain View, CA 94039-7311                          *
#**************************************************************************
#*
#* Makefile - lib, libraries
#*
#* $Revision: 1.6 $
#*
#*
#**************************************************************************

DEPTH = ..

#

LPDEFS = $(DEPTH)/lpdefs

include $(LPDEFS)

#
# If a subdirectory needs to be built it must be in this list.
#
#
SUBDIRS = oam access class filters forms lp lpNet msgs bsd requests \
		secure systems printers users postscript

.PRECIOUS:	$(SUBDIRS)

COMMONPREF = lib_

FSUBDIRS = $(SUBDIRS)

default install $(COMMONTARGS):
	@for d in $(FSUBDIRS); do \
		echo "======\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

include $(COMMONRULES)

$(FSUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

