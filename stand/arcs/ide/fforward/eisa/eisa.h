/*
 * eisa.h - EIU/INTEL register definitions
 */

#define EIU_BASE     		0x0
#define EIU_MODE_OFFSET		0x9ffc0
#define EIU_STAT_OFFSET		0x9ffc4
#define EIU_PREEMP_OFFSET	0x9ffc8
#define EIU_QTIME_OFFSET	0x9ffcc

#define EIU_MODE_B26		0x04000000
#define EIU_MODE_B30      	0x40000000
#define EIU_TESTVAL1     	0x0000
#define EIU_TESTVAL2    	0xaaaa
#define EIU_TESTVAL3    	0xcccc
#define EIU_TESTVAL4    	0xf0f0
#define EIU_TESTVAL5    	0xff00
#define EIU_TESTVAL6    	0xffff

#define INTEL_BASE		0x80000
#define	INTEL_REG1_OFFSET	0x48f
#define	INTEL_REG2_OFFSET	0x489

#define INTEL_TESTREG		0xd4
