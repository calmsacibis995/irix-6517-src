#!smake
#
#ident	"$Revision: 1.8 $"
#
# Notes:
#

#
# Common Definitions
#
DEPTH=../..
include	$(DEPTH)/slocaldefs

#
# Compile Time Options
#

#
# Local Definitions
#
LIB=	../cassi.a
OBJS=	gf.o cmrcheck.o deltack.o error.o filehand.o

#
# Targets/Rules
#
TARGETS=	$(LIB)

default: $(TARGETS)

include $(COMMONRULES)

#
# Specific Target/Rules follow
#
install: default

$(LIB): $(OBJS)
	$(AR) cr $(LIB) $(OBJS)
	$(AR) ts $(LIB)
