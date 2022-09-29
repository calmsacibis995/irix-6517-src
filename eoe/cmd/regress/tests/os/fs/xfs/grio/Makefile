#!/usr/sbin/smake
#
# Makefile for cmd/regress/tests/os/fs/xfs
#
#	"$Revisions$"

include $(ROOT)/usr/include/make/commondefs

##########################
# Begin REGRESSP Section #
##########################

# Four simple steps need to be completed to ensure that this Makefile will
# work with the rest of regressp hierarchy.

# Step 1:
# If you added a new subdirectory to this directory, please add the name
# to the SUBDIRS macro.
SUBDIRS =

# Step 2:
# If you added any new files to this directory, please update the following 
# macros:
#	TESTS - A list of all of the names of the tests that are in
#		this directory.
#	CONFS - A list of all of the .conf files that correspond to the
#		tests.  If there is a CONF file for the directory, it
#		should be included here as well.
#	INFOS - A list of all of the .info files that correspond to the
#		tests.  If there is an INFO file for the directory, it
#		should be included here as well.
#	MISC  - A list of all other files (data files, the TEST_FILES file,
#		etc.) that should be copied to the directory.  Note: these
#		files will all be copied with permissions set to 644.
TESTS =
CONFS = CONF
INFOS =
MISC  = TEST_FILES

# Step 3:
# If this Makefile was copied from another directory, modify the CURDIR
# macro to correspond to the directory where this should be installed.
# N.B. If the current directory is a new one, make sure that you add the
#      directory name to the parent's SUBDIR macro in the parent's Makefile!
CURDIR = /usr/regress/tests/os/fs/xfs/grio

# Step 4:
# Change the COMMONPREF if you so desire...
COMMONPREF = xfs.grio.

# All done!  All other modifications to this Makefile should be rules to
# actually make the files in this directory.

########################
# End REGRESSP Section #
########################

.PRECIOUS: $(SUBDIRS)

CFILES =
HFILES =
SHFILES =
TARGETS =

IDB_TAG_SW = noship.sw.regress

default install $(COMMONTARGS): $(COMMONPREF)$$@
#if ! empty(SUBDIRS)
	@for d in $(SUBDIRS); do \
		echo "====\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done
#endif

include $(COMMONRULES)

#if ! empty(SUBDIRS)
$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)
#endif

$(COMMONPREF)default: $(TARGETS)

$(COMMONPREF)install: $(COMMONPREF)default
#if ! empty(TESTS)
	$(INSTALL) -F $(CURDIR) -idb $(IDB_TAG_SW) '$(TESTS)'
#endif
#if ! empty(CONFS)
	$(INSTALL) -F $(CURDIR) -idb $(IDB_TAG_SW) '$(CONFS)'
#endif
#if ! empty(INFOS)
	$(INSTALL) -F $(CURDIR) -m 644 -idb $(IDB_TAG_SW) '$(INFOS)'
#endif
#if ! empty(MISC)
	$(INSTALL) -F $(CURDIR) -m 644 -idb $(IDB_TAG_SW) '$(MISC)'
#endif
