# Makefile
# CREATED BY mpp
#       FROM MakeSkel
#         ON Wed Oct 25 07:27:21 PDT 1995
#


# MakeSkel - makefile skeleton for bsp/src/drv/vio
#
# modification history
# --------------------
# 01d,26sep95,ms   added wdbNetromPktDrv.o to DOC_FILES
# 01c,27jun95,ms   added wdbUlipPktDrv.o and wdbNetromPktDrv.o
# 01b,25may95,jcf  new environment variables
# 01a,09mar95,ms   created from src/drv/serial/MakeSkel
#
# DESCRIPTION
# This file contains the makefile rules for building the vx library
#
# SUBSTITUTION
# The following constant will be substitued:
#     <CPU>             cpu/architecture
#     <TOOL>            tool chain used for build
#
# INCLUDES
#     VX_MAKE/makeLibrary
#*/

DOC_FILES= wdbVioDrv.c wdbUlipPktDrv.c wdbSlipPktDrv.c wdbNetromPktDrv.c

UP=../../../
LIBNAME=lib$(CPU)$(TOOL)vx.a
LIBDIR=$(UP)lib/obj$(CPU)$(TOOL)vx/

OBJS =  wdbVioDrv.o wdbUlipPktDrv.o wdbNetromPktDrv.o wdbSlipPktDrv.o

# makeLibrary - meta makeLibrary stub for building libraries
#
# modification history
# --------------------
# 01a,22sep92,yp    added modification history
#
# DESCRIPTION
# This file contains the makeLibrary and intermediate makeLibrary for VxWorks.
#*/

# makeLibrary1 -  pass one of making libraries
#
# modification history
# --------------------
# 01f,25may95,jcf  new environment variables
# 01e,30sep94,kdl  changed "make" to "$(MAKE)" to use environment variable;
#		   changed Makefile.$(CPU)$(TOOL) rule to use temporary file.
# 01d,22sep92,yp   added EOF marker
# 01c,02sep92,yp   added doc rule
# 01b,27jul92,rrr  added release rule
# 01a,17jul92,rrr  added modification history. fixed dependance of makeLibrary2
#
# DESCRIPTION
# This file contains pass one of building libraries.
#

CPU	= MC68020
TOOL	= gnu
MAKESKEL= MakeSkel
RM	= rm -f

default:	Makefile.$(CPU)$(TOOL) .neverbuilt
	$(MAKE) -f Makefile.$(CPU)$(TOOL) CPU=$(CPU) TOOL=$(TOOL)

clean:		Makefile.$(CPU)$(TOOL) .neverbuilt
	$(MAKE) -f Makefile.$(CPU)$(TOOL) CPU=$(CPU) TOOL=$(TOOL) clean

man:		Makefile.$(CPU)$(TOOL) .neverbuilt
	$(MAKE) -f Makefile.$(CPU)$(TOOL) CPU=$(CPU) TOOL=$(TOOL) man

release:	Makefile.$(CPU)$(TOOL) .neverbuilt
	$(MAKE) -f Makefile.$(CPU)$(TOOL) CPU=$(CPU) TOOL=$(TOOL) release

Makefile.$(CPU)$(TOOL):	MakeSkel $(WIND_BASE)/target/h/make/make.defaults \
    $(WIND_BASE)/target/h/make/make.$(CPU)$(TOOL) $(WIND_BASE)/target/h/make/makeLibrary2 
	$(RM) $@
	$(WIND_BASE)/host/$(WIND_HOST_TYPE)/bin/mpp $(CPU)$(TOOL) \
	    | sed -e "s/<CPU>/$(CPU)/" > $@.tmp
	$(MAKE) -f $@.tmp CPU=$(CPU) TOOL=$(TOOL) depend.$(CPU)$(TOOL)
	$(RM) $@.tmp

.neverbuilt:

# end of makeLibrary1


# end of makeLibrary

