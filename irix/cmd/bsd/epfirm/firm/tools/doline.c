/* DOLINE.C -- MAIN LINE-PROCESSING FUNCTION                    */

#include "asm29.h"

/*
 * "DOLINE.C" -- 29000 assembler routine to process instructions or directives,
 *   entered with 'inst' pointing to a reserved-word entry.  The routine deter-
 *   mines the type of instruction or directive it is and processes it, calling
 *   appropriate functions to generate relocatable items and output data to the
 *   COFF file.  It also buffers data for the listing file.
 *
 *   NOTE:  The reserved word table stores opcodes as 2-byte shorts.  They must
 *     be converted to 4-byte words before being output to the COFF file.  This
 *     is done in the following routine.
 */

doline()
{
    char buf[256];
    long hold;
    int i;
    char i_fmt;
    long lx;
    char *mark;
    long reg1;
    long reg2;
    long save;
    struct value v;
    struct value v2;

    if (inst == NULL) return;
    if (ifstk[ifsp] && !(inst->rs_flags & R_UNCOND)) return;
    i_fmt = inst->rs_format;

    if (i_fmt != F_PSOP) {
	    if (saptr != NULL) {
		    error (e_symatt);
		    return;
	    }

	    if (cs_flags (STYP_BSS)) {
		    error (e_bss_op);
		    return;
	    }

	    align (4);			/* align the PC                 */
    }

    lwords = 0;
	zap (&v);
	zap (&v2);

	switch (i_fmt) {
		case F_PSOP:
			do_psop();
			break;
		case F_CAB:		/* feq, deq, fge, etc */
			v.v_val = (long)inst->rs_opcode << 16;
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+1);
			v.v_val |= (v2.v_val << 16);
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+3);
			v.v_val |= v2.v_val;
			genvword (&v);
			break;
		case F_MCAB:		/* add, sub, etc */
			v.v_val= (long)inst->rs_opcode << 16;
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+1);
			v.v_val |= (v2.v_val << 16);
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			(void)comma (1);
			mark = lptr;
			if (getreg (&v2, 0) == -1)
				{
				lptr = mark;
				getintval (&v2, 0L, 255L);
				v.v_val |= (v2.v_val | M_BIT);
				}
			  else
				v.v_val |=  v2.v_val;
			genbrel (&v2, pc+3);
			genvword (&v);
			break;
		case F_VAB:                                     /* emulate */
			v.v_val = (long)inst->rs_opcode << 16;
			/* old:  v.v_val |= (( getintb (0L, 255L) & 0x0ff) << 16); */
			/* new:  to allow relocation */
			getintval (&v2, 0L, 255L);
			genbrel (&v2, pc+1);
			v.v_val |= ((v2.v_val & 0x0ff) << 16);
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			genvword (&v);
			break;
		case F_MVAB:                            /* aseq, aslt, asge, etc */
			v.v_val = (long)inst->rs_opcode << 16;
			/* old:  v.v_val |= (( getintb (0L, 255L) & 0x0ff) << 16); */
			/* new:  to allow relocation */
			getintval (&v2, 0L, 255L);
			genbrel (&v2, pc+1);
			v.v_val |= ((v2.v_val & 0x0ff) << 16);
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			(void)comma (1);
			mark = lptr;
			if (getreg(&v2, 0) == -1)
				{
				lptr = mark;
				getintval (&v2, 0L, 255L);
				v.v_val |= (v2.v_val | M_BIT);
				}
			  else
				v.v_val |=  v2.v_val;
			genbrel (&v2, pc+3);
			genvword (&v);
			break;
		case F_AB:		/* calli, jmpti, etc */
			v.v_val = (long)inst->rs_opcode << 16;
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+3);
			v.v_val |= v2.v_val;
			genvword (&v);
			break;
		case F_CA:
			v.v_val = (long)inst->rs_opcode << 16;
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+1);
			v.v_val |= (v2.v_val << 16);
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			genvword (&v);
			break;
		case F_CACA:                            /* convert */
			v.v_val = (long)inst->rs_opcode << 16;
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+1);
			v.v_val |= (v2.v_val << 16);
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			(void)comma (1);
			v2.v_val = getintb (0L, 1L);
			v.v_val |= v2.v_val << 7;
			(void)comma (1);
			v2.v_val = getintb (0L, 7L);
			v.v_val |= v2.v_val << 4;
			(void)comma (1);
			v2.v_val = getintb (0L, 3L);
			v.v_val |= (v2.v_val << 2);
			(void)comma (1);
			v2.v_val = getintb (0L, 3L);
			v.v_val |= (v2.v_val);
			genvword (&v);
			break;
		case F_B:                                       /* jmpi */
			v.v_val = (long)inst->rs_opcode << 16;
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+3);
			v.v_val |= v2.v_val;
			genvword (&v);
			break;
		case F_:                                        /* halt, inv, iret, iretinv */
			v.v_val = (long)inst->rs_opcode << 16;
			genvword (&v);
			break;
		case F_NOP:                                     /* special case: a NOP */
			v.v_val = 0x70400101;   /* an ASEQ 40#h, gr1, gr1 */
			genvword (&v);
			break;
		case F_MCB:                                     /* clz */
			v.v_val= (long)inst->rs_opcode << 16;
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+1);
			v.v_val |= (v2.v_val << 16);
			(void)comma (1);
			mark = lptr;
			if (getreg(&v2, 0) == -1)
				{
				lptr = mark;
				getintval (&v2, 0L, 255L);
				v.v_val |= (v2.v_val | M_BIT);
				}
			  else
				v.v_val |=  v2.v_val;
			genbrel (&v2, pc+3);
			genvword (&v);
			break;
		case F_SB:                                      /* mfsr */
			v.v_val = (long)inst->rs_opcode << 16;
			getspreg (&v2);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+3);
			v.v_val |= v2.v_val;
			genvword (&v);
			break;
		case F_CS:                                      /* mtsr */
			v.v_val = (long)inst->rs_opcode << 16;
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+1);
			v.v_val |= ( v2.v_val << 16);
			getspreg (&v2);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			genvword (&v);
			break;
		case F_0A:                                      /* const, consth, constn */
			v.v_val = (long)inst->rs_opcode << 16;
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			(void)comma (1);
			getval (&v2);                   /* old $ code */
			if  (inst->rs_opcode == OP_CONSTH)
				{
				save = v2.v_val;
				v2.v_val = ( v2.v_val >> 16 );
				v.v_val |= ((v2.v_val & 0x0ff) | ((v2.v_val & 0x0ff00) << 8));
				v2.v_val = v.v_val;
				if (v2.v_ext || v2.v_seg)
					v2.v_reloc = R_IHIHALF;
				genvword (&v2);
				if (v2.v_reloc)
					{
					hold = pc;
					pc -= 4;
					genrel (save, R_IHCONST);
					/* need extra relocation item to get correct
					   value in linker after relocation */
					pc = hold;
					}
				}
			  else
				{
				v.v_val |= ((v2.v_val & 0x0ff) | ((v2.v_val & 0x0ff00) << 8));
				v2.v_val = v.v_val;
				if (v2.v_ext || v2.v_seg) v2.v_reloc = R_ILOHALF;
				genvword (&v2);
				}
			break;
		case F_0S:                                      /* mtsrim */
			v.v_val = (long)inst->rs_opcode << 16;
			getspreg (&v2);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			(void)comma (1);
			getintval (&v2, 0L, 65535L);
			v2.v_val = ((v2.v_val & 0x0ff) | ((v2.v_val & 0x0ff00) << 8));

#ifdef OLD
			if (v2.v_ext) v2.v_reloc = R_ILOHALF;
#else                           /* BUG FIX 11/11/88             */
			if ((v2.v_ext != NULL) || (v2.v_seg != 0))
			{
			    v2.v_reloc = R_ILOHALF;
			}
#endif

			v2.v_val |=  v.v_val;
			genvword (&v2);
			break;
		case F_MEAB:                            /* load, store, etc */
			v.v_val= (long)inst->rs_opcode << 16;
			if (getintb (0L, 1L))
				v.v_val |= CE_BIT;      /* coprocessor select bit */
			(void)comma (1);
			v.v_val |= ((getintb (0L, 127L) << 16));        /* control field */
			(void)getreg(&v2, 1);
			genbrel (&v2, pc+2);
			v.v_val |= (v2.v_val << 8);
			(void)comma (1);
			mark = lptr;
			if (getreg (&v2, 0) == -1) {
				lptr = mark;
				getval (&v2);
				(void)bcheck(v2.v_val, 0L, 255L);
				v.v_val |= (v2.v_val | M_BIT);
			} else {
				v.v_val |=  v2.v_val;
			}
			genbrel (&v2, pc+3);
			genvword (&v);
			break;
		case F_CALLX:
			(void)getreg(&v2, 1);
			reg1 = v2.v_val;
			(void)comma (1);
			(void)getreg(&v2, 1);
			reg2 = v2.v_val;
			(void)comma (1);
			getval (&v2);
			if (v2.v_ext != NULL)
				goto far_call;          /* externals */
			if (v2.v_seg != curseg)
				goto far_call;          /* other sections */
#ifdef OLD
			if (abs (v2.v_val - pc) > 131071)
				goto far_call;          /* a far call */
#else
			lx = v2.v_val - pc;
			if (lx < 0) lx = (-lx);
			if (lx > 131071L) goto far_call;
#endif
			if (pass == 3)
				if (!midpass)
					if (v2.v_val > pc)
						goto far_call;  /* all fwd references are far calls */
			/* else this is a real live near call */
			v.v_val = 0xa8000000 | (reg1 << 8)  ;
			goto near_call;
far_call:
			v.v_val = 0x03000000 | (reg2 << 8) ;
			v.v_val |= ((v2.v_val & 0x0ff) | ((v2.v_val & 0x0ff00) << 8));
			v.v_ext = v2.v_ext;
			v.v_seg = v2.v_seg;
			if (v.v_ext || v.v_seg)
				v.v_reloc = R_ILOHALF;
			genvword (&v);
			v.v_val = 0x02000000 | (reg2 << 8);
			save = v2.v_val;
			v2.v_val = (v2.v_val >> 16);
			v.v_val |= ((v2.v_val & 0x0ff) | ((v2.v_val & 0x0ff00) << 8));
			v2.v_val = v.v_val;
			if (v2.v_ext || v2.v_seg ) v2.v_reloc = R_IHIHALF;
			genvword (&v2);
			if (v2.v_reloc)
				{
				hold = pc;
				pc -= 4;
				genrel(save, R_IHCONST);
				pc = hold;
				}
			zap(&v);
			v.v_val = 0xc8000000 | (reg1 << 8) | reg2;
			genvword (&v);
			break;
		case F_A2A:                                     /* call, jmpf, etc */
			v.v_val = (long)inst->rs_opcode << 16;
			/* if JMP then no register operand */
			if (v.v_val != 0xa0000000) {
				(void)getreg(&v2, 1);
				genbrel (&v2, pc+2);
				v.v_val |= (v2.v_val  << 8);
				(void)comma (1);
			}
			getval (&v2);
			v2.v_reloc = R_IREL;
			/* if value is external or in another sect
			 * then reloc */
near_call:
			/* The value is in the current segment or
			 * it is absolute and the current segment is
			 * also absolute.
			 */
			if (v2.v_seg == curseg ||
			    (v2.v_seg == 0 &&
			     cursegp->sg_head.s_flags & STYP_ABS))
				{
#ifdef OLD
				if (abs(v2.v_val - pc) > 131071)
#else
				lx = v2.v_val - pc;
				if (lx < 0) lx = (-lx);
				if (lx > 131071L)
#endif
					{
					v.v_val |= 0x01000000;
					v2.v_reloc = R_IABS;
					if (v2.v_val > 262143) error (e_size);
					}
				  else
					v2.v_val = v2.v_val - (pc);
				v2.v_seg = 0;           /* no reloc needed */
				v2.v_reloc = 0;
				}
			if (v2.v_val & 0x03)
				error (e_unalign);      /* addr word alignd? */
			v.v_val |= (((v2.v_val & 0x03fc) >> 2) |
				((v2.v_val & 0x03fc00) << 6));
			v2.v_val = v.v_val;
			genvword (&v2);
			break;
		}
	deblank();
	if (!eol())
		{
		i = pageline;
		error (e_extra);

		if ((pass == 3) && (pageline != i))
			{
			pageline += 2;
			sprintf (buf,"%s   >>%s\n\n",
			    "    Problem occurred near: ",
				lptr);
			buf[75] = EOS; /* clip if necessary */
			printf (buf);
			}
		}
}
