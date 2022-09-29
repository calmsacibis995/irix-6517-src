/* SGI Private Transfer Vector */

#ifndef __ARCS_PVECTOR_H__
#define __ARCS_PVECTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.29 $"

#include <arcs/signal.h>

typedef struct {
	LONG		(*Ioctl)(ULONG,LONG,LONG);
	LONG		(*GetNvramTab)(char *,int);
	LONG		(*LoadAbs)(CHAR*,ULONG*);
	LONG		(*InvokeAbs)(ULONG,ULONG,LONG,CHAR *[],CHAR *[]);
	LONG		(*ExecAbs)(CHAR *,LONG, CHAR *[], CHAR *[]);
	int		(*FsReg)(int (*)(), CHAR *, int);
	int		(*FsUnReg)(char *);
	SIGNALHANDLER	(*Signal)(LONG, SIGNALHANDLER);
	struct htp_state *(*gethtp)(void);
	int		(*sgivers)(void);
	int		(*cpuid)(void);
	void		(*businfo)(LONG);
	int		(*cpufreq)(int);
	char		*(*kl_inv_find)(void) ;
	int		(*kl_hinv)(int, char **) ;
	int		(*kl_get_num_cpus)(void) ;
	int		(*sn0_getcpuid)(void) ;
} PrivateVector;

#if __USE_SPB32 || (defined(_K64PROM32) && !defined(_STANDALONE))
/* Strict 32 bit layout for access by 64 bit kernels */
typedef struct {
	__int32_t	Ioctl;
	__int32_t	GetNvramTab;
	__int32_t	LoadAbs;
	__int32_t	InvokeAbs;
	__int32_t	ExecAbs;
	__int32_t	FsReg;
	__int32_t	FsUnReg;
	__int32_t	Signal;
	__int32_t	gethtp;
	__int32_t	sgivers;
	__int32_t	cpuid;
	__int32_t	businfo;
	__int32_t	cpufreq;
} PrivateVector32;
#endif

#define PTVOffset(member) ((LONG)&(((PrivateVector *)0)->member))

extern LONG ioctl(ULONG,LONG,LONG);
extern LONG get_nvram_tab(char *,int);
extern LONG load_abs(CHAR *,ULONG*);
extern LONG invoke_abs(ULONG,ULONG,LONG,CHAR *[],CHAR *[]);
extern LONG exec_abs(CHAR *,LONG,CHAR *[],CHAR *[]);
extern int fs_register(int (*)(), char *, int);
extern int fs_unregister(char *);

/* sgivers:
 *	0 - before sgivers() call.
 *	1 - handles libsc gui programs.
 *	2 - handles elf file loading.
 *	3 - handles relocatable elf file loading.
 */
#define SGIVERS		3
extern int  sgivers(void);

/*extern int  cpuid(void);*/
extern void businfo(LONG);
extern int cpufreq(int);

#ifdef __cplusplus
}
#endif

#endif /* __ARCS_PVECTOR_H__ */
