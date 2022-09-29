#ifndef __IP32_STATUS_H_
#define __IP32_STATUS_H_

#ident	"$Revision: 1.1 $"

/*
 * IDE IP32 specific error/status codes.
 */

typedef struct {
	unsigned char	sw;
	unsigned char	func;
	unsigned char	test;
	unsigned char	code;
	unsigned int	w1;
	unsigned int	w2;
}code_t;


/* defines for standard error reporting using the code_t structure */

#define SW_POST_FW	0x04
#define SW_IDE		0x08
#define SW_CONF_TEST	0x0a
#define SW_UNIX_SYSTST	0x0c

#define FUNC_SW		0x00
#define FUNC_CPU	0x01
#define FUNC_FPU	0x02
#define FUNC_CACHE	0x03
#define FUNC_VICE	0x04
#define FUNC_CRIME	0x05
#define FUNC_MEMORY	0x06
#define FUNC_GBE	0x07
#define FUNC_GFX	0x08
#define FUNC_MACE	0X09
#define FUNC_PCI_CORE	0x0a
#define FUNC_PCI_OPTS	0x0b
#define FUNC_SCSI	0x0c
#define FUNC_ENET	0x0d
#define FUNC_SERIAL	0x0e
#define FUNC_GAMEP	0x0f
#define FUNC_PARALLEL	0x10
#define FUNC_RTC_NVRAM	0x11
#define FUNC_FLASHPROM	0x12
#define FUNC_KEYBOARD	0x13
#define FUNC_MOUSE	0x14
#define FUNC_COUNTERS	0x15
#define FUNC_I2C	0x16
#define FUNC_AUDIO	0x17
#define FUNC_VIDEO	0x18
#define FUNC_MISC	0xff

#define NVRAM_ERROR_STACK 50
#define NVRAM_TEST_CODE 16
#define NVRAM_END 0x7f
#define NVRAM_ERROR_SIZE 16

/* Defines for IP32 graphics diagnostics */

#define TEST_GBEUD32			0x01
 #define CODE_GBEUD32_BAD_ARGS		0x04
 #define CODE_GBEUD32_INIT0		0x08
 #define CODE_GBEUD32_WALK1		0x0c
 #define CODE_GBEUD32_INIT1		0x10
 #define CODE_GBEUD32_WALK0		0x14
#define TEST_CMAP_UA			0x02
 #define CODE_GBE_UA_CMAP_VERIF0	0x04
 #define CODE_GBE_UA_CMAP_VERIFUA	0x08
#define TEST_GMAP_UA			0x03
 #define CODE_GBE_UA_GMAP_VERIF0	0x04
 #define CODE_GBE_UA_GMAP_VERIFUA	0x08
#define TEST_MISC_UA			0x04
 #define CODE_GBE_UA_MISC_VERIF0	0x04
 #define CODE_GBE_UA_MISC_VERIFUA	0x08
 #define W1_GBE_OVR_WIDTH_TILE		0x00010000
 #define W1_GBE_FRM_WIDTH_TILE		0x00020000
 #define W1_GBE_MODE_REGS		0x00030000
 #define W1_GBE_CRS_POS			0x00040000
 #define W1_GBE_CRS_CMAP		0x00050000
 #define W1_GBE_CRS_GLYPH		0x00060000
 #define W1_GBE_VC_LR			0x00070000
 #define W1_GBE_VC_TB			0x00080000
#define TEST_REUD64			0x08
 #define CODE_REUD64_BAD_ARGS		0x04
 #define CODE_REUD64_INIT0		0x08
 #define CODE_REUD64_WALK1		0x0c
 #define CODE_REUD64_INIT1		0x10
 #define CODE_REUD64_WALK0		0x14
#define TEST_RE_UA_TLB			0x09
 #define CODE_RE_UA_TLB_VERIF0		0x04
 #define CODE_RE_UA_TLB_VERIFUA		0x08
#define TEST_RE_VIS01			0x10
 #define CODE_RE_VIS01			0x04

void	set_test_code(code_t *);
void	report_error(code_t *);
unsigned char read_nvram(int);
void	write_nvram(int, int, unsigned char *);


#endif
