#!smake
#
#ident	"$Revision: 1.9 $"
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
LIB=	../comobj.a
OBJS = auxf.o chkid.o chksid.o	date_ab.o date_ba.o del_ab.o del_ba.o \
	dodelt.o dofile.o dohist.o doie.o dolist.o encode.o eqsid.o flushto.o \
	fmterr.o getline.o had.o index.o logname.o newstats.o \
	permiss.o pf_ab.o putline.o rdmod.o \
	setup.o sid_ab.o sid_ba.o sidtoser.o sinit.o stats_ab.o

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
