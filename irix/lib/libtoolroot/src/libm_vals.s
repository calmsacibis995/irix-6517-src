#ifdef _MIPSEL
#	define D(h,l) l,h
#endif
#ifdef _MIPSEB
#	define D(h,l) h,l
#endif

	.sdata
	.align 4
	.globl __libm_zero_ld
__libm_zero_ld:
	.word	D(0x00000000,0x0)
	.word	D(0x0,0x0)
	.globl __libm_qnan_ld
__libm_qnan_ld:
	.word	D(0x7FF10000,0x0)
	.word	D(0x0,0x0)
	.globl __libm_inf_ld
__libm_inf_ld:
	.word	D(0x7FF00000,0x0)
	.word	D(0x0,0x0)
	.globl __libm_neginf_ld
__libm_neginf_ld:
	.word	D(0xFFF00000,0x0)
	.word	D(0x0,0x0)
	.globl __libm_qnan_d
__libm_qnan_d:
	.word	D(0x7FF10000,0x0)
	.globl __libm_inf_d
__libm_inf_d:
	.word	D(0x7FF00000,0x0)
	.globl __libm_neginf_d
__libm_neginf_d:
	.word	D(0xFFF00000,0x0)
	.globl __libm_qnan_f
__libm_qnan_f:
	.word	0x7F810000
	.globl __libm_inf_f
__libm_inf_f:
	.word	0x7F800000
	.globl __libm_neginf_f
__libm_neginf_f:
	.word	0xFF800000

