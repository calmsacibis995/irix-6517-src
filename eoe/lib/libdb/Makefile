#!smake
#
# SGI Makefile for Berkeley DB
#
############################################
# SEE AN IMPORTANT NOTE IN THE README FILE #
############################################
# "$Revision$"

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/librootdefs

#-----------------------------------------------------------------------
# Leave this here in case we want to switch back.
DISTDIR = db.1.85
# DISTDIR = db.1.86
PORTDIR = $(DISTDIR)/PORT/irix
TESTDIR = $(DISTDIR)/test
COMMONPREF = yes
SRC != pwd

# Temporary: till this bug is fixed. spurious warnings.
# W = -woff 1042
OPT	= -O3 -OPT:Olimit=0 -CG:unique_exit=ON

DSOEXPORTS=common.exports
DSOEXPORTS_N32=common.exports
DSOEXPORTS_64=common.exports

DBSO	= libdb.so

TESTINC		= ../../$(PORTDIR)/include
TEST_CFLAGS	= -I$(TESTINC)
# SHARE = -r
SHARE = -shared

# --- normal object compilation
CC_O32	= $(CC) -32 -mips2 $W -O2
CC_N32	= $(CC) -n32 -mips3 $W $(OPT)
CC_64	= $(CC) -64 -mips3 $W $(OPT)

# --- shared libs creation linking
LLD_O32	= $(LD) -32 -mips2 $(SHARE) -o
LLD_N32	= $(LD) -n32 -mips3 $(SHARE) -o
LLD_64	= $(LD) -64 -mips3 $(SHARE) -o

# --- final linking
LD_O32	= $(CC) -32 -mips2
LD_N32	= $(CC) -n32 -mips3
LD_64	= $(CC) -64 -mips3

DBMAKEFLAGS_O32= CC='$(CC_O32)' OORG='' AR='$(LLD_O32)' LIBDB=$(DBSO)
DBMAKEFLAGS_N32= CC='$(CC_N32)' OORG='' AR='$(LLD_N32)' LIBDB=$(DBSO)
DBMAKEFLAGS_64 = CC='$(CC_64)' OORG='' AR='$(LLD_64)' LIBDB=$(DBSO)

# Target files to build - before install exports
DBLIBS= libdb_o32.so libdb_n32.so libdb_64.so
DBMANS= dbopen.3 hash.3 btree.3 recno.3 mpool.3
DBMANS_z= dbopen.z hash.z btree.z recno.z mpool.z
DBHDRS= db.h mpool.h

TARGETS= $(DBLIBS) $(DBMANS_z) $(DBHDRS)

TEST_DBSO= ../PORT/irix/$(DBSO)

#-----------------------------------------------------------------------
default: $(TARGETS)

install: exports headers manpages

exports: $(DBLIBS)
	# o32 shared libdb
	$(INST_32) -m 444 -idb "nostrip" -src libdb_o32.so libdb.so
	# n32 shared libdb
	$(INST_N32) -m 444 -idb "nostrip" -src libdb_n32.so libdb.so
	# 64bit shared libdb
	$(INST_64) -m 444 -idb "nostrip" -src libdb_64.so libdb.so

headers: $(DBHDRS)
	-for h in $(DBHDRS); do \
		$(INSTALL) -m 444 -F /usr/include -src $$h $$h; \
	done

manpages: $(DBMANS_z)
	$(INSTALL) -m 444 -F /usr/share/catman/p_man/cat3 $(DBMANS_z)


#
# --- Actual library build
#	For each library variant (o32/n32/64) we first tlink
#	the original directory to it so we can build and test
#	them all in parallel.
#
o32/PORT/irix/$(DBSO) : create_irix_links
	@echo === o32: $(DBSO) ===
	tlink -r $(DISTDIR) o32
	cd o32/PORT/irix; $(MAKE) $(DBMAKEFLAGS_O32)

n32/PORT/irix/$(DBSO) : create_irix_links
	@echo === n32: $(DBSO) ===
	tlink -r $(DISTDIR) n32
	cd n32/PORT/irix; $(MAKE) $(DBMAKEFLAGS_N32)

64/PORT/irix/$(DBSO) : create_irix_links
	@echo === 64: $(DBSO) ===
	tlink -r $(DISTDIR) 64
	cd 64/PORT/irix; $(MAKE) $(DBMAKEFLAGS_64)

libdb_o32.so : o32/PORT/irix/$(DBSO)
	cp -f $? $@

libdb_n32.so : n32/PORT/irix/$(DBSO)
	cp -f $? $@

libdb_64.so : 64/PORT/irix/$(DBSO)
	cp -f $? $@

include $(ROOT)/usr/include/make/startversion

include $(COMMONRULES)

#
# Create db links in the DISTDIR directory
#
create_irix_links!
	@echo === DISTDIR=$(DISTDIR)
	cd $(PORTDIR) ; \
	[ -r clib ] || tlink -r ../clib clib ; \
	cd include ; \
	[ -r cdefs.h ] || ln -s ../../include/cdefs.h cdefs.h ; \
	[ -r mpool.h ] || ln -s ../../include/mpool.h mpool.h ; \
	[ -r ndbm.h ] || ln -s ../../include/ndbm.h ndbm.h ; \
	[ -r queue.h ] || ln -s ../../include/queue.h queue.h ; \
	[ -r sys ] || ln -s . sys
	cd $(PORTDIR)/../include; \
	[ -r db.h ] || ln -s ../../include/db.h db.h; \
	[ -r mpool.h ] || ln -s ../../include/mpool.h mpool.h 

clean_irix_links!
	cd $(PORTDIR); rm -rf clib sys; \
	cd include; rm -rf cdefs.h mpool.h ndbm.h queue.h sys
	cd $(PORTDIR)/../include; rm -f db.h mpool.h

# --- Cleaning
clean:
	for d in o32 n32 64 ; do \
		if [ ! -d $$d ]; then continue ; fi; \
		cd $$d; \
		cd test && echo --- `pwd` --- && ${MAKE} clean; \
		cd ../PORT/irix && echo --- `pwd` --- && ${MAKE} clean; \
	done
	make clean_irix_links

clobber: clean
	-/bin/rm -rf $(TARGETS) o32 n32 64 check_o32 check_n32 check_64 $(DBMANS)

# --- Testing
check! check_o32 check_n32 check_64

check_o32! o32/PORT/irix/$(DBSO)
	cd o32/test; $(MAKE) $(DBMAKEFLAGS_O32) PORTDIR=../PORT/irix
	cd o32/test; \
		$(CC_O32) $(TEST_CFLAGS) -c ../../dbhash-bigdata.c; \
		LD_LIBRARY_PATH=../PORT/irix \
		$(LD_O32) -o O2test dbhash-bigdata.o $(TEST_DBSO); \
			LD_LIBRARY_PATH=../PORT/irix ./O2test
	cd o32/test; \
	TMPDIR=. LD_LIBRARY_PATH=../PORT/irix ./run.test && touch ../../$@

check_n32! n32/PORT/irix/$(DBSO)
	cd n32/test; $(MAKE) $(DBMAKEFLAGS_N32) PORTDIR=../PORT/irix
	cd n32/test; \
		$(CC_N32) $(TEST_CFLAGS) -c ../../dbhash-bigdata.c; \
		LD_LIBRARY_PATH=../PORT/irix \
		$(LD_N32) -o O2test dbhash-bigdata.o $(TEST_DBSO); \
			LD_LIBRARY_PATH=../PORT/irix ./O2test
	cd n32/test; \
	TMPDIR=. LD_LIBRARY_PATH=../PORT/irix ./run.test && touch ../../$@

check_64! 64/PORT/irix/$(DBSO)
	cd 64/test; $(MAKE) $(DBMAKEFLAGS_64) PORTDIR=../PORT/irix
	cd 64/test; \
		$(CC_64) $(TEST_CFLAGS) -c ../../dbhash-bigdata.c; \
		$(LD_64) -o O2test dbhash-bigdata.o $(TEST_DBSO); \
			LD_LIBRARY_PATH=../PORT/irix ./O2test
	cd 64/test; \
	TMPDIR=. LD_LIBRARY_PATH=../PORT/irix ./run.test && touch ../../$@

# --- Create the man pages in outside the source tree
.SUFFIXES: .3 .z

.3.z: $(DBMANS)
	nroff -man $*.3 > $*; pack $*

$(DBMANS): $(DISTDIR)/man/$$@
	cp $? .

# --- Create the header files outside the source tree
$(DBHDRS): create_irix_links
	[ -r $@ ] || cp $(DISTDIR)/PORT/irix/include/$@ $@

