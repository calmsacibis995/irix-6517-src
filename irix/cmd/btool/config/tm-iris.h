#include "tm-mips.h"

/* Names to predefine in the preprocessor for this target machine.  */

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-DLANGUAGE_C -Dunix -Dmips -Dsgi -Dhost_mips \
			-DSYSTYPE_SVR4 -DMIPSEB"
#undef CPP_SPEC
/*
 * XXX -xansi defines really
 * XXX doesn't work for -ansi
 * cccp.c defined STDC to be 1
 */
#define CPP_SPEC "-D_MIPSEB -D_SYSTYPE_SVR4 \
		-D_MIPS_FPSET=16 \
		-D_MIPS_SIM=_MIPS_SIM_ABI32 -D_MIPS_SZINT=32 \
		-D_MIPS_SZLONG=32 -D_MIPS_SZPTR=32 -D_LANGUAGE_C \
		-D_MODERN_C -D__DSO__ \
		-D_SVR4_SOURCE -D_SGI_SOURCE -D__EXTENSIONS__ \
		-D__sgi -D__unix -D__host_mips \
		-D_LONGLONG"

#define STARTFILE_SPEC  \
  "%{pg:gcrt1.o%s}%{!pg:%{p:mcrt1.o%s}%{!p:crt1.o%s}}"

#define LIB_SPEC "%{!p:%{!pg:-lc}}%{p:-lc_p}%{pg:-lc_p} crtn.o%s"

#define ASM_OUTPUT_SOURCE_FILENAME(FILE, FILENAME) \
  fprintf (FILE, "\t.file\t1 \"%s\"\n", FILENAME)

#undef ASM_OUTPUT_SOURCE_LINE
#define ASM_OUTPUT_SOURCE_LINE(file, line)              \
  { static int sym_lineno = 1;                          \
    fprintf (file, "\t.loc\t1 %d\nLM%d:\n",     \
             line, sym_lineno);         \
    sym_lineno += 1; }

#undef STACK_ARGS_ADJUST
#define STACK_ARGS_ADJUST(SIZE)                                         \
{                                                                       \
  SIZE.constant += 4;                                                   \
  if (SIZE.var)                                                         \
    {                                                                   \
      rtx size1 = ARGS_SIZE_RTX (SIZE);                                 \
      rtx rounded = gen_reg_rtx (SImode);                               \
      rtx label = gen_label_rtx ();                                     \
      emit_move_insn (rounded, size1);                                  \
      /* Needed: insns to jump to LABEL if ROUNDED is < 16.  */         \
      abort ();                                                         \
      emit_move_insn (rounded, gen_rtx (CONST_INT, VOIDmode, 16));      \
      emit_label (label);                                               \
      SIZE.constant = 0;                                                \
      SIZE.var = rounded;                                               \
    }                                                                   \
  else if (SIZE.constant < 32)                                          \
    SIZE.constant = 32;                                                 \
}

#define WORD_SWITCH_TAKES_ARG(STR) (!strcmp (STR, "Tdata") \
        || (!strcmp(STR, "test-dir")) || (!strcmp(STR, "test-map")) \
        || (!strcmp(STR, "test-control")) || (!strcmp(STR, "test-count")) \
	|| (!strcmp(STR, "woff")) || (!strcmp(STR, "MDupdate")) \
	|| (!strcmp(STR, "Olimit")) || (!strcmp(STR, "MDtarget")) \
	|| (!strcmp(STR, "realcc")) \
	)

#undef SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR)      \
  ((CHAR) == 'D' || (CHAR) == 'U' || (CHAR) == 'o' \
   || (CHAR) == 'e' || (CHAR) == 'T' || (CHAR) == 'u' \
   || (CHAR) == 'I' || (CHAR) == 'Y' /*|| (CHAR) == 'm'*/ \
   || (CHAR) == 'L' || (CHAR) == 'i' || (CHAR) == 'A' \
   || (CHAR) == 'G')


#undef DEFAULT_SIGNED_CHAR
#define DEFAULT_SIGNED_CHAR 0

#undef CC1_SPEC
