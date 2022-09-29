/***********************************************************************\
*	File:		prom_externs.h					*
*									*
*	Contains external declarations of various C routines used in	*
*	the PROM.							*
*									*
\***********************************************************************/

#ifndef _PROM_EXTERNS_H_
#define _PROM_EXTERNS_H_

#include <sys/types.h>
#include <sys/EVEREST/evconfig.h>

extern void	init_cfginfo(evcfginfo_t*);
extern int	init_inventory(evcfginfo_t*, uint);
extern void	set_endianess();
extern int	configure_cpus(evcfginfo_t*);

/*
 * Functions from epc_config.c
 */
extern int	io4_initmaster(evcfginfo_t*);

/* 
 * Functions from mc3_config.c
 */
extern int	mc3_config(evcfginfo_t*);

/*
 * Functions from nvram.c
 */
extern uint	get_nvreg(uint);
extern uint 	set_nvreg(uint, uint);
extern uint 	nvchecksum();
extern int	set_nvram(uint, uint, char*);
extern int	get_nvram(uint, uint, char*);
extern void	init_nvram();

/*
 * Functions from pod.c 
 */
extern void	pod_loop();
extern void	loprintf(char*, ...);
extern int*	logets(int*, int);

extern void 	send_int(int, int, int);
extern int	set_endian(int);

/* Version information */
extern char *getversion(void); 

#endif  /* _PROM_EXTERNS_H_ */
