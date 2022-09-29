#ident $Revision: 1.7 $
#

IDB_PATH = /usr/share/catman/p_man/cat2/ftn
IDB_TAG  = irix.man.ftn


include $(ROOT)/usr/include/make/mandefs

#
# Build pages for Fortran
#
LANGOPT = f

#
# Relative path to directory containing the c pages
#
MDIR = ../../man2

#
# List of pages in the other directory that contain .Op f directives
#
FTN_PAGES =	blockproc.2 \
		brk.2 \
		chown.2 \
		exit.2 \
		fcntl.2 \
		fork.2 \
		getdents.2 \
		gethostid.2 \
		gethostname.2 \
		getsockopt.2 \
		getuid.2 \
		ioctl.2 \
		kill.2 \
		link.2 \
		lseek.2 \
		mkdir.2 \
		mknod.2 \
		mount.2 \
		open.2 \
		pause.2 \
		pipe.2 \
		plock.2 \
		prctl.2 \
		profil.2 \
		rmdir.2 \
		schedctl.2 \
		send.2 \
		setpgrp.2 \
		sginap.2 \
		shmop.2 \
		signal.2 \
		signalnotes \
		sigset.2 \
		sigsetnotes \
		socket.2 \
		sproc.2 \
		stat.2 \
		stime.2 \
		symlink.2 \
		sysmp.2 \
		ulimit.2 \
		umount.2 \
		unlink.2 \
		wait.2 \
		truncate.2 \
		stat64.2 \
		warnings

MANPREF = ftn

default install: make_links
		$(MAKE) $(MANPREF)$@

clean:		$(MANPREF)clean

clobber:	$(MANPREF)clobber rm_links

include $(ROOT)/usr/include/make/manrules

make_links:
	@echo "Making links to source pages ..."
	-@for file in $(FTN_PAGES) ; do \
		rm -f $$file ; \
		ln -s $(MDIR)/$$file . ; \
	done

rm_links:
	-@find . -type l -print | xargs rm -f "{}" ";"
