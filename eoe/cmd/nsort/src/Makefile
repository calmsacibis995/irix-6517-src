#!smake
#
# Makefile for the src directory for Ordinal's nsort program
#
#	64 bit versions are built, both -mips3 and -mips4
#	XXX Is it really okay for no n32 or o32 versions to be distributed
#
#ident "$Revision: 1.3 $"

BASEVERSION=64M3
# Initially have -64 bit versions for mips3 and mips4
# XXX Will there be a justifiable use for 32M4 or o32 versions?
ALTVERSIONS=64M4 # 64dbg 32

# For customer and performance builds the debug ifdefs are disabled
# When compiling debugging versions of the code (e.g. 64dbgversiondefs)
# debug ifdefs are turned on
DEBUGOPTS=

WANTPARALLEL=si,per-piacere

include $(ROOT)/usr/include/make/cmdcommondefs

PROD=nsort
PROD_NAME=Nsort
PROD_VERSION=1.0

# If debugging, use minimal optimization
# If generating 64 bit code, only do -O2 to avoid possible compiler bug
# in last_resort(), exposed by
#	nsort -sp=rec.spec -meth:radix -key=first r6 -o r6.sorted
# 32 bit code can use full optimization
#if $(DEBUGOPTS) != ""
OPTIMIZER=-O1
#elif $(OBJECT_STYLE:M64*) != ""
OPTIMIZER=-O2 -g3
#else
OPTIMIZER=-O3 -g3
#endif

#OPTIMIZER=-O1 -g3

COMMANDS=$(PROD)

LCOPTS+=-fullwarn $(DEBUGOPTS) -DPROD_NAME=$(PROD_NAME)
LLDLIBS+=-lm

# Some more warnings to ignore
# 613: Not supported pragma prefetch_ref
# 1127: loop is not reachable from preceding code (all versions of merge.c)
# 1174: variable "foo" was declared but never referenced (caused by ifdefs)
# 1209:controlling expression is constant (ASSIGN_EOV/OVC() macros)
# 1412: source type of cast is too small to hold all pointers: truncate possible
# 1413: source type of cast is too small to hold all pointers
# 1506: implicit conversion from "long" to "int"
# 1552: variable "win_off" was set but never used (cc bug: it is too used)
LWOFF+=,613,1174,1209,1412,1413,1506,1552

default: $(TARGETS)

include $(CMDCOMMONRULES)

HFILES= otcsort.h parse.h ordinal.h ovc.h record.h merge.h radix.h ordaio.h \
	nsorterrno.h nsort.h
CFILES= build.c build_rec.c build_var.c checksum.c compare.c \
	copy.c copytask.c error.c fakedocancel.c fileinfo.c \
	fixptrrecode.c fixrecrecode.c funcs.c genline.c \
	genlist.c hash.c hashfast.c insertsort.c io.c io_init.c \
	merge_part.c merge_part_ptr.c merge_ptr_keep_out.c \
	merge_ptr_line.c merge_ptr_out.c merge_ptr_skip.c merge_rec_keep_out.c \
	merge_rec_line.c merge_rec_line_8.c merge_rec_out.c merge_rec_out_8.c \
	merge_rec_skip.c merge_rec_skip_8.c nsort.c \
	nrecode.c nrecrecode.c nvarrecode.c ordaio.c ordutils.c \
	parse.c prepare.c prepare2.c print.c process.c radix.c \
	recode.c recrecode.c scan.c sortpart.c sortparthash.c \
	sortparthashrec.c sortparthashvar.c sortpartint.c sortpartrad.c \
	sortpartradrec.c sortpartradvar.c sortpartrec.c sortpartvar.c \
	sortpartvarint.c varrecode.c
OTHERCFILES=chk.c generate.c fakeaio.c merge.c nsortapi.c

# Only 64 bit systems are supported
# XXX What is the proper way to distinguish 64M4 from 64M3 in the spec file?
ARCHMIPS3 = CPUARCH=R4000
ARCHMIPS4 = CPUARCH=TFP CPUARCH=R5000 CPUARCH=R8000 CPUARCH=R10000

clist: $(_FORCE)
	@$(ECHO) $(CFILES)

olist: $(_FORCE)
	@$(ECHO) $(OBJECTS)

AUXFILES=Makefile *versiondefs localdefs localrules

ALLSOURCES= $(CFILES) $(HFILES) $(OTHERCFILES) $(AUXFILES)

TESTS	= runtest testdiff countcheck printdiff tdiff todotest \
	  testall testall.std testmost testmost.std\
	  test4 test4.std test6 test6.std test7 test7.std test8 test8.std \
	  testd testd.std testdh testdh.std testdhp testdhp.std \
	  testr testr.std testb testb.std testb2 testb2.std \
	  testb3 testb3.std testb3r testb3r.std \
	  testsparse testsparse.std testvl testvl.std \
	  testlen testlen.std testlenrec testlenrec.std \
	  all.spec allgen.spec all1M.desc alldec100k.desc \
	  all2.spec allgen2.spec allgen3.spec all3.spec all3s.spec \
	  all250gen.spec all250k.desc all70.spec all500k.desc \
	  streamb.spec stream.desc streams.desc strtiny.desc \
	  varlen.spec varlen.desc varlens.desc vartiny.desc \
	  recgen.spec rec17gen.spec rec.spec rec17.spec r6.desc r17.desc \
	  tiny.desc binstream.spec lvgen.spec lv.spec lv.desc sparse250k.desc \
	  f100.spec f100.desc f8.spec perftest perflen minitest minitest.std \
	  test4d test4d.std testvld testvld.std  testint.c isort.c \
	  mine.desc mine.spec minegen.spec miner.desc miner.spec minergen.spec \
	  rand1M.desc vary varyi varyagg mineperf minerperf

$(PROD): $(OBJECTS) version.c
	$(CCF) $(OBJECTS) version.c $(LDFLAGS) -o $@

64M3install: default
	$(INSTALL) -F /usr/$(PROD)/bin -m 755 -O -idb "dba.sw.$(PROD)" -idb "mach($(ARCHMIPS3) MODE=64bit)" $(PROD)

64M4install: default
	$(INSTALL) -F /usr/$(PROD)/bin -m 755 -O -idb "dba.sw.$(PROD)" -idb "mach($(ARCHMIPS4) MODE=64bit)" $(PROD)

# This target is currently excluded from the shipping image
#
32install: default install_others
	$(INSTALL) -F /usr/$(PROD)/bin -m 755 -O -idb "dba.sw.$(PROD)" $(IDB_TAG32) $(PROD)

#
# prepare2.c cannot be compiled with optimization turned on by the
# 6.4 compilers - "Signal: Segmentation fault in Code_Expansion phase"
# so compile it with only minimal optimization
NOOPTGFLAGS=$(GCDEFS) $(GCINCS) $(CSTYLE) $(ENDIAN) $(MKDEPOPT) $(WOFF)
prepare2.o: prepare2.c
	$(CC) $(CVERSION) $(VCFLAGS) $(LCFLAGS) $(NOOPTGFLAGS) -O1 -c prepare2.c

# version.c is the date of the most recently modified source or header file
#version.c: $(CFILES) $(HFILES) Makefile
#	-rm -f version.c
#	ls -lt $(SOURCES) | head -1 | $(AWK) '{printf "char NsortSourceDate[] = \"$(PROD_VERSION) " $$6 " " $$7 " " $$8 "\";\n"}' > version.c

#
# Ordinal-specific targets, not directly related to SGI builds
sourcedist: $(_FORCE)
	tar cf - $(ALLSOURCES) | gzip -c > ${HOME}/src-`date '+%b-%d'`.gz

tags: $(_FORCE)
	ctags $(CFILES) $(HFILES)

#
#	Many of the cfiles are actually links to the same file (e.g. merge.c)
#	Each symbolic link has to be compiled with its own extra -D definitions
#	in its CC line

build.o: build.c
	$(CCF) -DPOINTER_SORT -c build.c
build_rec.o: build_rec.c
	$(CCF) -DRECORD_SORT -c build_rec.c
build_var.o: build_var.c
	$(CCF) -DVARIABLE_SORT -c build_var.c
fixptrrecode.o: fixptrrecode.c
	$(CCF) -DPOINTER_SORT -c fixptrrecode.c
fixrecrecode.o: fixrecrecode.c
	$(CCF) -DRECORD_SORT -c fixrecrecode.c
hashfast.o: hashfast.c
	$(CCF) -DHASHFAST -c hashfast.c
insertsort.o: insertsort.c
	$(CCF) -DRECORD_SORT -DCONST_EDITED_SIZE=12 \
		-DINSERT_SORT=insert_sort_rs_12 -c insertsort.c
merge_ptr_line.o: merge_ptr_line.c
	$(CCF) -DPTR_LINE -woff 1127 -c merge_ptr_line.c
merge_ptr_out.o: merge_ptr_out.c
	$(CCF) -DPTR_OUT -woff 1127 -c merge_ptr_out.c
merge_ptr_skip.o: merge_ptr_skip.c
	$(CCF) -DPTR_SKIP -woff 1127 -c merge_ptr_skip.c
merge_rec_line.o: merge_rec_line.c
	$(CCF) -DREC_LINE -woff 1127 -c merge_rec_line.c
merge_rec_out.o: merge_rec_out.c
	$(CCF) -DREC_OUT -woff 1127 -c merge_rec_out.c
merge_rec_skip.o: merge_rec_skip.c
	$(CCF) -DREC_SKIP -woff 1127 -c merge_rec_skip.c
merge_rec_line_8.o: merge_rec_line_8.c
	$(CCF) -DREC_LINE -DCONST_EDITED_SIZE=12 -woff 1127 -c merge_rec_line_8.c
merge_rec_out_8.o: merge_rec_out_8.c
	$(CCF) -DREC_OUT -DCONST_EDITED_SIZE=12 -woff 1127 -c merge_rec_out_8.c
merge_rec_skip_8.o: merge_rec_skip_8.c
	$(CCF) -DREC_SKIP -DCONST_EDITED_SIZE=12 -woff 1127 -c merge_rec_skip_8.c
nrecrecode.o: nrecrecode.c
	$(CCF) -DRECORD_SORT -c nrecrecode.c
nvarrecode.o: nvarrecode.c
	$(CCF) -DVARIABLE_SORT -c nvarrecode.c
prepareck.o: prepareck.c
	$(CCF) -DCHECK -c prepareck.c
recrecode.o: recrecode.c
	$(CCF) -DRECORD_SORT -c recrecode.c
sortpart.o: sortpart.c
	$(CCF) -DPOINTER_SORT -c sortpart.c
sortpartrec.o: sortpartrec.c
	$(CCF) -DRECORD_SORT -c sortpartrec.c
sortpartvar.o: sortpartvar.c
	$(CCF) -DVARIABLE_SORT -c sortpartvar.c
varrecode.o: varrecode.c
	$(CCF) -DVARIABLE_SORT -c varrecode.c
merge_ptr_keep_out.o: merge_ptr_keep_out.c
	$(CCF) -DPTR_OUT -DKEEPDUPS -c -woff 1127 merge_ptr_keep_out.c
merge_rec_keep_out.o: merge_rec_keep_out.c
	$(CCF) -DREC_OUT -DKEEPDUPS -c -woff 1127 merge_rec_keep_out.c
sortparthash.o: sortparthash.c
	$(CCF) -DHASHING -DPOINTER_SORT -c sortparthash.c
sortparthashrec.o: sortparthashrec.c
	$(CCF) -DHASHING -DRECORD_SORT -c sortparthashrec.c
sortparthashvar.o: sortparthashvar.c
	$(CCF) -DHASHING -DVARIABLE_SORT -c sortparthashvar.c
sortpartint.o: sortpartint.c
	$(CCF) -DPOINTER_SORT -DINTERNAL_SORT -c sortpartint.c
sortpartrad.o: sortpartrad.c
	$(CCF) -DPOINTER_SORT -DRADIX -c sortpartrad.c
sortpartradrec.o: sortpartradrec.c
	$(CCF) -DRECORD_SORT -DRADIX -c sortpartradrec.c
sortpartradvar.o: sortpartradvar.c
	$(CCF) -DVARIABLE_SORT -DRADIX -c sortpartradvar.c
sortpartvarint.o: sortpartvarint.c
	$(CCF) -DVARIABLE_SORT -DINTERNAL_SORT -c sortpartvarint.c

#
# Make the symbolic links which the above rules use
#
build_rec.c: build.c
	if [ ! -r $@ ] ; then ln -s build.c build_rec.c; fi
build_var.c: build.c
	if [ ! -r $@ ] ; then ln -s build.c $@; fi
recrecode.c: recode.c
	if [ ! -r $@ ] ; then ln -s recode.c $@; fi
varrecode.c: recode.c
	if [ ! -r $@ ] ; then ln -s recode.c $@; fi
sortpartrec.c: sortpart.c
	if [ ! -r $@ ] ; then ln -s sortpart.c $@; fi
sortpartvar.c: sortpart.c
	if [ ! -r $@ ] ; then ln -s sortpart.c $@; fi
sortparthash.c: sortpart.c
	if [ ! -r $@ ] ; then ln -s sortpart.c $@; fi
sortparthashrec.c: sortpart.c
	if [ ! -r $@ ] ; then ln -s sortpart.c $@; fi
sortparthashvar.c: sortpart.c
	if [ ! -r $@ ] ; then ln -s sortpart.c $@; fi
sortpartrad.c: sortpart.c
	if [ ! -r $@ ] ; then ln -s sortpart.c $@; fi
sortpartradrec.c: sortpart.c
	if [ ! -r $@ ] ; then ln -s sortpart.c $@; fi
sortpartradvar.c: sortpart.c
	if [ ! -r $@ ] ; then ln -s sortpart.c $@; fi
sortpartint.c: sortpart.c
	if [ ! -r $@ ] ; then ln -s sortpart.c $@; fi
sortpartvarint.c: sortpart.c
	if [ ! -r $@ ] ; then ln -s sortpart.c $@; fi
merge_rec_line.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
merge_rec_skip.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
merge_rec_out.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
merge_rec_line_8.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
merge_rec_skip_8.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
merge_rec_out_8.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
merge_ptr_line.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
merge_ptr_skip.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
merge_ptr_out.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
merge_ptr_keep_out.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
merge_rec_keep_out.c: merge.c
	if [ ! -r $@ ] ; then ln -s merge.c $@; fi
nrecrecode.c: nrecode.c
	if [ ! -r $@ ] ; then ln -s nrecode.c $@; fi
nvarrecode.c: nrecode.c
	if [ ! -r $@ ] ; then ln -s nrecode.c $@; fi
fixptrrecode.c: fixrecrecode.c
	if [ ! -r $@ ] ; then ln -s fixrecrecode.c $@; fi
prepareck.c: prepare.c
	if [ ! -r $@ ] ; then ln -s prepare.c $@; fi
hashfast.c: hash.c
	if [ ! -r $@ ] ; then ln -s hash.c $@; fi

#
# sort order checking program, not to ship
#
CHECKOBJECTS= chk.o prepareck.o

$(PROD).a: $(OBJECTS)
	ar rsv $@ $?

nchk: $(CHECKOBJECTS) $(PROD).a
	$(CCF) -o $@ $(CHECKOBJECTS) $(PROD).a
