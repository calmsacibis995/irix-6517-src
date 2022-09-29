/*
 * FILE: eoe/cmd/miser/lib/libmiser/libmiser.h
 *
 * DESCRIPTION:
 *	Miser library header file with definitions and function prototypes.
 */
 
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#ifndef __LIBMISER_H
#define __LIBMISER_H


#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */


#include <sys/miser_public.h>
#include <sys/procfs.h>
#include <stdio.h>
#include <stdlib.h>


typedef struct miser_qhdr_s {
	id_type_t	name;
	id_type_t	policy;
	quanta_t	quantum;
	uint64_t	nseg;
	uint64_t	cseg;
	FILE		*file;
	char		*fname;
} miser_qhdr_t;


/* Utility */
void	merror		(const char *, ...);
void	msyslog		(int priority, const char *, ...);
void	merror_hdr	(const char *, ...);
void	merror_v	(const char *, va_list);
char *	miser_error	(int16_t error);


/* Convertions */
const char *	miser_qstr	(id_type_t);
id_type_t	miser_qid	(const char *);


/* Formating */
char *	fmt_str2num	(const char *, uint64_t *);
int	fmt_str2bcnt	(const char *, memory_t *);
int	fmt_str2time	(const char *, quanta_t *);
char *	fmt_time	(char *, int, time_t);
void	curr_time_str	(char *timebuf);


/* Initialization */
int	miser_init	(void);


/* Query */
quanta_t		miser_quantum		(void);
miser_queue_names_t *	miser_get_qnames	(void);
miser_bids_t *		miser_get_jids		(id_type_t qid, int start);
miser_schedule_t *	miser_get_jsched	(bid_t jid, int start);

int	miser_get_prpsinfo(int pid, prpsinfo_t *pinfo);

int	miser_qname		();
int	miser_qjid		(char *q_name, int start);
int	miser_qstat		(char *queue, int start);
int	miser_qstat_all		(int start);
int	miser_qjsched_all	(int start);


/* Command */
int	miser_submit	(char *, miser_data_t *);
int	miser_move	(char *, char *, miser_data_t *);
int	miser_kill	(int bid);
int	miser_reset	(char *);

/* Verify */
int	miser_checkaccess	(FILE *, int);


/* Parse Constants */
#define PARSE_JSUB 0
#define PARSE_QMOV 1
#define PARSE_QDEF 2


/* Parse Functions */
miser_data_t *	parse			(int16_t, const char *);
int		parse_start		(int16_t, const char *);
miser_data_t *	parse_stop		(void);
miser_seg_t *	parse_jseg_start	(void);
int		parse_jseg_stop		(void);
miser_qseg_t *	parse_qseg_start	(void);
int		parse_qseg_stop		(void);
miser_qhdr_t *	parse_qdef_start	(void);
int		parse_qdef_hdr		(void);
int		parse_qdef_stop		(void);
uint64_t	parse_lnum		(void);
void		parse_select		(int16_t);
int16_t		parse_type		(void);
void		parse_error		(const char *, ...);
int		parse_open		(const char *);
void		parse_close		(void);
void		parse_ctxt_save		(void);
void		parse_ctxt_restore	(void);


/* Printing */
void	print_move		(FILE *, miser_data_t *);
void	miser_print_job_desc	(bid_t jid);
void	miser_print_job_status	(miser_seg_t *seg);
void	miser_print_job_sched	(miser_seg_t *seg);
void	miser_print_block	(int, miser_resources_t *first, 
				miser_resources_t *last, int start);

extern int G_syslog;

#ifdef DEBUG
#define ASSERT(X) { if (!(X)) { merror("Assfail [%s:%d] \"%s\"", \
	__FILE__, __LINE__, # X); exit(1); }
#else
#define ASSERT(X)
#endif	/* DEBUG */


#ifdef __cplusplus
}
#endif	/* __cplusplus */


#endif	/* __LIBMISER_H */
