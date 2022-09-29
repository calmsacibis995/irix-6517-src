#!smake
#
# Makefile for regression tests
#

#OBJECTS = autotest genreport runall
OBJECTS =
#SUBDIRS = X acceptance aim3 scripts
SUBDIRS = X acceptance

default : all

all:	subdirs $(OBJECTS)

subdirs:
	for i in ${SUBDIRS}; do \
	    (cd $$i && $(MAKE) all); \
	done

$(OBJECTS): $$@.sh
	cp $@.sh $@; chmod +x $@

install: all
	for i in ${SUBDIRS}; do \
	    (cd $$i && $(MAKE) install); \
	done
#	/etc/install -F /usr/stress $(OBJECTS)

clean:
	for i in ${SUBDIRS}; do \
	    (cd $$i && $(MAKE) clean); \
	done

clobber:
	for i in ${SUBDIRS}; do \
	    (cd $$i && $(MAKE) clobber); \
	done; \
	rm -f $(OBJECTS)
