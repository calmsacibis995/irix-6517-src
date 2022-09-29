#ident	"include/sys/fpu.h:  $Revision: 1.5 $"

/*
 * fpu.h -- floating point coprocessor specific defines
 */

#define FPREV_REVISION_MASK 0xff
#define FPREV_IMPLEMENTATION_MASK 0xff00

/* FPU co-processor definitions 
 */
#if	defined(_LANGUAGE_ASSEMBLY)
# define __C1REG__(x)	$x
#else
# define __C1REG__(x)	x
#endif

#define	C1_F0		__C1REG__(0)
#define	C1_F1		__C1REG__(1)
#define	C1_STATUS	__C1REG__(31)

#define RN 0
#define RZ 1
#define RP 2
#define RM 3

#define FP_COND		0x00800000

/* Exception bits */

#define FP_EXC		0x0003f000
#define FP_EXC_E	0x00020000
#define FP_EXC_V	0x00010000
#define FP_EXC_Z	0x00008000
#define FP_EXC_O	0x00004000
#define FP_EXC_U	0x00002000
#define FP_EXC_I	0x00001000

/* Enables */

#define FP_ENABLE	0x00000f80
#define FP_EN_V		0x00000800
#define FP_EN_Z		0x00000400
#define FP_EN_O		0x00000200
#define FP_EN_U		0x00000100
#define FP_EN_I		0x00000080

/* Sticky bits */

#define FP_STKY		0x0000007c
#define FP_STKY_V	0x00000040
#define FP_STKY_Z	0x00000020
#define FP_STKY_O	0x00000010
#define FP_STKY_U	0x00000008
#define FP_STKY_I	0x00000004

/* Rounding Modes */

#define FP_RMODE	0x00000003
#define FP_RN	 	0x00000000
#define FP_RZ		0x00000001
#define FP_RP		0x00000002
#define FP_RM		0x00000003
#define FP_RN_MASK 	0x00000003
#define FP_RZ_MASK	0x00000002
#define FP_RP_MASK	0x00000001
#define FP_RM_MASK	0x00000000

#if	defined(_LANGUAGE_ASSEMBLY)
#define fpc_irr	$0
#define fpc_led	$0
#define fpc_eir	$30
#define fpc_csr	$31
#endif
#define IMPLEMENTATION_NONE	0	/* software */
#define IMPLEMENTATION_R2360	0x100	/* board */
#define IMPLEMENTATION_R2010	0x200	/* chip */
