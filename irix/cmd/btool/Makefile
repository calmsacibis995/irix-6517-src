include $(ROOT)/usr/include/make/commondefs

LCOPTS=-signed -common
LWOFF=,624
LCINCS=-I. -Iconfig

# NOTE:  We cannot use yacc, c-parse.y uses non-yacc keywords.  
# The output files c-parse.tab.c and cexp.c are delivered with the system.

BISON = ./bison
BISONFLAGS = -v

# Directory in which to put the executable for the command `branch'
BINDIR = /usr/sbin
# Directory in which to put the subprograms used by the compiler.
LIBDIR = /usr/lib/btool
# Directory in which to put man pages.
mandir = /usr/man/local/man1
# Number to put in man-page filename.
manext = l

# Change this to a null string if obstacks are installed in the
# system library.
OBSTACK=obstack.o
ALLOCA = alloca.o

# Dependency on obstack, alloca, malloc or whatever library facilities
# are not installed in the system libraries.
LIBDEPS= $(OBSTACK) $(ALLOCA)
LIBS = $(LIBDEPS)

# Language-specific object files for C.
C_OBJS = c-parse.tab.o c-decl.o c-typeck.o c-convert.o

# generated headers
GENHEADERS=insn-config.h insn-flags.h insn-codes.h

# Language-independent object files.
OBJS = toplev.o version.o tree.o print-tree.o stor-layout.o fold-const.o \
 rtl.o rtlanal.o expr.o stmt.o expmed.o explow.o optabs.o varasm.o \
 symout.o dbxout.o sdbout.o emit-rtl.o insn-emit.o \
 integrate.o jump.o cse.o loop.o flow.o stupid.o combine.o \
 regclass.o local-alloc.o global-alloc.o reload.o reload1.o caller-save.o \
 insn-peep.o final.o recog.o insn-recog.o insn-extract.o insn-output.o

RTL_H = rtl.h rtl.def machmode.def
TREE_H = tree.h real.h tree.def machmode.def

BINS=btool bmerge bnewer bfilter bsummary breport bmatch \
	btool_init cpp kbtool btell

TARGETS=$(BINS) buildcc1

all: $(TARGETS)

COMMONPREF=btool
include $(COMMONRULES)

clean:btoolclean

clobber:clean rmtargets
	-rm -f tm.h aux-output.c config.h md

rmtargets:
	rm -f $(BINS)
	rm -f cc1 cccp bison

$(TARGETS) buildcc1:aux-output.c config.h tm.h bison md

md:config/mips.md
	ln -sf config/mips.md $@
	
aux-output.c:config/out-mips.c
	ln -sf config/out-mips.c $@
	
config.h:config/xm-iris.h
	ln -sf config/xm-iris.h $@
	
tm.h:config/tm-iris.h
	ln -sf config/tm-iris.h $@

bison:bison.uu
	uudecode bison.uu
	chmod 0755 $@
	
btool: gcc.o version.o $(LIBDEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o btool gcc.o version.o $(LIBS)

btool_init: btool_init.sh
	-/bin/rm btool_init
	sed 's=LIBDIR=$(LIBDIR)=' btool_init.sh > btool_init
	chmod a+x btool_init

buildcc1:$(GENHEADERS) cc1

cc1: $(C_OBJS) $(OBJS) $(LIBDEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o cc1 $(C_OBJS) $(OBJS) $(LIBS)

# C language specific files.

c-parse.tab.c : c-parse.y
	BISON_SIMPLE=bison.simple $(BISON) $(BISONFLAGS) c-parse.y -o $@

# Language-independent files.

gcc.o: gcc.c
	$(CC) $(CFLAGS) \
  -DSTANDARD_STARTFILE_PREFIX=\"$(LIBDIR)/\" \
  -DSTANDARD_EXEC_PREFIX=\"$(LIBDIR)/\" -c \
  `echo gcc.c | sed 's,^\./,,'`


# Normally this target is not used; but it is used if you
# define ALLOCA=alloca.o.  In that case, you must get a suitable alloca.c
# from the GNU Emacs distribution.
# Note some machines won't allow $(CC) without -S on this source file.
alloca.o:	alloca.c
	$(CC) $(CFLAGS) -S `echo alloca.c | sed 's,^\./,,'`
	$(ASF) alloca.s -o alloca.o

# Now the source files that are generated from the machine description.

.PRECIOUS: insn-config.h insn-flags.h insn-codes.h \
  insn-emit.c insn-recog.c insn-extract.c insn-output.c insn-peep.c

insn-config.h: md genconfig
	./genconfig md > Tinsn-config.h
	mv Tinsn-config.h insn-config.h

insn-flags.h: md genflags
	./genflags md > Tinsn-flags.h
	mv Tinsn-flags.h insn-flags.h

insn-codes.h: md gencodes
	./gencodes md > Tinsn-codes.h
	mv Tinsn-codes.h insn-codes.h

insn-emit.c: md genemit
	./genemit md > Tinsn-emit.c
	mv Tinsn-emit.c insn-emit.c

insn-recog.c: md genrecog
	./genrecog md > Tinsn-recog.c
	mv Tinsn-recog.c insn-recog.c

insn-extract.c: md genextract
	./genextract md > Tinsn-extr.c
	mv Tinsn-extr.c insn-extract.c

insn-peep.c: md genpeep
	./genpeep md > Tinsn-peep.c
	mv Tinsn-peep.c insn-peep.c

insn-output.c: md genoutput
	./genoutput md > Tinsn-output.c
	mv Tinsn-output.c insn-output.c

# Now the programs that generate those files.

genconfig : genconfig.o rtl.o $(LIBDEPS)
	$(HOST_CC) $(CFLAGS) $(LDFLAGS) -o genconfig genconfig.o rtl.o $(LIBS)

genflags : genflags.o rtl.o $(LIBDEPS)
	$(HOST_CC) $(CFLAGS) $(LDFLAGS) -o genflags genflags.o rtl.o $(LIBS)

gencodes : gencodes.o rtl.o $(LIBDEPS)
	$(HOST_CC) $(CFLAGS) $(LDFLAGS) -o gencodes gencodes.o rtl.o $(LIBS)

genemit : genemit.o rtl.o $(LIBDEPS)
	$(HOST_CC) $(CFLAGS) $(LDFLAGS) -o genemit genemit.o rtl.o $(LIBS)

genrecog : genrecog.o rtl.o $(LIBDEPS)
	$(HOST_CC) $(CFLAGS) $(LDFLAGS) -o genrecog genrecog.o rtl.o $(LIBS)

genextract : genextract.o rtl.o $(LIBDEPS)
	$(HOST_CC) $(CFLAGS) $(LDFLAGS) -o genextract genextract.o rtl.o $(LIBS)

genpeep : genpeep.o rtl.o $(LIBDEPS)
	$(HOST_CC) $(CFLAGS) $(LDFLAGS) -o genpeep genpeep.o rtl.o $(LIBS)

genoutput : genoutput.o rtl.o $(LIBDEPS)
	$(HOST_CC) $(CFLAGS) $(LDFLAGS) -o genoutput genoutput.o rtl.o $(LIBS)


# Making the preprocessor
cpp: cccp
	-rm -f cpp
	ln cccp cpp
cccp: cccp.o cexp.o version.o $(LIBDEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o cccp cccp.o cexp.o version.o $(LIBS)

cexp.c: cexp.y
	BISON_SIMPLE=bison.simple $(BISON) $(BISONFLAGS) -o cexp.c cexp.y

cccp.o: cccp.c
	$(CC) $(CFLAGS) \
          -DGCC_INCLUDE_DIR=\"$(LIBDIR)/gcc-include\" \
          -DGPLUSPLUS_INCLUDE_DIR=\"$(LIBDIR)/g++-include\" \
	  -c `echo cccp.c | sed 's,^\./,,'`


CLEANSTUFF = *.o insn-flags.h insn-config.h insn-codes.h \
 insn-output.c insn-recog.c insn-emit.c insn-extract.c insn-peep.c \
 stamp-flags.h stamp-config.h stamp-codes.h \
 stamp-output.c stamp-recog.c stamp-emit.c stamp-xtrct.c stamp-peep.c \
 c-parse.tab.c c-parse.output \
 genemit genoutput genrecog genextract genflags gencodes genconfig genpeep

LDIRT=$(CLEANSTUFF) stamp-*.[ch] Tinsn-* *.s *.s[0-9] *.co *.greg \
	*.lreg *.combine *.flow *.cse *.jump *.rtl *.tree *.loop \
	*.dbr *.jump2

# Like clean but also delete the links made to configure gcc.
cleanconfig: clean
	-rm -f tm.h aux-output.c config.h md config.status 

# Copy the files into directories where they will be run.
install:
	$(INSTALL) -F $(BINDIR) btool breport bmerge bmatch bnewer \
			bfilter bsummary btool_init kbtool btell
	$(INSTALL) -F $(LIBDIR) cpp
	$(INSTALL) -F $(LIBDIR) cc1
	$(INSTALL) -F $(LIBDIR) btool_lib.c btool_defs.h

installtoolroot:
	ROOT= $(INSTALL) -F $(TOOLROOT)/$(BINDIR) btool breport bmerge bmatch bnewer \
			bfilter bsummary btool_init kbtool btell
	ROOT= $(INSTALL) -F $(TOOLROOT)/$(LIBDIR) cpp
	ROOT= $(INSTALL) -F $(TOOLROOT)/$(LIBDIR) cc1
	ROOT= $(INSTALL) -F $(TOOLROOT)/$(LIBDIR) btool_lib.c btool_defs.h
