/*
 * FILE: eoe/cmd/miser/lib/libcpuset/src/libcpuset.h
 *
 * DESCRIPTION:
 *	Cpuset library header file with definitions and function prototypes.
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


#ifndef __LIBCPUSET_H
#define __LIBCPUSET_H

#include <sys/miser_public.h>


/* Command */

int cpuset_create (char* qname, char* fname);
int cpuset_attach(char* qname, char* cmdname, char** cmdargs);
int cpuset_destroy(char* qname);
int cpuset_move_procs(char* qname);


/* Query */

int cpuset_query_current();
int cpuset_query_names();
int cpuset_query_cpus(char *qname);
int cpuset_list_procs(char* qname);

#endif /* __LIBCPUSET_H */
