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
#* Makefile - devpostaps, width tables for APS-5 font set
#*
#*
#*
#**************************************************************************
DEPTH = ../../../..

include $(DEPTH)/lpdefs

TARGETS = B BI CW DESC FONTMAP GR H HB HI HX Hb Hi Hr I PA PB PI PX R S S1

IDB_TAG = eoe2.sw.lp 


SUBDIRS = charlib
.PRECIOUS: $(SUBDIRS)

COMMONPREF = devpostaps_

default install $(COMMONTARGS): $(COMMONPREF)$$@
	@for d in $(SUBDIRS); do\
		echo "=====\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

include $(COMMONRULES)

$(COMMONPREF)default:

# Dont bother installing these files, no body ever tested them.
$(COMMONPREF)install: default

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

