#! smake
# generate sendmail from sendmail.org
#
# SGI BUILDERS AND ENGINEERS
#	This is not a normal sgi style makefile
#	Engineers ... Read the file README.SGI for instructions on working in
#		      this tree
#	Builders .... This Makefile should correctly drive the sendmail build.
#
# "/proj/irix6.5m/eoe/cmd/sendmail/RCS/Makefile,v 1.6 1999/11/05 00:56:09 mende Exp"

SENDMAIL_VERSION= sendmail-8.9.3
SRC_DIRT= src.symlinks src.files src.dirs cf.dirs cf.files
ABI=-n32 -mips3
LDIRT= cf.dirs cf.files current.idb src.symlinks IDB sendmail.idb $(SRC_DIRT)
MAN_PAGES=sendmail/src/aliases.5.z sendmail/src/mailq.1.z \
	sendmail/src/newaliases.1.z sendmail/src/sendmail.8.z \
	sendmail/mail.local/mail.local.8.z \
	sendmail/mailstats/mailstats.8.z \
	sendmail/makemap/makemap.8.z \
	sendmail/praliases/praliases.8.z \
	sendmail/rmail/rmail.8.z
SM_TARGETS=/usr/lib/sendmail /usr/etc/mail.local /usr/bsd/mailstats /usr/bsd/makemap /usr/bsd/praliases /usr/bin/rmail
SM_LINKS=mailq newaliases
SUBDIRS= sgi

include	${ROOT}/usr/include/make/commondefs

INSTALL_EOE=${INSTALL} -u root -g sys -idb "EOE"
INSTALL_MAIL=${INSTALL} -u root -g mail -idb "EOE"
INSTALL_MAN=${INSTALL} -u root -g sys -idb "MAN_STD" -m 444
INSTALL_SRC=${INSTALL} -u root -g sys -idb "SENDMAIL_SRC"

all: sendmail
	# Mark this version as ours just for the build
	mv sendmail/src/version.c sendmail/src/version.c_stock
	sed 's/Version\[\] = \"/Version\[\] = \"SGI-/' < sendmail/src/version.c_stock > sendmail/src/version.c
	# building sendmail
	for i in sendmail ${SUBDIRS} ; do \
	  ( cd $$i ; MAKE=${MAKE} ABI="$(ABI)" $(MAKE) $@ ); \
	done
	# Revert to stock version.c
	mv sendmail/src/version.c_stock sendmail/src/version.c
	# making repeatable pathnames
	j=`find sendmail/src/* -type d -print` ; \
	k=`basename $$j` ; \
	l=`find sendmail -type d -name $$k -print` ; \
	for i in $$l ; do \
	  m=`dirname $$i` ; \
	  if ! test -d $$m/obj ; then \
	    mv $$m/$$k $$m/obj ; \
	  fi ; \
	done
	# man pages
	${MAKE} manpages

default: all

manpages: ${MAN_PAGES}

${MAN_PAGES}:
	dirname=`dirname $@` ; \
	basename=`basename $@` ; \
	page=`echo $$basename | sed 's/\.[0-9].*//'` ; \
	section=`echo $$basename | sed -e 's/\.z//' -e 's/^.*\.//'` ; \
	if ! test -e sendmail/man/$${page}.z ; then \
	  cp $$dirname/obj/$${page}.0 sendmail/man/$$page ; \
	  pack -f sendmail/man/$$page ; \
	fi

clean clobber:
	./clobber-tree ${SENDMAIL_VERSION}
	rm -rf sendmail
	for i in ${SUBDIRS}; do \
	  (cd $$i ; MAKE=${MAKE} ABI="$(ABI)" $(MAKE) $@ ); \
	done
	rm -f ${DIRT} ${LDIRT}

headers exports:

sendmail: src.all
	./symlink-tree preserve ${SENDMAIL_VERSION}
	tlink ${SENDMAIL_VERSION} sendmail
	./symlink-tree restore ${SENDMAIL_VERSION}
	./symlink-tree restore sendmail
	if ! test -d sendmail/man ; then \
	  mkdir sendmail/man ; \
	fi
	rm -f man.format
	if test -e /usr/lib/tmac/tmac.andoc; then \
	  echo "using nroff" > man.format ; \
	  echo "define(\`confNROFF',\`nroff')" > sendmail/BuildTools/Site/site.config.m4 ; \
	fi
	if test -x /usr/gnu/bin/groff;  then \
	  echo "using groff" > man.format ; \
	fi
	if test ! -e man.format ; then \
	  echo "Using preformatted man pages (hack)" > man.format ; \
	  mv sendmail/BuildTools/bin/Build sendmail/BuildTools/bin/Build.lnk ; \
	  sed 's/ch158/ch1580/' < sendmail/BuildTools/bin/Build.lnk > sendmail/BuildTools/bin/Build ; \
	  for i in ${MAN_PAGES}; do \
	    dirname=`dirname $$i` ; \
	    basename=`basename $$i` ; \
	    page=`echo $$basename | sed 's/\.[0-9].*//'` ; \
	    mv $${dirname}/$${page}.0 $${dirname}/$${page}.0.lnk ; \
	    cp $${dirname}/$${page}.0.lnk $${dirname}/$${page}.0 ; \
	  done ; \
	fi
	cat man.format
	rm  -f man.format

${SENDMAIL_VERSION}/.symlinks:
	if ! test -e $@; then \
	  ./symlink-tree preserve ${SENDMAIL_VERSION}; \
	fi

src.all:
	rm -f $(SRC_DIRT)
	${MAKE} $(SRC_DIRT)

src.symlinks: ${SENDMAIL_VERSION}/.symlinks
	sed -e 's/^\./sendmail/' < $(SENDMAIL_VERSION)/.symlinks > $@

src.files:
	find ${SENDMAIL_VERSION} -type f -print | \
	  grep -v '\.symlinks$$' | \
	  sed -e 's/${SENDMAIL_VERSION}/sendmail/' > $@

src.dirs:
	find ${SENDMAIL_VERSION} -type d -print | \
	  sed -e 's/${SENDMAIL_VERSION}/sendmail/' > $@

cf.dirs: src.dirs
	grep sendmail/cf src.dirs > $@

cf.files: src.files
	grep sendmail/cf src.files | egrep -v '(ostype|cf/cf|siteconfig|/domain/)' > $@
	grep sendmail/cf/ostype src.files | egrep '(irix|unknown)' >> $@
	grep sendmail/cf/cf src.files | grep 'proto' >> $@
	grep sendmail/cf/domain/generic src.files >> $@

install: default src.all
	# install SM_TARGETS
	( cd sendmail/src/obj ; \
	  ${INSTALL_EOE} -m 4755 sendmail /usr/lib/sendmail )
	( cd sendmail/src/obj ; \
	  ${INSTALL_EOE} -m 755 sendmail.hf /usr/lib/sendmail.hf)
	( cd sendmail/mail.local/obj ; \
	  ${INSTALL} -u root -g mail -idb "EOE" -m 4755 mail.local /usr/bin/mail.local)
	( cd sendmail/mailstats/obj ; \
	  ${INSTALL_EOE} -m 755 mailstats /usr/bsd/mailstats)
	( cd sendmail/makemap/obj ; \
	  ${INSTALL_EOE} -m 755 makemap /usr/bsd/makemap)
	( cd sendmail/praliases/obj ; \
	  ${INSTALL_EOE} -m 755 praliases /usr/bsd/praliases)
	( cd sendmail/rmail/obj ; \
	  ${INSTALL_MAIL} -m 755 rmail /usr/bin/rmail)
	#
	# install SM_LINKS
	for i in ${SM_LINKS} ; do \
	  ${INSTALL_EOE} -F /usr/bsd -lns ../lib/sendmail $$i ; \
	done
	#
	# install MAN_PAGES
	#
	for i in ${MAN_PAGES}; do \
	  k=`basename $$i` ; \
	  page=`echo $$k | sed 's/\.[^\.]*\.z/\.z/'` ; \
	  section=`echo $$k | sed -e 's/\.z//' -e 's/^.*\.//'` ; \
	  if test "$$section" = "8"; then section=1; fi ; \
	  if test "$$section" = "5"; then \
	    section=4 ; \
	    mansect="p_man" ; \
	  else \
	    mansect="a_man" ; \
	  fi ; \
	  ( cd sendmail/man ; \
	    ${INSTALL_MAN} $$page /usr/share/catman/$${mansect}/cat$${section}/$${page} ; \
	  ) ; \
	done
	# install config files
	#
	# install system dirs
	${INSTALL} -idb "EOE" -u root -g mail -m 0775 -dir /var/spool/mqueue
	#
	# install src.dirs
	for i in `cat src.dirs` ; do \
	  ( cd $$i ; \
	    ${INSTALL_SRC} -m 755 -dir /usr/share/src/$$i  ; \
	  ) ; \
	done
	#
	# install src.files
	for i in `cat src.files` ; do \
	  j=`dirname $$i` ; \
	  k=`basename $$i` ; \
	  (cd $$j ; \
	    ${INSTALL_SRC} -m 444 $$k /usr/share/src/$$i ; \
	  ) ; \
	done
	#
	# install src.symlinks
	cat src.symlinks | while read i; do set $$i ; \
	  j=`echo $$1 | sed 's/^sendmail\///'` ; \
	  ${INSTALL_SRC} -F /usr/share/src/sendmail -lns $$2 $$j ; \
	done
	#
	# install cf.dirs
	for i in `cat cf.dirs` ; do \
	  h=`echo $$i | sed 's/^sendmail\/cf/sendmail.cf_m4/'` ; \
	  ( cd $$i ; \
	    ${INSTALL_EOE} -m 755 -dir /usr/lib/$$h  ; \
	  ) ; \
	done
	#
	# install cf.files
	for i in `cat cf.files` ; do \
	  h=`echo $$i | sed 's/^sendmail\/cf\///'` ; \
	  j=`dirname $$i` ; \
	  k=`basename $$i` ; \
	  (cd $$j ; \
	    ${INSTALL_EOE} -m 644 $$k /usr/lib/sendmail.cf_m4/$$h ; \
	  ) ; \
	done
	#
	# Install in subdirs
	for i in $(SUBDIRS) ; do \
	  (cd $$i ; $(MAKE) $@ ) ; \
	done

sendmail.idb:
	rm -f IDB
	SRC=$$WORKAREA/irix6.5m/eoe \
	  RAWIDB=$$WORKAREA/irix6.5m/eoe/cmd/sendmail/IDB \
	  $(MAKE) install
	sort +4 -6 -u IDB > $@
	rm -f IDB

../../build/idb:: FRC
	p_tupdate $@

current.idb: ../../build/idb
	egrep 'sendmail|rmail' ../../build/idb > $@

FRC:
