/* fppLib.h - floating-point coprocessor support library header */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,02sep93,hdn  deleted a macro FPX.
01c,08jun93,hdn  added support for c++. updated to 5.1.
01b,29sep92,hdn  changed UTINY to UCHAR.
01a,07apr92,hdn  written based on TRON version.
*/

#ifndef	__INCfppI86Libh
#define	__INCfppI86Libh

#ifdef __cplusplus
extern "C" {
#endif


/* FP_CONTEXT structure offsets */

#define	FPCR		0x00	/* OFFSET(FP_CONTEXT, fpcr)		*/
#define	FPSR		0x04	/* OFFSET(FP_CONTEXT, fpsr)		*/
#define	FPTAG		0x08	/* OFFSET(FP_CONTEXT, fptag)		*/
#define	IP_OFFSET	0x0c	/* OFFSET(FP_CONTEXT, ipOffset)		*/
#define	CS_SELECTOR	0x10	/* OFFSET(FP_CONTEXT, csSelector)	*/
#define	OP_OFFSET	0x14	/* OFFSET(FP_CONTEXT, opOffset)		*/
#define	OP_SELECTOR	0x18	/* OFFSET(FP_CONTEXT, opSelector)	*/
/* XXX bad #define	FPX	0x1c	   OFFSET(FP_CONTEXT, fpx[0])	*/
/* XXX ok  #define	FPX	0x0c	   OFFSET(FPREG_SET,  fpx[0])	*/


#ifndef	_ASMLANGUAGE

/* number of fp registers on coprocessor */
#define	FP_NUM_REGS	8

/* maximum size of floating-point coprocessor state frame */
#define FP_STATE_FRAME_SIZE	108

typedef struct	/* DOUBLEX - double extended precision */
    {
    UCHAR f[10];
    } DOUBLEX;

typedef struct fpregSet
    {
    int		fpcr;
    int		fpsr;
    int		fptag;
    DOUBLEX	fpx[FP_NUM_REGS];
    } FPREG_SET;

typedef struct fpContext	/* FP_CONTEXT */
    {
    int		fpcr;				/* control word		:   4 */
    int		fpsr;				/* status word		:   4 */
    int		fptag;				/* tag word		:   4 */
    int		ipOffset;			/* ip offset		:   4 */
    int		csSelector;			/* cs selector		:   4 */
    int		opOffset;			/* operand offset	:   4 */
    int		opSelector;			/* operand selector	:   4 */
    DOUBLEX	fpx[FP_NUM_REGS];		/* 8 extended doubles	:  80 */
    } FP_CONTEXT;				/*	     	  TOTAL	: 108 */

#define FPX_OFFSET(n)	(0xc + (n)*sizeof(DOUBLEX))

/* variable declarations */

extern REG_INDEX fpRegName[];		/* f-point data register table */
extern REG_INDEX fpCtlRegName[];	/* f-point control register table */
extern WIND_TCB *pFppTaskIdPrevious;	/* task id for deferred exceptions */
extern FUNCPTR	 fppCreateHookRtn;	/* arch dependent create hook routine */
extern FUNCPTR	 fppDisplayHookRtn;	/* arch dependent display routine */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern	  void	       fppArchInit (void);
extern	  void	       fppArchTaskCreateInit (FP_CONTEXT *pFpContext);
extern	  STATUS       fppProbeSup (void);
extern	  void         fppProbeTrap (void);
extern	  void         fppDtoDx (DOUBLEX *pDx, double *pDouble);
extern	  void         fppDxtoD (double *pDouble, DOUBLEX *pDx);

#else

extern	  void	       fppArchInit ();
extern	  void	       fppArchTaskCreateInit ();
extern	  STATUS       fppProbeSup ();
extern	  void         fppProbeTrap ();
extern	  void         fppDtoDx ();
extern	  void         fppDxtoD ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCfppI86Libh */
