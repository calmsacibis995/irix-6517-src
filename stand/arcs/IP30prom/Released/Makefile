#ident "IP30prom/Released/Makefile: $Revision: 1.5 $"
#
# Makefile for IP30 PROM
#
# This makefile does just about nothing.
# handles installing the check'd-in image into the images ...
#

DEPTH= ../..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)
include $(CPUARCHDEFS)

#ignored targets
default exports clobber clean lint tags incdepend depend fluff:

install: IP30prom.bin
	$(INSTALL) -idb 'mach(CPUBOARD=${CPUBOARD})' \
	    -idb 'exitop("if test @$$instmode = @normal;then dvhtool -v creat $$rbase/usr/cpu/firmware/IP30prom.bin IP30prom $$vhdev>/dev/null 2>&1; $$rbase/usr/sbin/runon 0 $$rbase/usr/sbin/flash -p $$rbase/usr/cpu/firmware; exit 0; fi")' \
	    -m 644 -F /usr/cpu/firmware IP30prom.bin

IP30prom.hex: IP30prom.bin
	${CONVERT} -p -f intelhex IP30prom.bin > IP30prom.hex
