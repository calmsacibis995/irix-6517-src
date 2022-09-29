#!smake -J 1
#
SUBDIRS = \
	sbcs \
	xkeysyms


default install headers exports all clobber clean:
	-@for cmd in ${SUBDIRS}; do \
	    echo "\n\n==== Make $@ for $$cmd"; \
	    (cd $$cmd; exec ${MAKE} $@); \
	done

