#ident "$Header: "
#include <sys/types.h>
#include <klib/hwreg.h>

char hwreg_dir_stab[] =
	"x\000"
	"DIR_PREM_A_HI\000"
	"FINE_BIT_VEC<37:00>\000"
	"UNUSED\000"
	"ECC\000"
	"Applies only to state 0 (shared) fine mode\000"
	"DIR_PREM_A_LO\000"
	"FINE_BIT_VEC<63:38>\000"
	"ONE_CNT\000"
	"OCT\000"
	"STATE\000"
	"PRIORITY\000"
	"AX\000"
	"DIR_PREM_B_HI\000"
	"COARSE_BIT_VEC<37:00>\000"
	"Applies only to state 0 (shared) coarse mode\000"
	"DIR_PREM_B_LO\000"
	"COARSE_BIT_VEC<63:38>\000"
	"DIR_PREM_C_HI\000"
	"CW_OFFSET\000"
	"Applies to all states except 0 (shared)\000"
	"DIR_PREM_C_LO\000"
	"UNUSED1\000"
	"POINTER\000"
	"UNUSED2\000"
	"DIR_STD_A_HI\000"
	"FINE_BIT_VEC<10:00>\000"
	"Applies only to state 0 (shared)\000"
	"DIR_STD_A_LO\000"
	"FINE_BIT_VEC<15:11>\000"
	"DIR_STD_C_HI\000"
	"DIR_STD_C_LO\000"
	"DIR_PROT_PREM\000"
	"IO_PROTECT\000"
	"REF_COUNT\000"
	"MIG_MODE\000"
	"PROTECTION\000"
	"DIR_PROT_STD\000"
;

hwreg_field_t hwreg_dir_fields[] = {
	{ 16, 47, 10, 6, 1, 0x0LL },
	{ 36, 9, 7, 6, 1, 0x0LL },
	{ 43, 6, 0, 6, 1, 0x0LL },
	{ 104, 47, 22, 6, 1, 0x0LL },
	{ 124, 21, 16, 6, 1, 0x0LL },
	{ 132, 15, 13, 6, 1, 0x0LL },
	{ 136, 12, 12, 6, 1, 0x0LL },
	{ 142, 11, 8, 6, 1, 0x0LL },
	{ 151, 7, 7, 6, 1, 0x0LL },
	{ 43, 6, 0, 6, 1, 0x0LL },
	{ 168, 47, 10, 6, 1, 0x0LL },
	{ 36, 9, 7, 6, 1, 0x0LL },
	{ 43, 6, 0, 6, 1, 0x0LL },
	{ 249, 47, 22, 6, 1, 0x0LL },
	{ 124, 21, 16, 6, 1, 0x0LL },
	{ 136, 15, 12, 6, 1, 0x0LL },
	{ 142, 11, 8, 6, 1, 0x0LL },
	{ 151, 7, 7, 6, 1, 0x0LL },
	{ 43, 6, 0, 6, 1, 0x0LL },
	{ 36, 47, 10, 6, 1, 0x0LL },
	{ 285, 9, 7, 6, 1, 0x0LL },
	{ 43, 6, 0, 6, 1, 0x0LL },
	{ 349, 47, 33, 6, 1, 0x0LL },
	{ 357, 32, 22, 6, 1, 0x0LL },
	{ 365, 21, 16, 6, 1, 0x0LL },
	{ 136, 15, 12, 6, 1, 0x0LL },
	{ 142, 11, 8, 6, 1, 0x0LL },
	{ 151, 7, 7, 6, 1, 0x0LL },
	{ 43, 6, 0, 6, 1, 0x0LL },
	{ 386, 15, 5, 6, 1, 0x0LL },
	{ 43, 4, 0, 6, 1, 0x0LL },
	{ 452, 15, 11, 6, 1, 0x0LL },
	{ 36, 10, 10, 6, 1, 0x0LL },
	{ 136, 9, 7, 6, 1, 0x0LL },
	{ 142, 6, 6, 6, 1, 0x0LL },
	{ 151, 5, 5, 6, 1, 0x0LL },
	{ 43, 4, 0, 6, 1, 0x0LL },
	{ 36, 15, 8, 6, 1, 0x0LL },
	{ 285, 7, 5, 6, 1, 0x0LL },
	{ 43, 4, 0, 6, 1, 0x0LL },
	{ 357, 15, 10, 6, 1, 0x0LL },
	{ 136, 9, 7, 6, 1, 0x0LL },
	{ 142, 6, 6, 6, 1, 0x0LL },
	{ 151, 5, 5, 6, 1, 0x0LL },
	{ 43, 4, 0, 6, 1, 0x0LL },
	{ 512, 47, 45, 6, 1, 0x0LL },
	{ 36, 44, 25, 6, 1, 0x0LL },
	{ 523, 24, 5, 6, 1, 0x0LL },
	{ 533, 4, 3, 6, 1, 0x0LL },
	{ 542, 2, 0, 6, 1, 0x0LL },
	{ 523, 15, 5, 6, 1, 0x0LL },
	{ 533, 4, 3, 6, 1, 0x0LL },
	{ 542, 2, 0, 6, 1, 0x0LL },
};

hwreg_t hwreg_dir_regs[] = {
	{ 2, 47, 0x0LL, 0, 3 },
	{ 90, 47, 0x0LL, 3, 7 },
	{ 154, 190, 0x0LL, 10, 3 },
	{ 235, 190, 0x0LL, 13, 6 },
	{ 271, 295, 0x0LL, 19, 3 },
	{ 335, 295, 0x0LL, 22, 7 },
	{ 373, 406, 0x0LL, 29, 2 },
	{ 439, 406, 0x0LL, 31, 6 },
	{ 472, 295, 0x0LL, 37, 3 },
	{ 485, 295, 0x0LL, 40, 5 },
	{ 498, 0, 0x0LL, 45, 5 },
	{ 553, 0, 0x0LL, 50, 3 },
	{ 0, 0, 0, 0 }
};

hwreg_set_t hwreg_dir = {
	hwreg_dir_regs,
	hwreg_dir_fields,
	hwreg_dir_stab,
	12,
};
