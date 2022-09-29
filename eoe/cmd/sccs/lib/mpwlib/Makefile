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
LIB=	../mpwlib.a
CFILES= abspath.c any.c cat.c clean.c dname.c fatal.c fdfopen.c fmalloc.c \
	imatch.c index.c lockit.c logname.c patoi.c rename.c repl.c \
	satoi.c setsig.c sname.c strend.c trnslat.c userexit.c xcreat.c \
	xlink.c xmsg.c xopen.c xpipe.c xunlink.c zero.c

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

$(LIB): $(OBJECTS)
	$(AR) cr $(LIB) $(OBJECTS)
	$(AR) ts $(LIB)
