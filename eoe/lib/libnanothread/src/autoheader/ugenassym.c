#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#define _KMEMUSER
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/kusharena.h>
#include <sys/ucontext.h>

typedef enum {
	HEX,
	DEC,
	ABI_START,
	ABI_END,
	UNDEF_FORMAT
} format_t;

#define PDEC(name, value) name, DEC, value,
#define PHEX(name, value) name, HEX, value,

struct entry_s {
	char *name;
	format_t format;
	size_t value;
} entry[] = {

#ifdef USE_PTHREAD_RSA

        PHEX("PRDA", (uint64_t)PRDA)
        PHEX("PRDA_NID", offsetof(struct prda, t_sys.t_nid))
	PHEX("PRDA_UNUSED0", offsetof(struct prda, t_sys.t_unused[0]))
	PHEX("PRDA_UNUSED1", offsetof(struct prda, t_sys.t_unused[1]))
	PHEX("PRDA_UNUSED2", offsetof(struct prda, t_sys.t_unused[2]))
	PHEX("PRDA_UNUSED3", offsetof(struct prda, t_sys.t_unused[3]))
	PHEX("PRDA_UNUSED4", offsetof(struct prda, t_sys.t_unused[4]))
	PHEX("PRDA_UNUSED5", offsetof(struct prda, t_sys.t_unused[5]))
        PHEX("KUS_RSA", offsetof(struct kusharena, rsa))
        PHEX("KUS_NREQUESTED", offsetof(struct kusharena, nrequested))
        PHEX("KUS_NALLOCATED", offsetof(struct kusharena, nallocated))
        PHEX("KUS_RBITS", offsetof(struct kusharena, rbits))
        PHEX("KUS_FBITS", offsetof(struct kusharena, fbits))
        PHEX("KUS_NIDTORSA", offsetof(struct kusharena, nid_to_rsaid))
#ifdef YIELDCOUNT
	PHEX("KUS_YIELDP", offsetof(struct kusharena, yieldp))
	PHEX("FYIELD_OFFSET", sizeof(int)*1024)
#endif
	PHEX("KUS_PAD", offsetof(struct kusharena, cacheline_filler))
        PDEC("RSA_SIZE", sizeof(rsa_t))
        PDEC("PADDED_RSA_SIZE", sizeof(padded_rsa_t))
        PDEC("NT_MAXNIDS", NT_MAXNIDS )
        PDEC("NT_MAXRSAS", NT_MAXRSAS )
	PDEC("NULL_NID", NULL_NID)

        PDEC("RSA_R0", offsetof(struct rsa, rsa_context.gregs[CTX_R0]))
        PDEC("RSA_A0", offsetof(struct rsa, rsa_context.gregs[CTX_A0]))
        PDEC("RSA_A1", offsetof(struct rsa, rsa_context.gregs[CTX_A1]))
        PDEC("RSA_A2", offsetof(struct rsa, rsa_context.gregs[CTX_A2]))
        PDEC("RSA_A3", offsetof(struct rsa, rsa_context.gregs[CTX_A3]))
        PDEC("RSA_AT", offsetof(struct rsa, rsa_context.gregs[CTX_AT]))
        PDEC("RSA_FP", offsetof(struct rsa, rsa_context.gregs[CTX_S8]))
        PDEC("RSA_GP", offsetof(struct rsa, rsa_context.gregs[CTX_GP]))
        PDEC("RSA_RA", offsetof(struct rsa, rsa_context.gregs[CTX_RA]))
        PDEC("RSA_S0", offsetof(struct rsa, rsa_context.gregs[CTX_S0]))
        PDEC("RSA_S1", offsetof(struct rsa, rsa_context.gregs[CTX_S1]))
        PDEC("RSA_S2", offsetof(struct rsa, rsa_context.gregs[CTX_S2]))
        PDEC("RSA_S3", offsetof(struct rsa, rsa_context.gregs[CTX_S3]))
        PDEC("RSA_S4", offsetof(struct rsa, rsa_context.gregs[CTX_S4]))
        PDEC("RSA_S5", offsetof(struct rsa, rsa_context.gregs[CTX_S5]))
        PDEC("RSA_S6", offsetof(struct rsa, rsa_context.gregs[CTX_S6]))
        PDEC("RSA_S7", offsetof(struct rsa, rsa_context.gregs[CTX_S7]))
        PDEC("RSA_SP", offsetof(struct rsa, rsa_context.gregs[CTX_SP]))
#if (_MIPS_SIM == _ABIO32)
        PDEC("RSA_T0", offsetof(struct rsa, rsa_context.gregs[CTX_T0]))
        PDEC("RSA_T1", offsetof(struct rsa, rsa_context.gregs[CTX_T1]))
        PDEC("RSA_T2", offsetof(struct rsa, rsa_context.gregs[CTX_T2]))
        PDEC("RSA_T3", offsetof(struct rsa, rsa_context.gregs[CTX_T3]))
        PDEC("RSA_T4", offsetof(struct rsa, rsa_context.gregs[CTX_T4]))
        PDEC("RSA_T5", offsetof(struct rsa, rsa_context.gregs[CTX_T5]))
        PDEC("RSA_T6", offsetof(struct rsa, rsa_context.gregs[CTX_T6]))
        PDEC("RSA_T7", offsetof(struct rsa, rsa_context.gregs[CTX_T7]))
#endif
#if (_MIPS_SIM == _ABIN32) || (_MIPS_SIM == _ABI64)
        PDEC("RSA_A4", offsetof(struct rsa, rsa_context.gregs[CTX_A4]))
        PDEC("RSA_A5", offsetof(struct rsa, rsa_context.gregs[CTX_A5]))
        PDEC("RSA_A6", offsetof(struct rsa, rsa_context.gregs[CTX_A6]))
        PDEC("RSA_A7", offsetof(struct rsa, rsa_context.gregs[CTX_A7]))
        PDEC("RSA_T0", offsetof(struct rsa, rsa_context.gregs[CTX_T0]))
        PDEC("RSA_T1", offsetof(struct rsa, rsa_context.gregs[CTX_T1]))
        PDEC("RSA_T2", offsetof(struct rsa, rsa_context.gregs[CTX_T2]))
        PDEC("RSA_T3", offsetof(struct rsa, rsa_context.gregs[CTX_T3]))
#endif
        PDEC("RSA_T8", offsetof(struct rsa, rsa_context.gregs[CTX_T8]))
        PDEC("RSA_T9", offsetof(struct rsa, rsa_context.gregs[CTX_T9]))
        PDEC("RSA_V0", offsetof(struct rsa, rsa_context.gregs[CTX_V0]))
        PDEC("RSA_V1", offsetof(struct rsa, rsa_context.gregs[CTX_V1]))

        PDEC("RSA_MDHI", offsetof(struct rsa, rsa_context.gregs[CTX_MDHI]))
        PDEC("RSA_MDLO", offsetof(struct rsa, rsa_context.gregs[CTX_MDLO]))
        PDEC("RSA_EPC", offsetof(struct rsa, rsa_context.gregs[CTX_EPC]))

#define rsa_fpreg rsa_context.fpregs.fp_r.fp_regs

	PDEC("RSA_FV0", offsetof(struct rsa, rsa_fpreg[CTX_FV0]))
	PDEC("RSA_FV1", offsetof(struct rsa, rsa_fpreg[CTX_FV1]))
	PDEC("RSA_FA0", offsetof(struct rsa, rsa_fpreg[CTX_FA0]))
	PDEC("RSA_FA1", offsetof(struct rsa, rsa_fpreg[CTX_FA1]))
	PDEC("RSA_FA2", offsetof(struct rsa, rsa_fpreg[CTX_FA2]))
	PDEC("RSA_FA3", offsetof(struct rsa, rsa_fpreg[CTX_FA3]))
	PDEC("RSA_FA4", offsetof(struct rsa, rsa_fpreg[CTX_FA4]))
	PDEC("RSA_FA5", offsetof(struct rsa, rsa_fpreg[CTX_FA5]))
	PDEC("RSA_FA6", offsetof(struct rsa, rsa_fpreg[CTX_FA6]))
	PDEC("RSA_FA7", offsetof(struct rsa, rsa_fpreg[CTX_FA7]))
	PDEC("RSA_FT0", offsetof(struct rsa, rsa_fpreg[CTX_FT0]))
	PDEC("RSA_FT1", offsetof(struct rsa, rsa_fpreg[CTX_FT1]))
	PDEC("RSA_FT2", offsetof(struct rsa, rsa_fpreg[CTX_FT2]))
	PDEC("RSA_FT3", offsetof(struct rsa, rsa_fpreg[CTX_FT3]))
	PDEC("RSA_FT4", offsetof(struct rsa, rsa_fpreg[CTX_FT4]))
	PDEC("RSA_FT5", offsetof(struct rsa, rsa_fpreg[CTX_FT5]))
	PDEC("RSA_FT6", offsetof(struct rsa, rsa_fpreg[CTX_FT6]))
	PDEC("RSA_FT7", offsetof(struct rsa, rsa_fpreg[CTX_FT7]))
	PDEC("RSA_FT8", offsetof(struct rsa, rsa_fpreg[CTX_FT8]))
	PDEC("RSA_FT9", offsetof(struct rsa, rsa_fpreg[CTX_FT9]))
	PDEC("RSA_FT10", offsetof(struct rsa, rsa_fpreg[CTX_FT10]))
	PDEC("RSA_FT11", offsetof(struct rsa, rsa_fpreg[CTX_FT11]))
	PDEC("RSA_FT12", offsetof(struct rsa, rsa_fpreg[CTX_FT12]))
	PDEC("RSA_FT13", offsetof(struct rsa, rsa_fpreg[CTX_FT13]))
#if (_MIPS_SIM == _ABIN32)
	PDEC("RSA_FT14", offsetof(struct rsa, rsa_fpreg[CTX_FT14]))
	PDEC("RSA_FT15", offsetof(struct rsa, rsa_fpreg[CTX_FT15]))
#endif
	PDEC("RSA_FS0", offsetof(struct rsa, rsa_fpreg[CTX_FS0]))
	PDEC("RSA_FS1", offsetof(struct rsa, rsa_fpreg[CTX_FS1]))
	PDEC("RSA_FS2", offsetof(struct rsa, rsa_fpreg[CTX_FS2]))
	PDEC("RSA_FS3", offsetof(struct rsa, rsa_fpreg[CTX_FS3]))
	PDEC("RSA_FS4", offsetof(struct rsa, rsa_fpreg[CTX_FS4]))
	PDEC("RSA_FS5", offsetof(struct rsa, rsa_fpreg[CTX_FS5]))
#if (_MIPS_SIM == _ABI64)
	PDEC("RSA_FS6", offsetof(struct rsa, rsa_fpreg[CTX_FS6]))
	PDEC("RSA_FS7", offsetof(struct rsa, rsa_fpreg[CTX_FS7]))
#endif
	PDEC("RSA_FPC_CSR", offsetof(struct rsa, rsa_context.fpregs.fp_csr))
#endif /* USE_PTHREAD_RSA */

	NULL, UNDEF_FORMAT, 0
};

#if (_MIPS_SIM == _ABIO32)
const char *abi = "_ABIO32";
#endif
#if (_MIPS_SIM == _ABIN32)
const char *abi = "_ABIN32";
#endif
#if (_MIPS_SIM == _ABI64)
const char *abi = "_ABI64";
#endif

int
main(int argc, char *argv[])
{
   int i = 0;
   char line[256], *lptr;
   do {
      lptr = line;
      while ((*(lptr++) = getchar()) != 10) {
	 if (feof(stdin)) {
	    break;
	 }
      }
      if (feof(stdin)) {
	 fprintf(stderr, "Could not locate ASM section of input.\n");
	 fprintf(stderr, "This section proceeds a line containing defined(_LANGUAGE_ASSEMBLY)\n");
	 exit(1);
      }
      *lptr = NULL;
      printf("%s", line);
   } while(strspn(line, "#if defined(_LANGUAGE_ASSEMBLY)") != 31);
   printf("#if (_MIPS_SIM == %s)\n", abi);
   do {
      switch (entry[i].format) {
	 case HEX:
	    printf("#define %s 0x%x\n", entry[i].name, entry[i].value);
	    break;
	 case DEC:
	    printf("#define %s %d\n", entry[i].name, entry[i].value);
	    break;
	 case UNDEF_FORMAT:
	    fprintf(stderr, "Undefined format for <%s> (entry[%d]).\n",
		    entry[i].name, i);
	    return -1;
      }
      i++;
   } while (entry[i].name != NULL);
   printf("#endif /* (_MIPS_SIM == %s) */\n", abi);
   while(gets(line)) {
      printf("%s\n", line);
      if (feof(stdin)) {
	 break;
      }
   }
   return 0;
}
