/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * DCE Messaging header file.
 */

/*
 * HISTORY
 * $Log: dce_msg.h,v $
 * Revision 65.1  1997/10/20 19:15:06  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.6.2  1996/02/18  23:33:04  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:21:07  marty]
 *
 * Revision 1.1.6.1  1995/12/08  21:37:29  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  18:09:25  root]
 * 
 * Revision 1.1.4.4  1994/09/30  19:44:51  rsalz
 * 	Implement DCE RFC 24.2 (OT CR 11929).
 * 	[1994/09/27  04:45:17  rsalz]
 * 
 * Revision 1.1.4.3  1994/09/13  15:27:06  rsalz
 * 	Make dce_msg__inq_comp thread-safe (OT CR 12067).
 * 	[1994/09/13  05:00:32  rsalz]
 * 
 * Revision 1.1.4.2  1994/06/09  16:05:52  devsrc
 * 	cr10892 - fix copyright
 * 	[1994/06/09  15:50:28  devsrc]
 * 
 * Revision 1.1.4.1  1994/05/26  18:51:35  bowe
 * 	Add prototype for dce_msg__inq_comp() [CR 10483,10478]
 * 	[1994/05/26  18:12:33  bowe]
 * 
 * Revision 1.1.2.3  1993/11/04  18:53:38  rsalz
 * 	Use error_status_t, not unsigned32, for status codes
 * 	[1993/11/04  18:52:43  rsalz]
 * 
 * Revision 1.1.2.2  1993/08/16  18:07:58  rsalz
 * 	Initial release
 * 	[1993/08/16  18:02:30  rsalz]
 * 
 * $EndLog$
 */

#if	!defined(_DCE_MSG_H)
#define _DCE_MSG_H


/*
**  An in-core message table maps status code values to text strings.
*/
typedef struct dce_msg_table_s_t {
    unsigned32			message;
    char			*text;
} dce_msg_table_t;


/*
**  Get message from catalog, or from in-core tables.  Return pointer to
**  allocated space that must be free'd.
*/
extern unsigned char *dce_msg_get_msg(
    unsigned32			/* message */,
    error_status_t*		/* status */
);


/*
**  Get a message; abort on error.
*/
extern unsigned char *dce_msg_get(
    unsigned32			/* message */
);


/*
**  Add a program-specific message table to the in-core table list.
*/
extern void dce_msg_define_msg_table(
    dce_msg_table_t*		/* table */,
    unsigned32			/* count */,
    error_status_t*		/* status */
);


/*
**  Translate all messages in a table.
*/
extern void dce_msg_translate_table(
    dce_msg_table_t*		/* table */,
    unsigned32			/* count */,
    error_status_t*		/* status */
);

/*
**  Get message from in-core tables.  Return static pointer.
*/
extern unsigned char *dce_msg_get_default_msg(
    unsigned32			/* message */,
    error_status_t*		/* status */
);


/*
**  One-shot routine to get a message from a DCE message catalog.
*/
extern unsigned char *dce_msg_get_cat_msg(
    unsigned32			/* message */,
    error_status_t*		/* status */
);


/*
**  DCE provides a layer over XPG4 message catalogs.
*/
typedef struct dce_msg_cat_handle_s_t *dce_msg_cat_handle_t;


/*
**  Given a "typical" message code, return a handle to the open
**  message catalog.
*/
extern dce_msg_cat_handle_t dce_msg_cat_open(
    unsigned32			/* typical_message */,
    error_status_t*		/* status */
);


/*
**  Get a message from an open DCE message catalog.
*/
extern unsigned char *dce_msg_cat_get_msg(
    dce_msg_cat_handle_t	/* handle */,
    unsigned32			/* message */,
    error_status_t*		/* status */
);


/*
**  Close an open DCE message catalog.
*/
extern void dce_msg_cat_close(
    dce_msg_cat_handle_t	/* handle */,
    error_status_t*		/* status */
);


typedef char	dce_msg_inqbuf_t[4];


/*
**  Internal routine to get the DCE component from a Message ID.
*/
extern void dce_msg__inq_comp(
    unsigned32			/* message */,
    dce_msg_inqbuf_t		/* buffer */
);


/*
**  Internal routine to get the DCE technology from a Message ID.
*/
extern void dce_msg__inq_tech(
    unsigned32			/* message */,
    dce_msg_inqbuf_t		/* buffer */
);
#endif	/* !defined(_DCE_MSG_H) */
