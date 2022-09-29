#!smake

########
# Defs...
#

HFILES = \
	dmalloc.h \
	stacktrace.h \
	$(NULL)
CFILES = \
	dmalloc.c \
	dmalloc_q.c \
	stacktrace.c \
	$(NULL)
ASFILES = \
	stacktrace_s.s \
	$(NULL)

#
# Note, due to bug #308776, everything must be
# in the very first member of _RLD_LIST,
# so we can't just let the subdir make its own .so.
#

#OSPATCHES = ospatches/dmalloc_patches.a
# but there aren't any currently, and ld doesn't like an empty archive...
#OSPATCHES =

LLDLIBS = $(OSPATCHES) -lmpc -lexc $(LIBMLD) -lmangle
# -init is not understood by cc, so encode it with -Wl to get it to ld
LLDOPTS = -Wl,-init,_malloc_init
LCDEFS = -DRELEASE_MAJOR=$(RELEASE_MAJOR)

TARGETS = \
	libdmalloc.a \
	libdmalloc.so \
	libdmalloc.-g.a \
	libdmalloc.-g.so \
	$(NULL)

LDIRT = main *.bak tags

# -mips2 is the default on 6.2 systems, but we want
# to be able to run on R3000 5.3 systems, so make mips1 the default

include $(ROOT)/usr/include/make/releasedefs
include versiondefs
# Keep commondefs from including releasedefs again...
# NO_RELEASEDEFS=
include $(ROOT)/usr/include/make/commondefs

#if !defined(LIBBITSUF) || empty(LIBBITSUF)
#LIBMLD = -lmld
#else
LIBMLD =
#endif

########
# Rules...
#

libdmalloc.a:    $@($(OBJECTS)         ) MAKELIB
libdmalloc.-g.a: $@($(OBJECTS:.o=.-g.o)) MAKELIB

.SUFFIXES: .-g.o .a .so
#
# Rule for making a -g.o from a .c or .s
#
.c.-g.o .s.-g.o:
	$(CCF:N-O*) -g -c $< -o $@
#
# Rule for making a .so from a .c, .s, .o, or .a
#
.c.so .s.so .o.so .a.so: 
	$(CCF) $(LDOPTS) $(LDDSOOPTS) $< $(LDLIBS) $(ENDIAN) -o $@

# ARGH! to keep it from trying to make libc.so from libc.a...
$(ROOT)/usr/lib$(LIBBITSUF)/libc.so:
	just make sure there's a command here, it need not make sense

#
# The links to libmutex (the smallest DSO I could find)
# serves as stubs so that N32 and N64 programs can run
# when _RLD_LIST is set, since there is no N32 or N64 libdmalloc.
#
$(COMMONPREF)install! $(COMMONPREF)default
	$(INSTALL) -idb dmalloc.eoe.dsos -F /usr/lib$(LIBBITSUF) -m 444 -O $(TARGETS:M*.so)
	$(INSTALL) -idb dmalloc.dev.static -F /usr/lib$(LIBBITSUF) -m 444 $(TARGETS:M*.a)
#if !defined(LIBBITSUF) || empty(LIBBITSUF)
	$(INSTALL) -idb dmalloc.dev.headers -F /usr/include -m 444 $(HFILES)
	$(INSTALL) -idb dmalloc.src.dmalloc -m 755 -dir /usr/share/src/dmalloc/src
	$(INSTALL) -idb dmalloc.src.dmalloc -f /usr/share/src/dmalloc/src -m 644 $(SOURCES) Makefile versiondefs
#endif

include $(COMMONRULES)
