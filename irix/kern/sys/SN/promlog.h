/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.3 $"

#ifndef	__SYS_SN_PROMLOG_H__
#define	__SYS_SN_PROMLOG_H__

#include <sys/SN/fprom.h>

#define PROMLOG_MAGIC			0x504c4f47
#define PROMLOG_VERSION			1

#define PROMLOG_OFFSET_MAGIC		0x10
#define PROMLOG_OFFSET_VERSION		0x14
#define PROMLOG_OFFSET_SEQUENCE		0x18
#define PROMLOG_OFFSET_ENTRY0		0x100

#define PROMLOG_ERROR_NONE		0
#define PROMLOG_ERROR_PROM	       -1
#define PROMLOG_ERROR_MAGIC	       -2
#define PROMLOG_ERROR_CORRUPT	       -3
#define PROMLOG_ERROR_BOL	       -4
#define PROMLOG_ERROR_EOL	       -5
#define PROMLOG_ERROR_POS	       -6
#define PROMLOG_ERROR_REPLACE	       -7
#define PROMLOG_ERROR_COMPACT	       -8
#define PROMLOG_ERROR_FULL	       -9
#define PROMLOG_ERROR_ARG	       -10

#define PROMLOG_TYPE_END		3
#define PROMLOG_TYPE_LOG		2
#define PROMLOG_TYPE_LIST		1
#define PROMLOG_TYPE_VAR		0

#define PROMLOG_TYPE_ANY		98
#define PROMLOG_TYPE_INVALID		99

#define PROMLOG_KEY_MAX			16
#define PROMLOG_VALUE_MAX		128
#define PROMLOG_CPU_MAX			4

typedef struct promlog_header_s {
    unsigned int	unused[4];
    unsigned int	magic;
    unsigned int	version;
    unsigned int	sequence;
} promlog_header_t;

typedef unsigned int promlog_pos_t;

typedef struct promlog_ent_s {		/* PROM individual entry */
    uint		valid		: 1;	/* Byte 0 */
    uint		type		: 2;
    uint		cpu_num		: 1;
    uint		key_len		: 4;

    uint		reserved	: 1;	/* Byte 1 */
    uint		value_len	: 7;
} promlog_ent_t;

typedef struct promlog_s {		/* Activation handle */
    fprom_t		f;
    int			sector_base;
    int			cpu_num;

    int			active;		/* Active sector, 0 or 1 */

    promlog_pos_t	log_start;
    promlog_pos_t	log_end;

    promlog_pos_t	alt_start;
    promlog_pos_t	alt_end;

    promlog_pos_t	pos;
    promlog_ent_t	ent;
} promlog_t;

int	promlog_init(promlog_t *l, fprom_t *f,
		     int sector_base, int cpu_num);
int	promlog_clear(promlog_t *l, int init);
int	promlog_first(promlog_t *l, int type);
int	promlog_prev(promlog_t *l, int type);
int	promlog_next(promlog_t *l, int type);
int	promlog_last(promlog_t *l);

#define promlog_tell(l, pos)	(*(pos) = (l)->pos, 0)
#define promlog_seek(l, pos)	((l)->pos = *(pos), 0)

int	promlog_get(promlog_t *l, char *key, char *value);
int	promlog_find(promlog_t *l, char *key, int type);
int	promlog_lookup(promlog_t *l, char *key, char *value, char *defl);
int	promlog_replace(promlog_t *l, char *key, char *value, int type);
int	promlog_compact(promlog_t *l);
int	promlog_put_var(promlog_t *l, char *key, char *value);
int	promlog_put_list(promlog_t *l, char *key, char *value);
int	promlog_put_log(promlog_t *l, char *key, char *value);
int	promlog_delete(promlog_t *l);
int	promlog_dump(promlog_t *l);
char   *promlog_errmsg(int errcode);

#endif /* __SYS_SN_PROMLOG_H__ */
