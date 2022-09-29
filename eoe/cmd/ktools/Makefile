#
# $Modified: Mon Dec  8 20:15:48 1997 by cwilson $
#
# Makefile for ktools inst image
SHELL = /bin/sh
SOURCE= /

MANPAGES = ktools.1 kcons.1 kreset.1 ksteal.1 kwho.1

all: man na rtelnetd product

man:	
	cd man; make

install: 
	${INSTALL} -idb "ktools.sw.base" -m 755 -u root -g sys -F usr/annex -src kcons kcons
	${INSTALL} -idb "ktools.sw.base" -lns kcons -F usr/annex kfree
	${INSTALL} -idb "ktools.sw.base" -lns kcons -F usr/annex kgrab
	${INSTALL} -idb "ktools.sw.base" -lns kcons -F usr/annex klist 
	${INSTALL} -idb "ktools.sw.base" -lns kcons -F usr/annex ksteal
	${INSTALL} -idb "ktools.sw.base" -m 755 -u root -g sys -F usr/annex -src kwho.pl kwho
	${INSTALL} -idb "ktools.sw.base" -m 755 -u root -g sys -F usr/annex -src kreset kreset
	${INSTALL} -idb "ktools.sw.base exitop(\"/bin/rm -f ${rbase}/usr/annex/INSTALL ${rbase}/usr/annex/Sysfiles ${rbase}/usr/annex/init.rtelnetd ${rbase}/usr/annex/portsfile\")" -m 755 -u root -g sys -F usr/annex -src rtelnetd.and.na.and.etc/na/na na


	${INSTALL} -idb "ktools.sw.kdbx" -m 755 -u root -g sys -F usr/annex -src rtelnetd.and.na.and.etc/rtelnetd/rtelnetd rtelnetd
	${INSTALL} -m 755 -dir /dev/annex
	${INSTALL} -idb "ktools.sw.kdbx" -m 755 -u root -g sys -F usr/annex -src kgenerate kgenerate
	${INSTALL} -idb "config(update) ktools.sw.kdbx" -m 755 -u root -g sys -F etc/uucp -src Sysfiles Sysfiles
	${INSTALL} -idb "ktools.sw.kdbx" -m 755 -u root -g sys -F etc/init.d -src init.rtelnetd rtelnetd
	${INSTALL} -idb "ktools.sw.kdbx" -m 755 -u root -g sys -F etc/config -src chkconfig.rtelnetd rtelnetd
	${INSTALL} -idb "ktools.sw.kdbx" -F etc/rc2.d -lns ../init.d/rtelnetd S69rtelnetd
	${INSTALL} -idb "ktools.sw.kdbx exitop(\"/bin/echo '====================\\\nrtelnetd installed, with a chkconfig flag.  To start it now\\\nsay    \'/etc/init.d/rtelnetd start\' \\\n====================\\\n'\")" -F etc/rc0.d -lns ../init.d/rtelnetd K69rtelnetd


	${INSTALL} -idb "ktools.man.base" -m 755 -u root -g sys -F usr/share/catman/local/cat1 -src man/ktools.z ktools.z
	${INSTALL} -idb "ktools.man.base" -m 755 -u root -g sys -F usr/share/catman/local/cat1 -src man/kcons.z kcons.z
	${INSTALL} -idb "ktools.man.base" -lns kcons.z -F usr/share/catman/local/cat1 kgrab.z
	${INSTALL} -idb "ktools.man.base" -m 755 -u root -g sys -F usr/share/catman/local/cat1 -src man/kreset.z kreset.z
	${INSTALL} -idb "ktools.man.base" -m 755 -u root -g sys -F usr/share/catman/local/cat1 -src man/ksteal.z ksteal.z
	${INSTALL} -idb "ktools.man.base" -m 755 -u root -g sys -F usr/share/catman/local/cat1 -src man/kwho.z kwho.z
	${INSTALL} -idb "ktools.man.relnotes" -m 755 -u root -g sys -F usr/relnotes/ktools -src man/relnotes.TC	TC


na:	rtelnetd.and.na.and.etc/na/na
	cd rtelnetd.and.na.and.etc/na; make

rtelnetd: rtelnetd.and.na.and.etc/rtelnetd/rtelnetd
	cd rtelnetd.and.na.and.etc/rtelnetd; make

product: $(ALL)
	rm -rf rawidb; test -d image || mkdir image
	SRC=\   RAWIDB=rawidb \
		$(MAKE) -u install
	sort +4 <rawidb >idb
	REL=$${REL:-`cat rel`} ALPHA=$${ALPHA:-`cat alpha`} \
		gendist.5.3 -verbose -root / -source ${SOURCE} -idb idb \
		-spec spec -dist image
