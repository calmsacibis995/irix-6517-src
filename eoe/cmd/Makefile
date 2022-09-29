#
# Makefile for irix/cmd
#
# $Revision: 1.286 $

include	$(ROOT)/usr/include/make/commondefs

#
# Alphabetized, grouped list of subdirectories.
#

# Turn off directories :
#	NONE
#
SUBDIRS= \
	CMWsession \
	abicmd acct acctcvt adm alpq apitools attr attrinit \
	autopush awk \
	banner basename bc bdiff bfs bison bloatview bootp bru bsd bstream \
	bufview \
	cal calendar cat cdplayer check chkconfig cksum \
	chacl chcap chlabel chroot \
	chmod chown chproj cmp cmplrs coffcheck col comm \
	cpio cron crypt csh \
	csplit cu cut \
	date dd deroff dev df dhcp_server diff dircmp dirname \
	diskpatch dkstat dlook dplace dvhtool du \
	ecc echo ed egrep env efs evctr expand expr exstr \
	failover \
	factor false fam fcman fdetach \
	fgrep file find flex fmtmsg \
	fp fsr fstyp ftimer fuser fpmode fx \
	gencat getabi getconf getopt gettapedev gettxt \
	getty gr_osview grep grpck groups grelnotes gzip \
	head \
	id idbg init initpkg Install ioconfig ipc iconv \
	iconv_comp gnum4 join \
	kbdload kbdpipe kbdset \
	kill killall ksh \
	latenscope lex line link listen locale localedef lockstat log login logname \
	ls Mail m4 maclbldevs mail mailx make makekey man mesg message_system \
	messages \
	mediad miser \
	mkboottape mkdepend mkdir mkfifo mkfs mkmsgs \
	mknod ml more mountcd mountcdda mountdos mounthfs mpadmin mt mv \
	netsnoop newform newgrp newlabel newproj news \
	newsess nfsunhang nice nl nohup npri ns nsort nvram numa \
	oawk od odiff osview \
	pack par passmgmt passwd patch pathchk pawk pax pcp perl5 pg paste pfmt \
	pmake ppp preg printf pr priocntl \
	priotest proclaim profiler prtvtoc ps pwck pwconv pwd \
	rcs react recover regcmp regress regview relnotes renice rfind \
	rm rmdir rmt rpcbind rtmon runon \
	sa saf sat sccs script scsiadminswap \
		scsicontrol sdiff sed osendmail setmnt \
		setsym sh sid sleep slip \
		sort srchtxt spell split \
		stacker stat strings stty \
		su suattr sulogin sum sun swap sync sysinfo sysmeter systune \
		sadmin sss \
	tabs tail tar tcsh tee test time tlink touch tr true tset \
		tsix tsort tty top topology topwait \
	uadmin ul uname unexpand unifdef units uniq unlink unpack utmpadmin uucp \
	vi \
	wakeupat wall wc wchrtbl whereis who whodo whoami write \
	xactl xargs xdiff xfs \
	yacc

HEADERS_SUBDIRS=bison message_system messages pcp sss
EXPORTS_SUBDIRS=pcp sss

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
