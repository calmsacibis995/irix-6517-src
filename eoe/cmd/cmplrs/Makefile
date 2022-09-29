#!smake
#ident "$Revision: 1.2 $"
#
# As a part of eoe.sw.unix, install a seperate compiler default file
# for different CPU types:
#
#	R4x00	-> -n32 -mips3
#	R5000	-> -n32 -mips4 -r5000
#	R8000	-> -n32 -mips4 -r8000
#	R10000	-> -n32 -mips4 -r10000

include $(ROOT)/usr/include/make/commondefs

default:

include $(COMMONRULES)

install: default
	${INSTALL} -idb "mach(CPUARCH=R10000) config(update)" \
		-m 644 -src defaults.n32.mips4.r10k -F /etc compiler.defaults
	${INSTALL} -idb "mach(CPUARCH=R8000) config(update)" \
		-m 644 -src defaults.n32.mips4.r8k -F /etc compiler.defaults
	${INSTALL} -idb "mach(CPUARCH=R5000) config(update)" \
		-m 644 -src defaults.n32.mips4.r5k -F /etc compiler.defaults
	${INSTALL} -idb "mach(CPUARCH=R4000) config(update)" \
		-m 644 -src defaults.n32.mips3 -F /etc compiler.defaults
