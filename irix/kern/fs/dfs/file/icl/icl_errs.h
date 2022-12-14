/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 *
 * HISTORY
 * $Log: icl_errs.h,v $
 * Revision 65.2  1999/02/04 19:19:41  mek
 * Provide C style include file for IRIX kernel integration.
 *
 * Revision 65.1  1997/10/20 19:17:39  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.46.1  1996/10/02  17:52:12  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:40:52  damon]
 *
 * $EndLog$
 */

/*
 * icl_errs.h:
 * This file is automatically generated; please do not edit it.
 */
#define ICL_ERROR_RCSID                          (589672448L)
#define ICL_ERROR_NOSET                          (589672449L)
#define ICL_ERROR_NOLOG                          (589672450L)
#define ICL_ERROR_CANTOPEN                       (589672451L)
#define ICL_INFO_TIMESTAMP                       (589672452L)
#define ICL_EMEM_BULK_SET                        (589672453L)
#define ICL_EMEM_BULK_LOG                        (589672454L)
#define ICL_BAD_FOLLOW_ARG                       (589672455L)
#define ICL_BAD_SLEEP_ARG                        (589672456L)
#define ICL_SLEEP_WO_FOLLOW                      (589672457L)
#define ICL_BAD_FILENAME_ARG                     (589672458L)
#define ICL_INVALID_SET_OP                       (589672459L)
#define ICL_CHANGE_STATE                         (589672460L)
#define ICL_W_MAX_SETS_EXCEEDED                  (589672461L)
#define ICL_W_MAX_LOGS_EXCEEDED                  (589672462L)
#define ICL_LOG_UNALLOCATED                      (589672463L)
#define ICL_LOG_SIZE_ZERO                        (589672464L)
#define ICL_BOGUS_RECORD                         (589672465L)
#define ICL_BAD_LOG_ARG                          (589672466L)
#define ICL_BAD_RAW_ARG                          (589672467L)
#define ICL_INFO_LOGOVERFLOW                     (589672468L)
#define ICL_INFO_LOGBIN                          (589672469L)
#define ICL_INFO_LOGRECORDS                      (589672470L)
#define initialize_icl_error_table()
#define ERROR_TABLE_BASE_icl (589672448L)

/* for compatibility with older versions... */
#define init_icl_err_tbl initialize_icl_error_table
#define icl_err_base ERROR_TABLE_BASE_icl
