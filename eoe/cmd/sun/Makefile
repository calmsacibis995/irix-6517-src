#
# Makefile for Sun-derived executable components and configuration files
#
include	${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

CDAEMON= biod.c nfsd.c portmap.c
CSIMPLE= domainname.c exportfs.c havenfs.c mkfile.c rarpd.c rpcinfo.c \
	 ypcat.c ypmatch.c ypwhich.c
CRPCSVC= rpc.mountd.c rpc.passwd.c rpc.rusersd.c rpc.rwalld.c rpc.sprayd.c \
	 rpc.ypupdated.c rup.c rusers.c rwall.c showmount.c \
	 spray.c ypchpass.c rpc.rquotad.c

CNLIST= nfsstat.c
CCRYPT= yppasswd.c
CFILES= ${CDAEMON} ${CSIMPLE} ${CRPCSVC} ${CNLIST} ${CCRYPT} umount.c

DAEMON= ${CDAEMON:.c=}
SIMPLE= ${CSIMPLE:.c=}
RPCSVC= ${CRPCSVC:.c=}
NLIST= ${CNLIST:.c=}
CRYPT= ${CCRYPT:.c=}
TARGETS= ${DAEMON} ${SIMPLE} ${RPCSVC} ${NLIST} ${CRYPT} umount

DIRS=	autofs automount cachefs config mount netdisk nhfsstone rexd \
	rpc.bootparamd rpc.lockd rpc.rstatd rpc.statd rpcgen rpcgen.tli \
	snoop testsuite yp
FILES=	bootparams dlif ethers exports netgroup rpc

LLDLIBS= -lbsd
LLDOPTS= -Wl,-T,10000000,-D,10040000
LDDOPTS= -Wl,-T,7FC00000,-D,7FC40000
LCDEFS = -D_BSD_TIME

#
# Installation tags - you can install just nfs.sw.nis, or both .nis and .nfs,
# or .nfs only.
#
IDBTAGNFS= -idb nfs.sw.nfs
IDBTAGYP= -idb nfs.sw.nis

default: ${TARGETS} ${FILES}
	@for d in ${DIRS}; do \
		${ECHO} "====\tcd $$d; ${MAKE}"; \
		cd $$d; ${MAKE}; cd ..; \
	done

COMMONPREF=sun
include ${COMMONRULES}

#
# XXX don't link daemons with shared libc because the pte overhead is not
# XXX worth the savings.
#
${DAEMON}: $$@.c
	${CCF} $@.c $(GLDOPTS) $(LDDOPTS) $(LLDLIBS) -o $@

${RPCSVC}: $$@.c
	${CCF} $@.c ${LDOPTS} -lrpcsvc ${LDLIBS} -o $@

${NLIST}: $$@.c
	${CCF} $@.c ${LDOPTS} -lelf -lcurses ${LDLIBS} -o $@

${CRYPT}: $$@.c
	${CCF} $@.c ${LDOPTS} -lcrypt -lrpcsvc ${LDLIBS} -o $@

umount: $$@.c
	${CCF} $@.c -Wl,-I,$(ROOTRLDNAME) ${LDOPTS} \
		$(ROOT)/usr/$(LIB_LOCATION)/librpcsvc.a \
		$(ROOT)/usr/$(LIB_LOCATION)/libbsd.a -o $@

${DIRS}: ${_FORCE}
	@${ECHO} "====\t$@"; cd $@; ${MAKE}

install ${COMMONTARGS}: ${COMMONPREF}$$@
	@for d in ${DIRS}; do \
		${ECHO} "====\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
	done

${COMMONPREF}install: ${TARGETS} ${FILES}
	${INSTALL} -F /sbin umount
	$(INSTALL) -F /etc -lns ../sbin/umount umount
	${INSTALL} -F /usr/sbin mkfile showmount havenfs
	$(INSTALL) -F /etc -lns ../usr/sbin/mkfile mkfile
	$(INSTALL) -F /etc -lns ../usr/sbin/showmount showmount
	$(INSTALL) -F /etc -lns ../usr/sbin/havenfs havenfs
	${INSTALL} -F /etc -m 644 -idb "config(update)" rpc
	${INSTALL} -F /etc/init.d -m 755 -idb "config(update)" dlif
	${INSTALL} -lns ../init.d/dlif -F /etc/rc0.d K81dlif
	${INSTALL} -F /usr/etc portmap rpcinfo
	${INSTALL} -F /usr/etc -m 711 biod nfsd rpc.mountd exportfs
	${INSTALL} -F /usr/etc -m 2711 -u root -g sys nfsstat
	${INSTALL} -F /etc -m 644 -idb "config(noupdate)" \
	    ethers exports netgroup
	${INSTALL} -F /usr/bin -m 711 \
	    domainname rup rusers yppasswd ypcat ypchpass ypmatch ypwhich
	${INSTALL} -F /usr/etc -m 711 \
	    rpc.passwd rpc.rusersd rpc.rwalld rpc.sprayd \
	    rpc.ypupdated rwall spray rpc.rquotad
	${INSTALL} -F /usr/etc -m 711 rarpd
