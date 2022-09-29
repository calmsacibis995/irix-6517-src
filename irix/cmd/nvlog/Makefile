#ident "$Revision: 1.1 $"

DEPTH = ..

include $(ROOT)/usr/include/make/commondefs

TARGETS=nvlog
CFILES =nvlog.c
C++FILES =

LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

# Directories to search for libraries and the libraries themselves
LLDLIBS = 

# Additional cruft to be removed during make clobber and clean
LDIRT =

# uncomment the following line if there is just a single file being compiled
LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

MACHTAGS = -idb "std.sw.unix" -idb "mach(CPUBOARD=IP25 CPUBOARD=IP21 CPUBOARD=IP19)"

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} $(MACHTAGS) -F /sbin $(TARGETS)
	${INSTALL} $(MACHTAGS) -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)






