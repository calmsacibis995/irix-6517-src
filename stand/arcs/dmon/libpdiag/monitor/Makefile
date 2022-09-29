#
# Makefile for libpdiag. This makefile produces libraries for
# $(EVERYPRODUCT) and places them in the $(PRODUCT).O directory.
# Note: If the environment variable 'PRODUCT' is defined it makes
# that product, else it makes all products as defined by $(EVERYPRODUCT).
#
#	"$Revision: 1.1 $"

DEPTH= ..
include	$(ROOT)/usr/include/make/commondefs
include $(PRODUCTDEFS)
include $(DEPTH)/libdefs
include depfiles


# Define the products platforms we will be building.
#
EVERYPRODUCT =  4DCT 4DACE1

# There will be a directory for each value of PRODUCT containing
# the results produced by this Makefile. $(OAREA) is the macro referring
# to this directory. PRODUCT is defined in the $ROOT/usr/include/make
# directory. It is brought in by the PRODUCTDEFS include file.
OAREA	=$(PRODUCT).O


# List of 'C' files, CFILES_MACHDEP is defined in the $(PRODUCT)defs file
#
CFILES= sload.c \
	stdio.c

IP22_X = \#
IP20_X = \#

$(CPUBOARD)_X=

# Define machine dependent files
$(IP22_X)ASFILES_MACHDEP=$(SFILES_IP22)
$(IP20_X)ASFILES_MACHDEP=$(SFILES_IP20)

# Assembly files, ASFILES_MACHDEP is defined in the $(CPUBOARD)def file.
#
ASFILES=	$(ASFILES_MACHDEP) \
	commands.s	\
	aina_test.s	\
	atoi.s          \
	clientasm.s     \
	libasm.s	\
	date.s          \
	exit.s          \
	getbcd.s        \
	getdec.s        \
	gethex.s        \
	gets.s          \
	gets_nv.s       \
	led.s           \
	nv_ram.s        \
	nvram_addru.s   \
	parser.s        \
	locore.s	\
	putbcd.s        \
	putbyte.s       \
	putdec.s        \
	puthex.s        \
	puts.s          \
	set_tod.s       \
	subr.s		\
	nv_addrudbg.s	\
	dsubr.s

#LDIRT= $(OAREA)/*.o

# Define the target file to build
#
TARGETS	=libpdiag.a

# Default rule
default:
	@if test -n "$(PRODUCT)"; \
	then \
		if test ! -d $(DEPTH)/$(PRODUCT).O; then \
			mkdir $(DEPTH)/$(PRODUCT).O; \
		fi ;\

		echo ".. PRODUCT defined $(CPUBOARD)..";exec $(MAKE) product_default; \

	else \
		echo ".. PRODUCT variable not defined $(CPUBOARD).., making all products";exec $(MAKE) every; \
	fi


# Used by commonrules to defined distinct targets for different makes
COMMONPREF=monitor

# define the standard targets this way since there is no make files below
# to pass to. Doing a "make clean" here doesn't work. These rules are defined
# in commonrules
#
clean: $(COMMONPREF)clean

clobber: $(COMMONPREF)clobber

depend: $(COMMONPREF)depend

rmtargets: $(COMMONPREF)rmtargets


include $(COMMONRULES)

MKDEPRULE=$(EVERYPRODUCT_MKDEPRULE)



# Make library for all products
every: $(_FORCE)
	@for p in $(EVERYPRODUCT); do \
		echo "Making libpdiag for PRODUCT $$p."; \
		$(MAKE) PRODUCT=$$p $(MKARGS); \
	done

# Depends on the library
product_default: $(TARGETS)

install: $(_FORCE)
	@if test -n "$(PRODUCT)"; then \
		exec $(MAKE) product_install; \
	else \
		exec $(MAKE) every_install; \
	fi

$(OAREA): $(_FORCE)
	@if test ! -d $@; then \
		echo "\trm -rf $@; mkdir $@;" \
		rm -rf $@; mkdir $@; \
	fi

# Which depends on the objects
libpdiag.a:	$(OBJECTS)
		$(BLDLIB)
