#!smake

#ident "$Revision: 1.74 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

ABI_CFILES= \
	dup2.c fstat.c \
	gettimeday.c getdtabsz.c getpgsz.c lockf.c lstat.c \
	msgsys.c \
	pathconf.c \
	shmsys.c semsys.c stat.c

ABI_32CFILES= \
	fstat64.c lstat64.c stat64.c

NONABI_CFILES= \
	arsctl.c BSDdup2.c BSDgettimeday.c BSDsettimeday.c chproj.c \
	clock_getres.c clock_gettime.c clock_settime.c \
	flock.c getdtablehi.c \
	mknod.c mac.c mq_open.c \
	pm.c psched.c quotactl.c \
	reboot.c settimeday.c schedctl.c shm_open.c shm_unlink.c \
	sigevtop.c sysmp.c synch_cache.c xpg4_sockets.c \
	acceptSCI.c closeSCI.c connectSCI.c creatSCI.c \
	dmiSCI.c fcntlSCI.c fsyncSCI.c getmsgSCI.c \
	getpmsgSCI.c ioctlSCI.c msyncSCI.c nanosleepSCI.c \
	openSCI.c pauseSCI.c pollSCI.c preadSCI.c psema_cntlSCI.c \
	putmsgSCI.c putpmsgSCI.c pwriteSCI.c readSCI.c \
	readvSCI.c recvSCI.c recvfromSCI.c recvmsgSCI.c \
	sched_yieldSCI.c selectSCI.c sendSCI.c sendmsgSCI.c \
	sendtoSCI.c sginapSCI.c syscallSCI.c usync_cntlSCI.c \
	writeSCI.c writevSCI.c xpg4_select.c

NONABI_32CFILES =

ABI_ASFILES= \
	access.s acct.s alarm.s \
	chdir.s cerror.s \
	chmod.s chown.s chroot.s close.s  creat.s dup.s \
	fchdir.s fchmod.s fchown.s fcntl.s \
	fxstat.s fstatvfs.s fsync.s ftruncate.s \
	gethostid.s getitimer.s getmsg.s \
	getpmsg.s ioctl.s lchown.s \
	lxstat.s link.s lseek.s mkdir.s \
	mmap.s mount.s mprotect.s msync.s munmap.s nice.s \
	nuname.s  \
	open.s \
	pause.s pipe.s poll.s ptrace.s putmsg.s putpmsg.s \
	read.s readlink.s readv.s rename.s rmdir.s \
	sbrk.s select.s setitimer.s statvfs.s \
	stime.s symlink.s sync.s sysinfo.s \
	time.s times.s truncate.s \
	ulimit.s umask.s umount.s uname.s unlink.s \
	utime.s write.s writev.s xmknod.s xstat.s  \

ABI_32ASFILES = \
	cerror64.s fstatvfs64.s ftruncate64.s lseek64.s mmap64.s \
	statvfs64.s truncate64.s

NONABI_ASFILES = BSD_getime.s  \
	accept.s adjtime.s async_d.s attributes.s bind.s cachectl.s  \
	cacheflush.s connect.s \
	dmi.s \
	exportfs.s fdatasync.s \
	fstatfs.s getdomnm.s getfh.s \
	gethostnm.s  \
	getdents.s getmountid.s getpeernm.s getsocknm.s getsockopt.s \
	linkfollow.s listen.s \
	madvise.s \
	nanosleep.s nfssvc.s ngetdents.s \
	pread.s priocntl.s psema_cntl.s pwrite.s \
	recv.s recvmsg.s recvfrom.s \
	sched_getparam.s sched_getscheduler.s sched_rr_get_interval.s \
	sched_setparam.s sched_setscheduler.s sched_yield.s \
	sysfs.s syscall.s statfs.s send.s sendmsg.s sendto.s \
	setdomnm.s sethostid.s sethostnm.s setsockopt.s sgikopt.s \
	sginap.s shutdown.s socket.s socketpair.s  \
	swapctl.s \
	sysget.s sysmips.s syssgi.s \
	uadmin.s ustat.s usync_cntl.s utssys.s vhangup.s xpg4_recvmsg.s

NONABI_32ASFILES = \
	getdents64.s ngetdents64.s 

#if $(LIBRARY) == "libc_32_abi.a"
ASFILES= $(ABI_ASFILES) $(ABI_32ASFILES)
CFILES= $(ABI_CFILES) $(ABI_32CFILES)
#elif $(LIBRARY) == "libc_64_abi.a"
ASFILES= $(ABI_ASFILES) 
CFILES= $(ABI_CFILES)
#else
ASFILES=$(ABI_ASFILES) $(NONABI_ASFILES)
CFILES= $(ABI_CFILES) $(NONABI_CFILES)

#if $(OBJECT_STYLE:M32*) != ""
ASFILES+= $(NONABI_32ASFILES) $(ABI_32ASFILES)
CFILES+= $(NONABI_32CFILES) $(ABI_32CFILES)
#endif
#endif

include $(LIBLEAFRULES)
