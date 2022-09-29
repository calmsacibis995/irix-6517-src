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
#* Makefile - devpost, width tables for LW Plus font set
#*
#*
#*
#**************************************************************************
DEPTH = ../../../..

include $(DEPTH)/lpdefs

WIDTH_FILES = \
	AB AB.name AI AI.name AR AR.name AX AX.name B B.name BI BI.name  \
	CB CB.name CI CI.name CO CO.name CW CW.name CX CX.name DESC DESC.big \
	DESC.small GR GR.name H H.name HB HB.name HI HI.name HX HX.name \
	Hb Hb.name Hi Hi.name Hr Hr.name Hx Hx.name I I.name KB KB.name \
	KI KI.name KR KR.name KX KX.name LINKFILE NB NB.name NI \
	NI.name NR NR.name NX NX.name PA PA.name PB PB.name PI PI.name \
	PX PX.name R R.name S S.big S.name S.small S1 S1.name VB VB.name \
	VI VI.name VR VR.name VX VX.name ZD ZD.name ZI ZI.name

IDB_TAG = eoe2.sw.lp 


SUBDIRS = charlib
.PRECIOUS: $(SUBDIRS)

COMMONPREF = devpost_

default install $(COMMONTARGS): $(COMMONPREF)$$@
	@for d in $(SUBDIRS); do\
		echo "=====\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

include $(COMMONRULES)

$(COMMONPREF)default:

$(COMMONPREF)install: default
	$(INSTALL) -idb $(IDB_TAG) -u lp -g lp -m 444 \
			-F /usr/lib/font/devpost $(WIDTH_FILES)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

