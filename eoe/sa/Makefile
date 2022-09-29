#!smake
# ident "$Header: /proj/irix6.5.7m/isms/eoe/sa/RCS/Makefile,v 1.70 1999/11/06 00:52:59 sherwood Exp $"
#
# This makefile generates the files that make up a distribution.  It uses
# the following items:
#
#	$DIST		the directory in which to generate products.
#

include $(ROOT)/usr/include/make/ismcommondefs
include $(ROOT)/usr/include/make/kcommondefs
.NOTPARALLEL:

#
# Various Directories
#
SA_DIR=$(WORKAREA)/eoe/sa
IDB_DIR=$(SA_DIR)/idbs
DIST=$(SA_DIR)/sa_dir

# directory where we link all the files going into sa,
# the tape filesystem image
STMP=sa_tmp

LD_LIBRARY_PATH=$(ROOT)/lib:$(ROOT)/usr/lib
LD_LIBRARYN32_PATH=$(ROOT)/lib32:$(ROOT)/usr/lib32

MKFS_XFS=LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) \
	$(WORKAREA)/irix/cmd/xfs/mkfs/n32ns/mrmkfs_xfs

#
#	$PROTO_MR - is the check-in proto type file
#	$PROTO_NEW - is the new proto type file that was just created
#	
PROTO_MR=$(SA_DIR)/proto.mr
PROTO_NEW=$(SA_DIR)/proto.new
SA_WORKAREA=$(SA_DIR)/mrtree

IDB_MR_FILE=$(IDB_DIR)/idb.new
IDB_SASH=$(WORKAREA)/stand/build/finalidb
IDB_FX=$(WORKAREA)/irix/build/finalidb

TARGETS=sa

IDBINSTOPTS=-xvS


#
# The miniroot file system size, which must be less than or equal to
# the swaplo setting in the miniroot kernel, defined by the second
# parameter to the "SWAPDEV:" statement in irix/kern/master.d/system.gen
# (check to make sure it is "if" defined with "mr") and
# eoe/cmd/initpkg/brc-mr.sh.
# The miniroot filesystem is xfs_growfs-ed in place (in
# eoe/cmd/initpkg/bcheckrc-mr.sh), so it needs to be just
# larger than it's contents, and smaller than the swaplow value.
# These values will need to change as the contents change size.
# 	The file are (again) :
#		eoe/cmd/initpkg/brc-mr.sh
#		eoe/cmd/initpkg/bcheckrc-mr.sh
#		irix/kern/master.d/system.gen
#
# Size is in 512-byte blocks for the variables MRSIZE, and MRCHK.
# There is no need to change the MRSIZE in this file any 
# longer when the swap space size is changed for the SA.
#

MRSIZE	= 36500
MRCHK = 36200

default:$(TARGETS)

# build the standalone tools - in order
#
#	1) mkmrunix - create the unix mini-root kernel for $PRODUCT.
#	2) mkmr - Combine the mrtree into the "mini-root" file.
#	3) proto - Create a proto file and copy all the sash and fx's
#		to a tmp dir, build the miniroot EFS image, and then build
#		the sa tape filesystem image
#

sa:	mkmrunix mkmr proto

include $(ISMCOMMONRULES)

#
#	Create all the unix kernels
mkmrunix:
	@rm -rf $(DIST)/miniroot
#if defined(PRODUCT) && !empty(PRODUCT)
	@echo "PRODUCT=$$PRODUCT  $(MAKE) unix.mr"
	cd $(WORKAREA)/irix/kern; $(MAKE) unix.mr
	cp $(ROOT)/$(CPUINSTDIR)/unix.mr $(SA_WORKAREA)/unix.$(CPUBOARD)$(SUBPRODUCT)
#else
	@rm -rf $(SA_WORKAREA) ; mkdir $(SA_WORKAREA)
	@for p in $(EVERYPRODUCT) ; do \
		PRODUCT=$$p; export PRODUCT ;\
		echo "PRODUCT=$$p $(MAKE) unix.mr" ;\
		cd $(WORKAREA)/irix/kern; $(MAKE) unix.mr ;\
	done
#endif

#	Move all the files and kernels to the mrtree
#       HUGE ASSUMPTION: any file that is not user writable is not
#		writable by everyone. see the chmod a+w and chmod a-w.
#	
mkmr:
	cd idbs; $(MAKE) idb
	-find ${SA_WORKAREA}/ -type f | xargs strip -flsk 2>&1 | grep -v '^strip: .*: not an Elf file$$' > strip.errors
	sed -e 's/^strip: \(.*\): cannot open file/\1/' strip.errors | grep '^strip' >strip.realerrors || /bin/true
	if [ -s strip.realerrors ] ; then echo "ERRORS:" ; cat strip.realerrors ; exit 77; fi
	sed -e 's/^strip: \(.*\): cannot open file/\1/' strip.errors | grep -v '^strip' | while read file ; do chmod a+w $$file ; strip -fslk $$file ; chmod a-w $$file ; done 2>&1 | grep -v '^strip: .*: not an Elf file$$' >strip.realerrors || /bin/true
	if [ -s strip.realerrors ] ; then echo "ERRORS:" ; cat strip.realerrors ; exit 88; fi
	rm strip.realerrors strip.errors
	@if [ ! -d $(DIST)/miniroot ] ; then \
		mkdir -p $(DIST)/miniroot ;\
	fi
	@for i in $(EVERYPRODUCT); do \
		ipxx=`sed -n 's/^CPUBOARD *= *//p' $(ROOT)/usr/include/make/$$i""defs`; \
		ipsp=`sed -n 's/^SUBPRODUCT *= *//p' $(ROOT)/usr/include/make/$$i""defs`; \
		cp $(ROOT)/usr/sysgen/$$ipxx$$ipsp"boot/unix.mr" $(DIST)/miniroot/unix.$$ipxx$$ipsp ;\
	done
	sort -u +4 -5 $(IDB_MR_FILE) > finalidb

#  Build the miniroot xfs image, then the sa tape filesystem.  Check the
#  generated mkfs proto file against the checked in version for errors.
proto:
	@if [ -r proto_NEW ] ; then \
		rm -f proto_NEW ;\
	fi
	@if [ ! -s $(PROTO_NEW) ] ; then \
		$(IDBPROTO) -d -r mrtree -i finalidb MR > $(PROTO_NEW) ;\
	fi
	@if test "`diff -w $(PROTO_NEW) $(PROTO_MR) | wc -l`" -ne 0 ; then \
		echo "***** Stand Alone Error : 'proto_NEW' and 'proto_mr'" ;\
		echo "  are different.  Diff, Merge and Check-in new 'proto_mr' file." ;\
		echo "  To continue building SA - make proto" ;\
		mv $(PROTO_NEW) proto_NEW ;\
		exit 1 ;\
	fi
	@if test "`/usr/bin/du -s mrtree | cut -f1`" -gt $(MRCHK) ; then \
		echo "***** Stand Alone Error : mrtree size is greater that $(MRCHK) ****" ;\
		exit 1 ;\
	fi
#	Create actual mr file system from proto.mr file
	rm -rf $(STMP) ; mkdir $(STMP)
	$(MKFS_XFS) -b size=512 -d name=$(STMP)/mr,file,size=$(MRSIZE)b -l internal,size=1000b -p $(PROTO_MR)

#       If using mkfs uncomment these three lines.
#       @touch mr
#	/etc/mkfs mr $(MRSIZE) 6144 10 32 4900 32 32 $(PROTO_MR)
#       @mv -f mr $(STMP)/mr

	@cd $(STMP) ; ln -s mr 20mr
	@if [ ! -d $(DIST) ] ;  then \
		rm -f $(DIST) ; \
		mkdir -p $(DIST) ; \
	fi
	@if [ ! -s $(IDB_SASH) ] ; then \
		echo "***** Stand Alone Error : Missing $(IDB_SASH) file ****" ;\
	fi
	@if [ ! -s $(IDB_FX) ] ; then \
		echo "***** Stand Alone Error : Missing $(IDB_FX) file ****" ;\
	fi
	( $(IDBINST) $(IDBINSTOPTS) -s $(WORKAREA)/irix -r $(STMP) -i $(IDB_FX) SA ; \
	$(IDBINST) $(IDBINSTOPTS) -s $(WORKAREA)/stand -r $(STMP) -i $(IDB_SASH) MR ) | \
	(       cd $(STMP) > /dev/null ; echo 20mr mr ; \
		while read fname ; \
		do \
			bname=`expr "$$fname" : '[0-9][0-9]\(.*\)'` ; \
			rm -f $$bname ; \
			mv -f $$fname $$bname ; \
			ln -sf $$bname $$fname ; \
			echo $$fname $$bname ; \
		done \
	) | sort -u | sed 's:.* ::' | \
	( cd $(STMP) ; xargs -t mkboottape -f $(DIST)/sa )

# fastsa assumes that all the sash/fx/unix.IP* are in the sash/miniroot directory
# and are ready to go, (for use after patching up failures in proto?)
fastsa:
	@rm -f $(STMP)/mr
	$(MKFS_XFS) -b size=512 -d name=$(STMP)/mr,file,size=$(MRSIZE)b -l internal,size=1000b -p $(PROTO_MR)

#       If using mkfs uncomment these three lines.
#       @rm -f mr $(STMP)/mr ; touch mr
#	/etc/mkfs mr $(MRSIZE) 6144 10 32 4900 32 32 $(PROTO_MR)
#       @mv -f mr $(STMP)/mr

	@if [ ! -d $(DIST) ] ;  then \
		rm -f $(DIST) ; \
		mkdir -p $(DIST) ; \
	fi
	cd $(STMP) ; /sbin/ls [0-9][0-9]* | \
	( while read fname ;\
		do \
			bname=`expr "$$fname" : '[0-9][0-9]\(.*\)'` ; \
			echo $$fname $$bname ; \
		done \
	) | sort -u | sed 's:.* ::' | \
	( xargs -t mkboottape -f $(DIST)/sa )
