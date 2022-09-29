#
#
# Makefile for Diagnostic Debug Monitor
#
#	"$Revision: 1.1 $"

include	$(ROOT)/usr/include/make/commondefs
include $(PRODUCTDEFS)


# Directories to fire off makes in.
#
SUBDIRS= cache mem monitor

# Create variable to time stamp executable
TSTAMP=`date '+%m%d%H%M'`

# Executable name
IMAGENAME= dmonprom

### Path to libraries
LIBPTH= ../libpdiag

.PRECIOUS: $(SUBDIRS)

COMMONPREF=dmon

# KLUDGE STUFF FOR NOW 
LCOPTS = -non_shared -coff
RELEASE= 1.6a
LDV =	$(LD) 
### For bootp version of dmon
#DIAGMON_START=	a84c0000
#DATA_ADDR= -B a8580000
### For prom version of dmon
DATA_ADDR= -B a0010000
DIAGMON_START=	bfc00000
LDFLAGPROM=     -G 0 -m -N -T $(DIAGMON_START) $(ENDIAN) $(DATA_ADDR)


# Default rule builds it all
#
default:
	@if test ! -d $(PRODUCT).O; then \
		mkdir $(PRODUCT).O; \
	fi

	@for d in $(SUBDIRS); do \
		echo "====\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done
	$(MAKE) $(IMAGENAME)
	@echo "Build for dmon completed"


$(COMMONTARGS): 
	@for d in $(SUBDIRS); do \
		echo "====\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

# Install target copies the executable dmonprom to the archive
# directory. Then does a p_rlog on the tree to determine the
# revision of the files. The results is stored in a file release.x, where
# x is the release number.
install:
	@if test ! -d archive ;\
	then \
		echo "Making archive directory"; \
		mkdir archive;\
	fi;
	@if [ -f archive/$(IMAGENAME).$(RELEASE) ]; \
	then \
		echo "File $(IMAGENAME).$(RELEASE) already exists"; \
		rm -i archive/$(IMAGENAME).$(RELEASE); \
	fi
	cp $(IMAGENAME) archive/$(IMAGENAME).$(RELEASE)
#	@date >> release.$(RELEASE)
#	@echo "" >> release.$(RELEASE)
#	@for i in `find $(DEPTH)/. -name RCS -print` ; \

#	    do \p_rlog -h | egrep "^RCS file:|^head:" \
#		| sed -e N -e 's#^RCS file: \(.*\).\nhead: *#\1      #g'\
#	    done | sort >> archive/release.$(RELEASE)



# target to build the executable, if the libraries are out of date.
#
dmonprom: dmon_vers.o $(PRODUCT).O/libcache.a $(PRODUCT).O/libmem.a $(PRODUCT).O/libmonitor.a $(LIBPTH)/$(PRODUCT).O/libpdiag.a
	$(LDV) $(LDFLAGPROM) -o $(PRODUCT).O/$(IMAGENAME)_$(TSTAMP)  $(PRODUCT).O/csu.monitor.o \
		$(PRODUCT).O/libmonitor.a \
		$(PRODUCT).O/libcache.a \
		$(PRODUCT).O/libmem.a \
		$(LIBPTH)/$(PRODUCT).O/mark_s.o \
		$(LIBPTH)/$(PRODUCT).O/libpdiag.a \
		$(LIBPTH)/$(PRODUCT).O/libcache.a \
		$(LIBPTH)/$(PRODUCT).O/libmem.a \
		$(LIBPTH)/$(PRODUCT).O/mark_e.o \
		dmon_vers.o > $(PRODUCT).O/$@_$(TSTAMP).map 
	$(NM) -Bnx $(PRODUCT).O/$@_$(TSTAMP) > $(PRODUCT).O/$@_$(TSTAMP).nm
	@if test "4DACE1" = $(PRODUCT) ; \
	then \
		ln -s $(PRODUCT).O/$(IMAGENAME)_$(TSTAMP) $(IMAGENAME).ip22; \
	else \
		ln -s $(PRODUCT).O/$(IMAGENAME)_$(TSTAMP) $(IMAGENAME).ip24;\
	fi

#	@$(SIZE) -x $@


# Generate a revision string and place it in a file.
dmon_vers.c: $(PRODUCT).O/libcache.a $(PRODUCT).O/libmem.a $(PRODUCT).O/libmonitor.a dmon_vers.o
	-rm -f dmon_vers.c
	echo "/* Configuration Control File */" > dmon_vers.c;
	echo "static char *SccsId=\"@(#)Silicon Graphics Inc.\c" \
	    >> dmon_vers.c; \
	echo "DMON: Version $(RELEASE) $(TYPE)" `date` `$(WHOAMI)`"\c" \
		    >> dmon_vers.c; \
	echo "\";" >> dmon_vers.c
	echo "char diag_version[]=\"\c" >> dmon_vers.c; \
	echo "DiagMon: Version $(RELEASE) $(TYPE)" `date` `$(WHOAMI)`"\c" \
	    >> dmon_vers.c; \
	echo "\";" >> dmon_vers.c

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

include $(COMMONRULES)
