/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "irix/cmd/mkpart/mkpart.h: $Revision: 1.19 $"

#ifndef __MKPART_H__
#define __MKPART_H__

#define SN0 	1

#include <sys/SN/arch.h>

#include <sys/partition.h>

#define db_printf	if (debug) printf

#define VERSION		0x10

/* 
 * MAX_MODS_PER_PART - Max modules per partition.
 * A reasonable value for this seems to be MAX_NASIDS.
 * Assuming 1 node per module, and a single partition
 * config. The kernel can provide us info on only these
 * many modules anyway.
 * In case we cannot find MAX_NASIDS, use the define below.
 * #define MAX_MODS_PER_PART	256 
 */

#define MAX_MODS_PER_PART	MAX_NASIDS
#define MAX_PN			MAX_NASIDS    

/*
 * These are some of the local MAX_ defines used to determine
 * data struct length. These are fairly consistently used in
 * the code, so, if something fails due to insufficnent size 
 * of a data structure bumping these values may help.
 */

#define MAX_ERRBUF_LEN		256
#define MAX_KEYW_LEN		256
#define MAX_CMDLINE_LEN		1024 	/* or max len of a line in cfgfile */

#define IS_PARTID_VALID(p)      ((p >= 0) && (p < MAX_PARTITIONS))

/*
 * partcfg_t
 *
 * partition config struct. Command line or file input is 
 * translated into this structure. Most of the routines use
 * this.
 */

typedef struct partcfg_s {
    struct partcfg_s	*part_next ;	/* ptr to next in list */
    partid_t		part_id ;	/* part id */
    int			part_nmods ; 	/* number of modules found */
    moduleid_t		*part_modules ;	/* modules */
    int			part_mods_sz ;  /* part_modules allocated */
    int			flags ;
#define	PART_COMBINE	0x1
} partcfg_t ;

#define partcfgNext(pc)		(pc->part_next)
#define partcfgId(pc)		(pc->part_id)
#define partcfgNumMods(pc)	(pc->part_nmods)
#define partcfgNumModsIncr(pc)	(pc->part_nmods++)
#define partcfgModules(pc)	(pc->part_modules)
#define partcfgModuleId(pc, i)	(pc->part_modules[i])
#define partcfgModsSize(pc)	(pc->part_mods_sz)
#define partcfgModLimit(pc, i)  (i < partcfgModsSize(pc))

/*
 * partcfg struct operations.
 */

partcfg_t 	*partcfgCreate(int nmods) ;
void 		partcfgFree(partcfg_t *pc) ;
void 		partcfgFreeList(partcfg_t *pc) ;
void 		partcfgDump(partcfg_t *pc) ;
void 		partcfgDumpList(partcfg_t *pc) ;

/*
 * Convert a pn - which is the struct returned by syssgi
 * to a partcfg list. pn is the current config of the system.
 */

partcfg_t 	*pnToPcList(pn_t *pn, int cnt) ;

/*
 * Parsing support. In mkplib.c
 */

int 		get_string_index(char **tab, char *str) ;
char 		*cmd_to_file_string(int argc, char **argv, int argi) ;
partcfg_t   	*parse_file(FILE *fp, char *errbuf) ;
partcfg_t   	*parse_multi_string(char *str, char *errbuf) ;

/*
 * partitioning implementation functions.
 */

int 		part_make	(partcfg_t *, char *) ;
int 		part_list	(char *) ;
int		part_reinit 	(char *) ;
int 		part_activate	(char *);
int 		part_kill	(partcfg_t *, char *) ;
partcfg_t 	*part_scan(int dump_flag, char *errbuf) ;
partcfg_t       *mkp_interactive(void) ;
int 		check_sanity(partcfg_t *, char *) ;
void		ignore_signal	(int, int) ;
void		logmsg		(char *, ...) ;
void		inquire_and_reboot(partcfg_t *, char *) ;
void		inquire_and_reboot_init(char *) ;
void		shutdown_self_query(char *) ;
int 		yes_or_no	(char *) ;

/*
 * Some common functions.
 */

partid_t 	get_my_partid		(void) ;
partcfg_t 	*partcfgLookupPart	(partcfg_t *, partid_t, moduleid_t) ;
int		get_raw_path		(char *path, partid_t p) ;
int		partcfgValidList	(partcfg_t *, char *) ;
int 		rexec_mkpbe		(char *, char *, char *, int) ;

#define HOSTADDR_LEN 	4

int 		get_data_from_local(char op, partid_t p, char *buf, int blen) ;
/* 
 * buf and blen cannot be NULL and 0 respectively. This is because
 * this buffer is used to return data and if no data is needed it
 * is used to return status.
 */

void 		read_rou_fence(void) ;
void 		write_rou_fence(partcfg_t *) ;

extern void     bzero(void *, size_t);
extern void     bcopy(const void *, void *, size_t);

/*
 * Error message defines. These can be present in the .c
 * file where the strings are defined for clarity.
 */

#define MSG_HEADER      		0
#define MSG_CUR_PARTS   		1
#define MSG_WARN_SU     		2
#define MSG_ARG_INVALID 		3
#define MSG_SYNTAX_ERROR 		4

#define MSG_MEM_ALLOC 			0
#define MSG_MOD_EXCEED_LIMIT            1
#define MSG_LIB_SYNTAX_ERROR            2
#define MSG_PARTIAL_SYNTAX_ERROR        3
#define MSG_NO_PARTITIONS               4
#define MSG_INVALID_CONFIG              5
#define MSG_REMOTE_EADDR                6
#define MSG_REXEC_FAILED                7
#define MSG_LOCK_ERR                    8
#define MSG_UNLOCK_ERR                  9
#define MSG_ROUMAP_ERR                  10
#define MSG_ALL_MODULE                  11

#define PART_REBOOT_NVRAM_CMD 		"nvram PartReboot \0166;"
#define PART_ONLY_REBOOT_CMD		"echo 1 | /etc/reboot "
#define PART_REBOOT_SHUTDOWN_CMD	"echo 1 | /etc/shutdown -g0 -y;"
#define PART_REBOOT_CMD			PART_REBOOT_NVRAM_CMD	\
					PART_REBOOT_SHUTDOWN_CMD

#define PART_MAKE_NORMAL 		1
#define PART_MAKE_UNPARTITIONED 	2

/* Valid Partition Config headers */

/* Limits for various config values. These define data structure limits. */

/* Different total module sets, like 2, 4, 8, 16 ... */
#define MAX_TOTAL_MODULES       32

/* Number of different configs per module set, a 4 module system
 * can have 4 different configs.
 */
#define MAX_CONFIG              32

/* Number of different partition combos in a config. We can have
 * a config like 1 partition X 2 modules + 2 partitions X 1 module.
 * COMBOS will be 2 in such a case.
 */
#define MAX_COMBOS              32

/* number of partitions X modules per partition */

typedef struct nmodnpar_s {
        int     nmod ;
        int     npar ;
} nmodnpar_t ;

/* number of nmodnpar_t combinations */

typedef struct part_combo_s {
        int             ncombos ;
        nmodnpar_t      nmnp[MAX_COMBOS] ;
} part_combo_t ;

/* For a given number of total modules, number of configs
 * supported and their details.
 */

typedef struct valid_config_s {
        int             total_modules ;
        int             nconfigs ;
        part_combo_t    combo[MAX_CONFIG] ;
} valid_config_t ;

#define vcTotalModules(vcp)             (vcp->total_modules)
#define vcNConfigs(vcp)                 (vcp->nconfigs)
#define vcValid(vcp)                    (vcNConfigs(vcp) != 0)
#define vcCombo(vcp,i)                  (vcp->combo[i])
#define vcComboNCombos(vcp,i)           (vcCombo(vcp,i).ncombos)
#define vcComboModule(vcp,i,j)          (vcCombo(vcp,i).nmnp[j].nmod)
#define vcComboPartition(vcp,i,j)       (vcCombo(vcp,i).nmnp[j].npar)

/* valid config interface definition. */

void    read_valid_config(valid_config_t *) ;
void    dump_valid_config(valid_config_t *) ;
void    dump_valid_config_all(valid_config_t *) ;
int     check_valid_config(     partcfg_t *, partcfg_t *, partcfg_t *,
                                valid_config_t *) ;
#ifdef DEBUG_VALID_CONFIG
partcfg_t *generate_pc_from_user(void) ;
#endif

#define VALID_CONFIG_FILE               "/etc/config/mkpart.config"
#define VALID_CONFIG_FILE_MAGIC         "#_mkpart.valid.configs"

#endif /* __MKPART_H__ */
