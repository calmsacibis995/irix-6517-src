#
# Makefile to build the sa file
#
#ident "$Revision: 1.25 $"

include $(ROOT)/usr/include/make/ismcommondefs

SA_DIR=$(WORKAREA)/eoe/sa
IDB_DIR=$(SA_DIR)/idbs

SA_WORKAREA=$(SA_DIR)/mrtree

IDB_SUFFIX=build/finalidb

RAWIDB=$(IDB_DIR)/idb.new

#
# ISMS that have "MR" files
#
#ISMS= buildtools irix cmplrs eoe

ISMS=buildtools irix cmplrs eoe

#
# Special tools
#

# comment this line out to build a miniroot with debug dso-s
STRIPALL=1

IDBINSTOPTS=-xv
#if defined(STRIPALL) && !empty(STRIPALL)
IDBINSTOPTS+=-T
#else
IDBINSTOPTS+=-S
#endif


default install:

idb:
#
# Check to make sure all of the files needed exist before creating mr.
# And then check to make sure the finalidb files have MR tags - if not idbinst
# will error and cause the script to fail.
#
	@for p in $(ISMS) ; do \
		if [ ! -s $(WORKAREA)/$$p/$(IDB_SUFFIX) ] ; then \
			echo "**** Stand Alone Error : $$p/$(IDB_SUFFIX) doesn't exist ****" ; \
			exit 1 ; \
		fi ; \
		if  test "`grep -s MR $(WORKAREA)/$$p/$(IDB_SUFFIX) | wc -l `" -eq 0  ; then \
			echo "**** Stand Alone Error : $$p/$(IDB_SUFFIX) doesn't have MR tag ****" ;\
			exit 1 ; \
		fi ; \
        done
# 
# If all the files exist then build mr.
#
	@rm -rf $(RAWIDB)
	-@for i in $(ISMS) ; do \
		egrep "MR" $(WORKAREA)/$$i/$(IDB_SUFFIX)|grep '^d'>>$(RAWIDB); \
	done; \
	sort -u +4 -6 -o $(RAWIDB) $(RAWIDB); \
	$(IDBINST) $(IDBINSTOPTS) -s $(WORKAREA) -r $(SA_WORKAREA) \
	 -i $(RAWIDB) 'MR'
	@rm -rf $(RAWIDB)
	@for i in $(ISMS) ; do \
		egrep "MR" $(WORKAREA)/$$i/$(IDB_SUFFIX) >> $(RAWIDB) ; \
		$(IDBINST) $(IDBINSTOPTS) -s $(WORKAREA)/$$i -r $(SA_WORKAREA) \
			-i $(WORKAREA)/$$i/$(IDB_SUFFIX) 'MR' ; \
	done
