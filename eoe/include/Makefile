#
# Makefile for irix/include, to install header files under /usr/include.
#
include $(ROOT)/usr/include/make/commondefs


#
# Public header files, shipped to customers and used by almost everyone.
# Try to keep these alphabetized, to avoid duplicates and ease searches.
# These are in alphabetical order!
#
PUBLIC_HFILES= \
	abi_mutex.h aio.h archives.h asm.h assert.h \
	ABIinfo.h \
	bstring.h \
	cap_net.h capability.h clearance.h \
	core.out.h cpio.h cr1.h crypt.h cs.h ctype.h \
	dbm.h deflt.h devmgmt.h di_passwd.h di_group.h di_aux.h \
	dial.h dirent.h dlfcn.h \
	errno.h exportent.h \
	fam.h fcntl.h fetchop.h fmtmsg.h fnmatch.h fp_class.h fsd.h ftw.h \
	getabi.h getopt.h glob.h grio.h grp.h \
	hostreg.h \
	ia.h iconv.h invent.h \
	langinfo.h lastlog.h limits.h listen.h locale.h locale_attr.h \
	malloc.h mdbm.h memory.h mls.h mntent.h mon.h \
	monetary.h mutex.h mqueue.h \
	ndbm.h netconfig.h netdb.h netdir.h nl_types.h ns_api.h ns_daemon.h \
	paths.h \
	pfmt.h pkgdev.h pkginfo.h pkglocs.h pkgstrct.h pkgtrans.h \
	poll.h proj.h pthread.h pwd.h \
	re_comp.h regdef.h regex.h regexp.h resolv.h \
	sac.h sat.h sched.h search.h semaphore.h \
	setjmp.h sgtty.h shadow.h shareIIhooks.h siginfo.h signal.h \
	sgi_nl.h ssdi.h standards.h \
	std.h stddef.h \
	stdio.h stdlib.h string.h strings.h stropts.h \
	sum.h sys.s sysexits.h syslog.h t6net.h \
	task.h tar.h tcl.h termio.h termios.h time.h ttymap.h tzfile.h \
	timers.h tiuser.h \
	ucontext.h ulimit.h ulocks.h unistd.h ustat.h utime.h utmp.h utmpx.h \
	valtools.h vme_dma_engine.h \
	wait.h wchar.h wordexp.h wsregexp.h \
	xti.h

#
# List of headers that only system software utilities need
# These are public within SGI for internal development of other ISM's
# but are not shipped to customers.
#
FRIEND_HFILES= \
	comp.h \
	devconvert.h diskinfo.h diskinvent.h \
	mountinfo.h iconv_cnv.h iconv_int.h \
	mediaclient.h \
	sync.h \
	uncomp.h \
	xdr_mc.h

#
# List of subdirectories to run makes in.
#
SUBDIRS=arpa des protocols rpcsvc sys makerules

HPUT=$(INSTALL) -m 444 -F /usr/include

default:

COMMONPREF=inc
include $(COMMONRULES)

headers: install

exports:

install $(COMMONTARGS): $(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(COMMONPREF)install: $(_FORCE)
	$(HPUT) $(PUBLIC_HFILES)
	$(HPUT) $(FRIEND_HFILES)
