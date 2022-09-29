#!smake
#ident "$Revision: 1.128 $"

#
# According to the Generic ABI, the libc shared object is libc.so.1.
# However in this Makefile, the target libc shared object is libc.so and
# it is installed as libc.so.1. The target of libc.so permits make(1) to
# use the .a.so inference rule to generate the shared object.
#

BASELIB=libc.a
ALTLIBS=libc_32_M2_ns.a libc_nomp_32_M2_ns.a \
	libc_32_abi.a \
	libc_G0_noso_32.a \
	libc_64_M3.a libc_64_M3_ns.a \
	libc_64_M4.a libc_64_M4_ns.a \
	libc_64_abi.a \
	libc_n32_M3.a libc_n32_M3_ns.a libc_nomp_n32_M3_ns.a \
	libc_n32_M4.a libc_n32_M4_ns.a \

DSOEXPORTS=common.exports.new
DSOEXPORTS_N32=$(DSOEXPORTS)
DSOEXPORTS_32_ABI=abi.exports
DSOEXPORTS_64=$(DSOEXPORTS)
DSOEXPORTS_64_ABI=abi.exports

ABISO=yes-please

PRIVATEFILES=-x '^.*\.x$$'
#
# This is the list of objects containing frequently written variables.  It
# was generated using "solocal -m -Alibc.a libc.so.1" with the following
# adjustments:
#	Added hot hidden files to the list:
#		atexit.o dial.o, _loc_data.o, getopt.o, xdr.o drand48.o
# 	Some of the above files have their data re-written with the same
#	value they are initted with, thus confusing solocal.
#	Bump up getpty.o and putenv.o since used by 'xwsh'
#	Always put data.o, sibuf.o, and sobuf.o at the end because
#		iob[], smbuf, sibuf, and sobuf are so big!!!
#
HIGHUSAGEOBJS=\
	getopt.o xdr.o _loc_data.o \
        issubdir.o sleep.o regex.o rand.o crypt.o res_init.o \
        inet_ntoa.o _locale.o ns_services.o sesmgr_t6net.o popen.o \
        tmpnam.o oserror.o random.o import_def.o mp_def.o semlibc.o usinit.o \
        cuexit.o atexit.o tempnam.o ctime.o gtxt.o getpty.o auth_none.o \
        getutx.o bindresvport.o yp_bind.o opt_data.o ns_shadow.o mntent.o \
        syslog.o ns_hosts.o ns_group.o strtok.o ns_passwd.o time_comm.o \
        time_gdata.o putenv.o setlocale.o cap_acquire.o cerror.o ns_proto.o \
        sigsetops.o blk_init.o flush.o malloc.o execv.o sbrk.o  \
	data.o sibuf.o sobuf.o




include $(ROOT)/usr/include/make/librootdefs

# Add lint and old static shared libs
TARGETS+=llib-lc.ln

# Add compatibility libmpc libs
#if !empty(STYLES_TO_BUILD:M32*)
TARGETS+=libmpc.a
#endif
#if !empty(STYLES_TO_BUILD:M64*)
TARGETS+=libmpc_64_M3.a libmpc_64_M4.a
#endif
#if !empty(STYLES_TO_BUILD:MN32*)
TARGETS+=libmpc_n32_M3.a libmpc_n32_M4.a
#endif

LLDDSOOPTS=-rdata_shared -exclude _rld_new_interface.o -e __exec_rld  -LD_LAYOUT:segalign=0x4000
DSONAMEOPT=-soname libc.so.1
DSOTARGOPT=libc.so # only for the test rule
LDIRT=so_locations common.exports.new \
	libc.so.precord libc_n32_M3.so.precord libc_n32_M4.so.precord \
	libc_64_M3.so.precord libc_64_M4.so.precord \
	libcdso.a

# don't turn on -no_unresolved for these libraries
LD_NO_UNRESOLVED=

INSOPT=-m 444 -idb "nostrip"
INSOPT4=-idb "mach(CPUARCH=R4000)"
INSOPT8=-idb "mach(CPUARCH=R8000 CPUARCH=R10000 CPUARCH=R5000)"
INSOPT8_64=-idb "mach(CPUARCH=R8000 CPUARCH=R10000 MODE=64bit)"

#
# Set CSTYLE to NULL so that any .c's compiled in this directory require
# full style info
#
CSTYLE=

SHOWREFS=${TOOLROOT}/usr/sbin/showrefs

default!$(TARGETS)
	chmod 0755 libc.so

llib-lc.ln:	llib-lc.c
	$(LINT) -Y$(ROOT)/usr/include -vxd -Xa -oc -n llib-lc.c

#
# We seem to be having problems with data and text placement ...
# This will quickly check for problems
# Also do name space sanity checking
# There should be only 2 undefined variables _end and _rld_new_interface
#

sane :

	@echo "\tPerforming data/text check" ;\
	for i in lib*.so ; do \
		./data_ck $$i ;\
	done
	@echo "\tPerforming name space check" ;\
	for i in lib*.so ; do \
		if test `${SHOWREFS} $$i | wc -l` != 0 ; then \
			echo "$$i has name space pollution" ;\
			exit 2 ;\
		fi ;\
	done

# Ignore special SHOWREFS symbols which appear unresolved
	@echo "\tPerforming undefined variable check" ;\
	for i in lib*.so ; do \
		if test "$$i" = "libc_32_abi.so" -o "$$i" = "libc_64_abi.so"; then \
			continue ;\
		fi ;\
		sr_ret=`${SHOWREFS} -u $$i\
		  | egrep -v\
		    '__elf_header|__program_header_table|__dso_displacement'\
		  | wc -l`;\
		if  test $$sr_ret != 2 -a $$sr_ret != 1 ; then \
			echo "$$i has undefined variables" ;\
			exit 2 ;\
		fi ;\
	done
#		echo "The value of sr_ret is "$$sr_ret;\
#
# N.B. Please update the dso! rule below with any changes you make here
# All 3 rules are the same except for LDOPTS; not worth the
# restructuring to make it one rule only.
#
# Since most folks just build a few libc's add check here for
# name space pollution so lots of folks see it.
#

$(DSOEXPORTS): common.exports src/iconv/iconv.exports
	cat common.exports src/iconv/iconv.exports > $(DSOEXPORTS)

src/iconv/iconv.exports: src/computed_include/matrix.c src/iconv/stdlib_top_src.m4
	( cd src/iconv; make iconv.exports )

libc.so: $(.TARGET:.so=.a) libc.so.1.fb 
	$(AR) xl $(@:.so=.a) $(HIGHUSAGEOBJS)
	$(AR) d $(@:.so=.a) $(HIGHUSAGEOBJS)
	$(AR) ccrl $(@:.so=.a) $(HIGHUSAGEOBJS)
	rm -f $(HIGHUSAGEOBJS)
	$(LD) $(LDOPTS_32) $(LDDSOOPTS_32) $(@:.so=.a) -o $@.precord
	# use 'cord' to reorder the DSO to optimize text page usage
	$(CORD) -o $@ $@.precord libc.so.1.fb
	@echo "\tPerforming name space check" ;\
	if test `${SHOWREFS} $@ | wc -l` != 0 ; then \
		echo "$@ has name space pollution" ;\
		exit 2 ;\
	fi

libc_n32_M3.so: $(.TARGET:.so=.a) libc.so.1.fb
	$(AR) xl $(@:.so=.a) $(HIGHUSAGEOBJS)
	$(AR) d $(@:.so=.a) $(HIGHUSAGEOBJS)
	$(AR) ccrl $(@:.so=.a) $(HIGHUSAGEOBJS)
	rm -f $(HIGHUSAGEOBJS)
	$(LD) $(LDOPTS_N32_M3) $(LDDSOOPTS_N32) $(@:.so=.a) -o $@.precord
	# use 'cord' to reorder the DSO to optimize text page usage
	$(CORD) -o $@ $@.precord libc.so.1.fb
	@echo "\tPerforming name space check" ;\
	if test `${SHOWREFS} $@ | wc -l` != 0 ; then \
		echo "$@ has name space pollution" ;\
		exit 2 ;\
	fi

libc_n32_M4.so: $(.TARGET:.so=.a) libc.so.1.fb
	$(AR) xl $(@:.so=.a) $(HIGHUSAGEOBJS)
	$(AR) d $(@:.so=.a) $(HIGHUSAGEOBJS)
	$(AR) ccrl $(@:.so=.a) $(HIGHUSAGEOBJS)
	rm -f $(HIGHUSAGEOBJS)
	$(LD) $(LDOPTS_N32_M4) $(LDDSOOPTS_N32) $(@:.so=.a) -o $@.precord
	# use 'cord' to reorder the DSO to optimize text page usage
	$(CORD) -o $@ $@.precord libc.so.1.fb
	if test `${SHOWREFS} $@ | wc -l` != 0 ; then \
		echo "$@ has name space pollution" ;\
		exit 2 ;\
	fi

libc_64_M3.so: $(.TARGET:.so=.a) libc.so.1.fb
	$(AR) xl $(@:.so=.a) $(HIGHUSAGEOBJS)
	$(AR) d $(@:.so=.a) $(HIGHUSAGEOBJS)
	$(AR) ccrl $(@:.so=.a) $(HIGHUSAGEOBJS)
	rm -f $(HIGHUSAGEOBJS)
	$(LD) $(LDOPTS_64_M3) $(LDDSOOPTS_64) $(@:.so=.a) -o $@.precord
	# use 'cord' to reorder the DSO to optimize text page usage
	$(CORD) -o $@ $@.precord libc.so.1.fb
	@echo "\tPerforming name space check" ;\
	if test `${SHOWREFS} $@ | wc -l` != 0 ; then \
		echo "$@ has name space pollution" ;\
		exit 2 ;\
	fi

libc_64_M4.so: $(.TARGET:.so=.a) libc.so.1.fb
	$(AR) xl $(@:.so=.a) $(HIGHUSAGEOBJS)
	$(AR) d $(@:.so=.a) $(HIGHUSAGEOBJS)
	$(AR) ccrl $(@:.so=.a) $(HIGHUSAGEOBJS)
	rm -f $(HIGHUSAGEOBJS)
	$(LD) $(LDOPTS_64_M4) $(LDDSOOPTS_64) $(@:.so=.a) -o $@.precord
	# use 'cord' to reorder the DSO to optimize text page usage
	$(CORD) -o $@ $@.precord libc.so.1.fb
	if test `${SHOWREFS} $@ | wc -l` != 0 ; then \
		echo "$@ has name space pollution" ;\
		exit 2 ;\
	fi

include ${LIBROOTRULES}

install!headers exports
	$(INST_32_M2_NS) -m 444 -src README.nonshared README

#
# Split up the install rules so that its possible for developers to install
# stuff w/o having to build everything
#
# We install the shared libc.a's for SpeedShop so they can do a -Bsymbolic.
# These are NOT shipped.
#
exports!default
	# lint library & message catalogues
	#
	$(INSTALL) -F /usr/lib/lint -src llib-lc.c llib-lc
	$(INSTALL) -F /usr/lib/lint llib-lc.ln
	$(INSTALL) -F /usr/lib/locale/C/MSGFILES -m 444 \
			-src src/gen/uxsgierr_msg.src uxsgierr_msg.src
	$(INSTALL) -F /usr/lib/locale/C/MSGFILES -m 444 \
			-src src/gen/uxsyserr_msg.src uxsyserr_msg.src
	$(MAKE) doinstalls
	$(MAKE) sane

#
# None of the ins*_M4_* are installed since 
# we use the dependices to force ins*_M4_* to
# be done first since the build env should have
# ins*_M3_* as the default libs in :
#	/usr/lib32 and /usr/lib32/nonshared
#	/usr/lib64 and /usr/lib64/nonshared
#
doinstalls!ins32_s ins32_ns ins32_abi \
		ins64 ins64_M3_s ins64_M3_ns ins64_abi \
		insn32 insn32_M3_s insn32_ns insn32_M3_ns

#
# 32 BIT MIPS2 SHARED
#
ins32_s!
	#
	# 32 BIT MIPS2 SHARED
	#
	$(INST_32_M2) -m 444 libmpc.a
	$(INST_32_M2) -lns crt1.o acrt1.o
	$(INST_32_M2) -m 444 -src src/csu/crt1.o crt1.o
	$(INST_32_M2) -m 444 -src src/csu/crtn.o crtn.o
	$(INST_32_M2) -m 444 -src src/csu/mcrt1.o mcrt1.o
	$(INSTALL) $(INSOPT) -F /usr/SpeedShop$(INSTDIR_32_M2) -src libc.a libc.a
	$(I_32_M2) $(INSTALL) -F /lib  -m 1555 -src libc.so libc.so.1
	$(INST_32_M2) -lns ../../lib/libc.so.1 libc.so.1
	$(INST_32_M2) -lns ../../lib/libc.so.1 libc.so

#
# 32 BIT MIPS2 NONSHARED
#
ins32_ns!
	#
	# 32 BIT MIPS2 NONSHARED
	#
	$(INST_32_M2_NS) -lns crt1.o acrt1.o
	$(INST_32_M2_NS) $(INSOPT) -src libc_32_M2_ns/csu/crt1.o crt1.o
	$(INST_32_M2_NS) $(INSOPT) -src libc_32_M2_ns/csu/crtn.o crtn.o
	$(INST_32_M2_NS) $(INSOPT) -src libc_32_M2_ns/csu/mcrt1.o mcrt1.o
	$(INST_32_M2_NS) -m 444 -src libc_32_M2_ns.a libc.a
	# special versions
	$(INST_32_M2_NS) -m 444 -src libc_G0_noso_32.a libc_G0.a
	$(INST_32_M2_NS) -m 444 -src libc_nomp_32_M2_ns.a libc_nomp.a

# 32 bit ABI objects
ins32_abi!
	#
	# 32 bit ABI objects
	#
	$(INST_32_ABI) -lns crt1.o acrt1.o
	$(INST_32_ABI) $(INSOPT) -src libc_32_abi/csu/crt1.o crt1.o
	$(INST_32_ABI) $(INSOPT) -src libc_32_abi/csu/crtn.o crtn.o
	$(INST_32_ABI) $(INSOPT) -src libc_32_abi/csu/mcrt1.o mcrt1.o
	$(INST_32_ABI) -src libc_32_abi.so libc.so

#
# 64 BIT VERSIONS
#
ins64!
	#
	# 64 BIT VERSIONS
	#
	# Generic link between libc.so and libc.so.1
	$(INST_64) -lns libc.so.1 libc.so

#
# 64 BIT MIPS3 SHARED
#
ins64_M3_s! ins64_M4_s
	#
	# 64 BIT MIPS3 SHARED
	#
	$(INST_64_M3) -m 444 -src libmpc_64_M3.a libmpc.a
	$(INST_64_M3) $(INSOPT) -src libc_64_M3/csu/crt1.o crt1.o
	$(INST_64_M3) $(INSOPT) -src libc_64_M3/csu/crtn.o crtn.o
	$(INST_64_M3) $(INSOPT) -src libc_64_M3/csu/mcrt1.o mcrt1.o
	$(INSTALL) $(INSOPT) -F /usr/SpeedShop$(INSTDIR_64_M3) -src libc_64_M3.a libc.a
	$(INST_64_M3) -m 1555 -src libc_64_M3.so libc.so.1
	$(INST_64_M3) -lns libc.so.1 libc.so
	#
	# 64 BIT MIPS3 SHARED - symlinks per CPUARCH & MODE
	#
	$(I_64_M3) (cd libc_64_M3; $(INST_64) $(INSOPT4) -lns mips3/libc.so.1 libc.so.1)
	$(I_64_M3) (cd libc_64_M3; $(INST_64) $(INSOPT4) -lns mips3/crt1.o crt1.o)
	$(I_64_M3) (cd libc_64_M3; $(INST_64) $(INSOPT4) -lns mips3/crtn.o crtn.o)
	$(I_64_M3) (cd libc_64_M3; $(INST_64) $(INSOPT4) -lns mips3/mcrt1.o mcrt1.o)

#
# 64 BIT MIPS3 NONSHARED
#
ins64_M3_ns! ins64_M4_ns
	#
	# 64 BIT MIPS3 NONSHARED
	#
	$(INST_64_M3_NS) $(INSOPT) -src libc_64_M3_ns/csu/crt1.o crt1.o
	$(INST_64_M3_NS) $(INSOPT) -src libc_64_M3_ns/csu/crtn.o crtn.o
	$(INST_64_M3_NS) $(INSOPT) -src libc_64_M3_ns/csu/mcrt1.o mcrt1.o
	$(INST_64_M3_NS) -m 444 -src libc_64_M3_ns.a libc.a
	#
	# 64 BIT MIPS3 NONSHARED - symlinks per CPUARCH & MODE
	#
	$(I_64_M3) (cd libc_64_M3_ns; $(INST_64_NS) $(INSOPT4) \
				-lns ../mips3/nonshared/crt1.o crt1.o)
	$(I_64_M3) (cd libc_64_M3_ns; $(INST_64_NS) $(INSOPT4) \
				-lns ../mips3/nonshared/crtn.o crtn.o)
	$(I_64_M3) (cd libc_64_M3_ns; $(INST_64_NS) $(INSOPT4) \
				-lns ../mips3/nonshared/mcrt1.o mcrt1.o)
	$(I_64_M3) (cd libc_64_M3_ns; $(INST_64_NS) $(INSOPT4) \
				-lns ../mips3/nonshared/libc.a libc.a)

#
# 64 BIT MIPS4 SHARED
#
ins64_M4_s!
	#
	# 64 BIT MIPS4 SHARED
	#
	$(INST_64_M4) -m 444 -src libmpc_64_M4.a libmpc.a
	$(INST_64_M4) $(INSOPT) -src libc_64_M4/csu/crt1.o crt1.o
	$(INST_64_M4) $(INSOPT) -src libc_64_M4/csu/crtn.o crtn.o
	$(INST_64_M4) $(INSOPT) -src libc_64_M4/csu/mcrt1.o mcrt1.o
	$(INST_64_M4) -m 1555 -src libc_64_M4.so libc.so.1
	$(INST_64_M4) -lns libc.so.1 libc.so
	#
	# 64 BIT MIPS4 SHARED - symlinks per CPUARCH & MODE
	#
	$(I_64_M4) (cd libc_64_M4; $(INST_64) $(INSOPT8_64) -lns mips4/libc.so.1 libc.so.1)
	$(I_64_M4) (cd libc_64_M4; $(INST_64) $(INSOPT8_64) -lns mips4/crt1.o crt1.o)
	$(I_64_M4) (cd libc_64_M4; $(INST_64) $(INSOPT8_64) -lns mips4/crtn.o crtn.o)
	$(I_64_M4) (cd libc_64_M4; $(INST_64) $(INSOPT8_64) -lns mips4/mcrt1.o mcrt1.o)

#
# 64 BIT MIPS4 NONSHARED
#
ins64_M4_ns!
	#
	# 64 BIT MIPS4 NONSHARED
	#
	$(INST_64_M4_NS) $(INSOPT) -src libc_64_M4_ns/csu/crt1.o crt1.o
	$(INST_64_M4_NS) $(INSOPT) -src libc_64_M4_ns/csu/crtn.o crtn.o
	$(INST_64_M4_NS) $(INSOPT) -src libc_64_M4_ns/csu/mcrt1.o mcrt1.o
	$(INST_64_M4_NS) -m 444 -src libc_64_M4_ns.a libc.a
	#
	# 64 BIT MIPS4 NONSHARED - symlinks per CPUARCH & MODE
	#
	$(I_64_M4) (cd libc_64_M4_ns; $(INST_64_NS) $(INSOPT8_64) \
				-lns ../mips4/nonshared/crt1.o crt1.o)
	$(I_64_M4) (cd libc_64_M4_ns; $(INST_64_NS) $(INSOPT8_64) \
				-lns ../mips4/nonshared/crtn.o crtn.o)
	$(I_64_M4) (cd libc_64_M4_ns; $(INST_64_NS) $(INSOPT8_64) \
				-lns ../mips4/nonshared/mcrt1.o mcrt1.o)
	$(I_64_M4) (cd libc_64_M4_ns; $(INST_64_NS) $(INSOPT8_64) \
				-lns ../mips4/nonshared/libc.a libc.a)

# 64 bit ABI objects
ins64_abi!
	#
	# 64 bit ABI objects
	#
	$(INST_64_ABI) -lns crt1.o acrt1.o
	$(INST_64_ABI) $(INSOPT) -src libc_64_abi/csu/crt1.o crt1.o
	$(INST_64_ABI) $(INSOPT) -src libc_64_abi/csu/crtn.o crtn.o
	$(INST_64_ABI) $(INSOPT) -src libc_64_abi/csu/mcrt1.o mcrt1.o
	$(INST_64_ABI) -src libc_64_abi.so libc.so

#
# n32 BIT VERSIONS
#
insn32!
	#
	# n32 BIT VERSIONS
	#
	# Generic link between libc.so -> libc.so.1 and 
	# libc.so.1 -> /lib32/libc.so.1 in /usr/lib32
	$(INST_N32) -lns libc.so.1 libc.so
	$(INST_N32) -lns ../../lib32/libc.so.1 libc.so.1

#
# n32 BIT VERSIONS NONSHARED
#
insn32_ns!
	#
	# n32 BIT VERSIONS NONSHARED
	#
	$(INST_N32_NS) -m 444 -src libc_nomp_n32_M3_ns.a libc_nomp.a

#
# n32 BIT MIPS3 SHARED
#
insn32_M3_s! insn32_M4_s
	#
	# n32 BIT MIPS3 SHARED
	#
	$(INST_N32_M3) -m 444 -src libmpc_n32_M3.a libmpc.a
	$(INST_N32_M3) $(INSOPT) -src libc_n32_M3/csu/crt1.o crt1.o
	$(INST_N32_M3) $(INSOPT) -src libc_n32_M3/csu/crtn.o crtn.o
	$(INST_N32_M3) $(INSOPT) -src libc_n32_M3/csu/mcrt1.o mcrt1.o
	$(INSTALL) $(INSOPT) -F /usr/SpeedShop$(INSTDIR_N32_M3) -src libc_n32_M3.a libc.a
	$(INST_N32_M3) -lns libc.so.1 libc.so
# install a "real" mips3 n32 dso in mips3 dir on r8k systems, in /lib32
# on r4k, a symlink from usr/lib32/mips3/libc.so.1 -> ../libc.so.1 
# on r4k.  The inverse is done on r8k
	$(INST_N32_M3) $(INSOPT8) -m 1555 -src libc_n32_M3.so libc.so.1
	$(INSTALL) -F /lib32 $(INSOPT4) -m 1555 -src libc_n32_M3.so libc.so.1
	$(INST_N32_M3) $(INSOPT4) -lns ../libc.so.1 libc.so.1
	#
	# n32 BIT MIPS3 SHARED - symlinks per CPUARCH only
	#
	$(I_N32_M3) (cd libc_n32_M3; $(INST_N32) $(INSOPT4) -lns mips3/crt1.o crt1.o)
	$(I_N32_M3) (cd libc_n32_M3; $(INST_N32) $(INSOPT4) -lns mips3/crtn.o crtn.o)
	$(I_N32_M3) (cd libc_n32_M3; $(INST_N32) $(INSOPT4) -lns mips3/mcrt1.o mcrt1.o)

#
# n32 BIT MIPS3 NONSHARED
#
insn32_M3_ns! insn32_M4_ns
	#
	# n32 BIT MIPS3 NONSHARED
	#
	$(INST_N32_M3_NS) $(INSOPT) -src libc_n32_M3_ns/csu/crt1.o crt1.o
	$(INST_N32_M3_NS) $(INSOPT) -src libc_n32_M3_ns/csu/crtn.o crtn.o
	$(INST_N32_M3_NS) $(INSOPT) -src libc_n32_M3_ns/csu/mcrt1.o mcrt1.o
	$(INST_N32_M3_NS) -m 444 -src libc_n32_M3_ns.a libc.a
	#
	# n32 BIT MIPS3 NONSHARED - symlinks per CPUARCH only
	#
	$(I_N32_M3) (cd libc_n32_M3_ns; $(INST_N32_NS) $(INSOPT4) \
				-lns ../mips3/nonshared/crt1.o crt1.o)
	$(I_N32_M3) (cd libc_n32_M3_ns; $(INST_N32_NS) $(INSOPT4) \
				-lns ../mips3/nonshared/crtn.o crtn.o)
	$(I_N32_M3) (cd libc_n32_M3_ns; $(INST_N32_NS) $(INSOPT4) \
				-lns ../mips3/nonshared/mcrt1.o mcrt1.o)
	$(I_N32_M3) (cd libc_n32_M3_ns; $(INST_N32_NS) $(INSOPT4) \
				-lns ../mips3/nonshared/libc.a libc.a)

#
# n32 BIT MIPS4 SHARED
#
insn32_M4_s!
	#
	# n32 BIT MIPS4 SHARED
	#
	$(INST_N32_M4) -m 444 -src libmpc_n32_M4.a libmpc.a
	$(INST_N32_M4) $(INSOPT) -src libc_n32_M4/csu/crt1.o crt1.o
	$(INST_N32_M4) $(INSOPT) -src libc_n32_M4/csu/crtn.o crtn.o
	$(INST_N32_M4) $(INSOPT) -src libc_n32_M4/csu/mcrt1.o mcrt1.o
	$(INST_N32_M4) -lns libc.so.1 libc.so
# install a "real" mips4 n32 dso in mips4 dir on r4k systems, in /lib32
# on r8k, a symlink from usr/lib32/mips4/libc.so.1 -> ../libc.so.1 
# on r8k systems.  The inverse is done on r4k
	$(INST_N32_M4) $(INSOPT4) -m 1555 -src libc_n32_M4.so libc.so.1
	$(INSTALL) -F /lib32 $(INSOPT8) -m 1555 -src libc_n32_M4.so libc.so.1
	$(INST_N32_M4) $(INSOPT8) -lns ../libc.so.1 libc.so.1
	#
	# n32 BIT MIPS4 SHARED - symlinks per CPUARCH only
	#
	$(I_N32_M4) (cd libc_n32_M4; $(INST_N32) $(INSOPT8) -lns mips4/crt1.o crt1.o)
	$(I_N32_M4) (cd libc_n32_M4; $(INST_N32) $(INSOPT8) -lns mips4/crtn.o crtn.o)
	$(I_N32_M4) (cd libc_n32_M4; $(INST_N32) $(INSOPT8) -lns mips4/mcrt1.o mcrt1.o)

#
# n32 BIT MIPS4 NONSHARED
#
insn32_M4_ns!
	#
	# n32 BIT MIPS4 NONSHARED
	#
	$(INST_N32_M4_NS) $(INSOPT) -src libc_n32_M4_ns/csu/crt1.o crt1.o
	$(INST_N32_M4_NS) $(INSOPT) -src libc_n32_M4_ns/csu/crtn.o crtn.o
	$(INST_N32_M4_NS) $(INSOPT) -src libc_n32_M4_ns/csu/mcrt1.o mcrt1.o
	$(INST_N32_M4_NS) -m 444 -src libc_n32_M4_ns.a libc.a
	#
	# n32 BIT MIPS4 NONSHARED - symlinks per CPUARCH only
	#
	$(I_N32_M4) (cd libc_n32_M4_ns; $(INST_N32_NS) $(INSOPT8) \
				-lns ../mips4/nonshared/crt1.o crt1.o)
	$(I_N32_M4) (cd libc_n32_M4_ns; $(INST_N32_NS) $(INSOPT8) \
				-lns ../mips4/nonshared/crtn.o crtn.o)
	$(I_N32_M4) (cd libc_n32_M4_ns; $(INST_N32_NS) $(INSOPT8) \
				-lns ../mips4/nonshared/mcrt1.o mcrt1.o)
	$(I_N32_M4) (cd libc_n32_M4_ns; $(INST_N32_NS) $(INSOPT8) \
				-lns ../mips4/nonshared/libc.a libc.a)


#
# The symlink from /lib/libc.so --> /lib/libc.so.1 is for compatibility.
# libc.so.1 is set with the sticky bit to enable 'ghost' regions
#

headers!
	$(INSTALL) -F $(INSTINC) -m 444 -idb "noship" -src inc/synonyms.h  libc_synonyms.h
	$(INSTALL) -F $(INSTINC) -m 444 -idb "noship" -src inc/sync_internal.h sync_internal.h
	$(INSTALL) -F $(INSTINC) -m 444 -idb "noship" -src inc/semaphore_internal.h semaphore_internal.h
	$(INSTALL) -F $(INSTINC) -m 444 -idb "noship" -src inc/mplib.h mplib.h
	$(INSTALL) -F $(INSTINC) -m 444 -idb "noship" -src inc/libc_interpose.h libc_interpose.h
	$(INSTALL) -F $(INSTINC) -m 444 -idb "noship" -src inc/_wchar.h _wchar.h
	$(INSTALL) -F $(INSTINC) -m 444 -src inc/wctype.h wctype.h
	$(INSTALL) -F $(INSTINC) -m 444 -src inc/widec.h widec.h

libmpc.a: mpcdummy.o
	$(AR) rc libmpc.a mpcdummy.o

libmpc_64_M3.a:
	$(CCF) -64 -mips3 -o mpcdummy_64_M3.o -c mpcdummy.c
	$(AR) rc libmpc_64_M3.a mpcdummy_64_M3.o
	rm mpcdummy_64_M3.o

libmpc_64_M4.a:
	$(CCF) -64 -mips4 -o mpcdummy_64_M4.o -c mpcdummy.c
	$(AR) rc libmpc_64_M4.a mpcdummy_64_M4.o
	rm mpcdummy_64_M4.o

libmpc_n32_M3.a:
	$(CCF) -n32 -mips3 -o mpcdummy_n32_M3.o -c mpcdummy.c
	$(AR) rc libmpc_n32_M3.a mpcdummy_n32_M3.o
	rm mpcdummy_n32_M3.o

libmpc_n32_M4.a:
	$(CCF) -n32 -mips4 -o mpcdummy_n32_M4.o -c mpcdummy.c
	$(AR) rc libmpc_n32_M4.a mpcdummy_n32_M4.o
	rm mpcdummy_n32_M4.o

#
# A convenience rule for test building .so's
#
dso!
	cp libc.a libcdso.a
	$(AR) xl libcdso.a $(HIGHUSAGEOBJS)
	$(AR) d libcdso.a $(HIGHUSAGEOBJS)
	$(AR) ccrl libcdso.a $(HIGHUSAGEOBJS)
	rm -f $(HIGHUSAGEOBJS)
	$(LD) $(LDOPTS_32) $(LDDSOOPTS) libcdso.a -o $(DSOTARGOPT).precord
	# use 'cord' to reorder the DSO to optimize text page usage
	$(CORD) -o $(DSOTARGOPT) $(DSOTARGOPT).precord libc.so.1.fb
	chmod 0755 $(DSOTARGOPT)

showtargets!
	@echo $(TARGETS)
