#!smake
#
# "$Revision: 1.6 $"

include ../../Makedefs

TARGETS_TLIB =	combo.tlib confirm.tlib cvt.tlib err.tlib fsu.tlib \
		hostUtil.tlib info.tlib ipanel.tlib list.tlib matrix.tlib \
		mu.tlib obj.tlib objUtil.tlib svTmpl.tlib tfu.tlib tu.tlib \
		xdk.tlib xfs.tlib xlv.tlib
RBASE=\$$rbase
EXITOP = exitop(chroot $(RBASE) /sbin/sh -c 'cd /usr/xfsm/lib/tlib; /usr/sgitcl/bin/sgitcl -c \\'buildpackageindex [eval exec ls [glob *.tlib]]\\'')


default lint debug $(COMMONTARGS): $(_FORCE)

install:
	$(INSTALL) -u root -g sys -m 0644 -idb "$(EXITOP)" \
		-F $(INSTXFSMTLIB) $(TARGETS_TLIB)

$(_FORCE):
