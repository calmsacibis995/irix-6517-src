/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "xfs_get_hosts_key.h: $Revision: 1.1 $"

/* The local hosts file */
#define	ETC_LOCAL_HOSTFILE	"/etc/hosts"
#define	ETC_NETGROUP_HOSTFILE	"/etc/netgroup"

/*
 *	The definition of keywords used for accessing hostnames.
 */

#define	XFS_GETHOSTS_SRC_STR		"XFS_GETHOSTS_SRC"
#define	XFS_GETHOSTS_HOST_STR		"XFS_GETHOSTS_HOST"
#define	XFS_SERVER_IP_ADDR_STR		"XFS_SERVER_IP_ADDR"
#define	XFS_GETHOSTS_NGRP_NAME_STR	"XFS_GETHOSTS_NGRP_NAME"
#define	XFS_GETHOSTS_NGRP_HOST_STR	"XFS_GETHOSTS_NGRP_HOST"
#define	XFS_GETHOSTS_NGRP_USER_STR	"XFS_GETHOSTS_NGRP_USER"
#define	XFS_GETHOSTS_NGRP_DOMAIN_STR	"XFS_GETHOSTS_NGRP_DOMAIN"
#define	XFS_GETHOSTS_NGRP_END_STR	"XFS_GETHOSTS_NGRP_END"
#define	XFS_LOCAL_HOSTS_FILE_STR	"XFS_LOCAL_HOSTS_FILE"
#define	XFS_GET_SERVER_IP_ADDR_STR	"XFS_GET_SERVER_IP_ADDR"
#define	XFS_GET_NAME_BY_IP_ADDR_STR	"XFS_GET_NAME_BY_IP_ADDR"


/* Known sources to fetch the hostname information */
#define	ETC_HOSTS_STR		"ETC_HOSTS"
#define	NIS_HOSTS_STR		"NIS_HOSTS"
#define	ETC_NETGROUP_STR	"ETC_NETGROUP"

/* To temporarily bypass internationalization */
#define gettxt(str_line_num,str_default)        str_default

extern int gethostbyipaddr(char* hostfile,char* addr,char** info,char** msg);
extern int getlocalhostipaddr(char* hostfile,char** info,char** msg);
extern int gethostsinfo_local(char* hostfile,char** info,char** msg);
extern int gethostsinfo_nis(char** info,char** msg);
extern int gethostsinfo_netgrp(char** info,char** msg);

