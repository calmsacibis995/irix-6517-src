#!smake

#ident "$Revision: 1.125 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

NOTYET=getmntent.c pt.c swapctl.c

# ABIinfo.c is in libabi for linking and in libc for runtime

ABI_CFILES= \
	abort.c atexit.c assert.c bsearch.c \
	calloc.c clock.c closedir.c \
	crypt.c crypt_data.c cuexit.c ctime.c \
	fattach.c fdetach.c ffs.c fnmatch.c ftime.c ftok.c ftw.c \
	getcwd.c getdate.c getdate_gd.c getenv.c getlogin.c \
	getopt.c \
	getsubopt.c getwd.c glob.c \
	hsearch.c initgroups.c insque.c isastream.c iswctype.c \
	lfind.c lsearch.c \
	getpty.c \
	malloc.c memalign.c mkfifo.c mpconf.c ndbm.c nftw.c \
	opendir.c opt_data.c \
	perror.c putenv.c qsort.c \
	rand.c random.c readdir.c realpath.c remove.c rewinddir.c \
	regex.c \
	regcomp.c regerror.c regexec.c regfree.c _locutils.c \
	seekdir.c sleep.c \
	strptime.c syslog.c sysconf.c \
	tell.c telldir.c tfind.c time_gdata.c time_comm.c trwctype.c tsearch.c \
	valloc.c \
	wcsxfrm.c wctypefuncs.c wordexp.c \
	ualarm.c usleep.c utimes.c \
	values-Xt.c \
	_xftw.c xpg4_data.c

NONABI_CFILES= \
	ABIinfo.c atcpr.c \
	BSDclosedir.c BSDinitgroups.c BSDopendir.c BSDreaddir.c \
	BSDscandir.c BSDseekdir.c BSDtelldir.c \
	afsauth.c argvtostr.c \
	bit.c \
	cfree.c cftime.c \
	daemonize.c dbm.c devname.c dial.c di_aux.c \
	eic.c errlst.c exportent.c \
	\
	getabi.c _getcwd.c getprent.c \
	getdate_data.c \
	gethz.c getinvent.c \
	getlvent.c getpw.c getut.c getutx.c \
	ia_audit.c \
	import_def.c irixerror.c \
	issubdir.c \
	lckpwdf.c \
	mkdev.c mntent.c module_info.c \
	new_list.c \
	oserror.c \
	satvwrite.c \
	scandir.c scaninvent.c \
	ssdiloader.c strtoargv.c \
	str2id.c \
	stubfpeexit.c stubfpestart.c stubmpcexit.c \
	stubprofexit.c sysid.c \
	telldir64.c seekdir64.c \
	time_data.c \
	uuid.c 

ABI_32CFILES= ftw64.c nftw64.c readdir64.c

#if $(LIBRARY) == "libc_32_abi.a"
CFILES= $(ABI_CFILES) $(ABI_32CFILES)
#elif $(LIBRARY) == "libc_64_abi.a"
CFILES= $(ABI_CFILES)
#else
CFILES= $(ABI_CFILES) $(NONABI_CFILES)

#
# Only put stubmpexit, *ftw64, *dir64 in ucode 32 bit
#
#if $(OBJECT_STYLE:M32*) != ""
CFILES+= stubmpexit.c ftw64.c nftw64.c readdir64.c _xftw64.c scandir64.c
#endif

#
# Only put *dbm64 and tell64 in 32-bit libs not 64-bit
#
#if $(OBJECT_STYLE:M32*) != "" || $(OBJECT_STYLE:MN32*) != ""
CFILES+= ndbm64.c dbm64.c tell64.c
#endif

#if $(OBJECT_STYLE:MN32*) != "" || $(OBJECT_STYLE:M64*) != ""
CFILES+= __C_runtime_error.c
#endif
#endif

include $(LIBLEAFRULES)

mkerrlist: mkerrlist.c
# Force N32 compilation since build group sets up a default ABI of
# O32 which conflicts with -diag_error in $(LIBRARY_COPTS)
	$(HOST_CC) -n32 $(LIBRARY_COPTS) -o mkerrlist mkerrlist.c

errlst.c: errlist mkerrlist
	./mkerrlist errlist

new_list.c: errlst.c

LDIRT = errlst.c new_list.c uxsgierr_msg.src uxsyserr_msg.src mkerrlist
