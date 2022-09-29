/*
 * NOTE: Keep this file in sync with:
 *    /hosts/bonnie/proj/ficus/isms/stand/x86/mmsc/sgi/include/oobmsg.h
 *
 * oobmsg.h
 *	Constants and declarations that pertain to Out Of Band Messages
 *	such as those sent by mmscd, the IRIX daemon that talks to the MMSC.
 */

#ifndef _OOBMSG_H_
#define _OOBMSG_H_

/*
 * The MMSC can communicate with IRIX over the IO6 using out-of-band
 * messages.  Messages can originate from either IRIX or the MMSC, and
 * have the following format:
 *
 *	Field	Bytes	Purpose
 *	--------------------------------------------------
 *	Escape    1	Out of Band Prefix character, 0xa0
 *	Opcode    1	Command opcode
 *	Length    2	Length of Data field in bytes (big-endian)
 *	Data	0-64K	Optional command-dependent arguments
 *	CkSum	  1	The sum of the Length, Opcode & Data fields (mod 256)
 *
 * If it is ever necessary for one side to send an actual 0xa0 character
 * to the other side, it will actually send two consecutive 0xa0's. This
 * implies that there can never be an opcode 0xa0.
 *
 * Every command message generates a response. The format of a response
 * is similar to that of a command:
 *
 *	Field	Bytes	Purpose
 *	--------------------------------------------------
 *	Escape    1	Out of Band Prefix character, 0xa0
 *	Status	  1	Return status
 *	Length    2	Length of Data field in bytes (big-endian)
 *	Data	0-64K	Optional command-dependent arguments
 *	CkSum	  1	The sum of the Length, Opcode & Data fields (mod 256)
 *
 */

/* Return status codes */
#define STATUS_NONE		0	/* Command completed successfully */
#define STATUS_FORMAT		1	/* Illegal command format */
#define STATUS_COMMAND		2	/* Unknown command opcode */
#define STATUS_ARGUMENT		3	/* Illegal argument(s) */
#define STATUS_CHECKSUM		4	/* Command checksum error */
#define STATUS_INTERNAL		5	/* Internal error */


/* Graph command opcodes */
#define GRAPH_START		1	/* Enter graph mode */
#define GRAPH_END		2	/* Leave graph mode */
#define GRAPH_MENU_ADD		3	/* Add item to graph menu */
#define GRAPH_MENU_GET		4	/* Get currently selected graph item */
#define GRAPH_LABEL_TITLE	5	/* Set graph title */
#define GRAPH_LABEL_HORIZ	6	/* Set horizontal axis label */
#define GRAPH_LABEL_VERT	7	/* Set vertical axis label */
#define GRAPH_LABEL_MIN		8	/* Set minimum graph value */
#define GRAPH_LABEL_MAX		9	/* Set maximum graph value */
#define GRAPH_LEGEND_ADD	10	/* Add legend label */
#define GRAPH_DATA		11	/* Graph data */

/* Image opcodes */
#define IMAGE_START		40	/* Initiate image transfer */
#define IMAGE_DATA		41	/* Append data to image buffer */
#define IMAGE_END		42	/* Complete image transfer */
#define IMAGE_FREE		43	/* Release stored image */
#define IMAGE_BACKGROUND	44	/* Set display backgroud */

/* Miscellaneous opcodes */
#define OOB_NOP			0	/* No Operation */
#define ELSC_COMMAND		60	/* Send command to ELSC */
#define MMSC_COMMAND		61	/* Send command to MMSC */
#define MMSC_INIT		62	/* Initialize/exchange version */
#define OOB_RESERVED		160	/* OBP character */

/* Macros to access individual fields in an out of band msg, given a */
/* pointer to a buffer containing the entire thing.		     */
#define OOBMSG_CODE(B)	  ((unsigned char) B[1])
#define OOBMSG_DATALEN(B) ((((unsigned char) B[2])*256)+((unsigned char) B[3]))
#define OOBMSG_DATA(B)	  (&B[4])
#define OOBMSG_SUM(B)	  ((unsigned char) (B[OOBMSG_DATALEN(B) + 4]))
#define OOBMSG_MSGLEN(B)  (OOBMSG_DATALEN(B) + MAX_OOBMSG_OVERHEAD)

#endif /* _OOBMSG_H_ */
