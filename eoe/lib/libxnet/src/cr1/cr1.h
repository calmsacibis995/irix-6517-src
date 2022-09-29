/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:cr1/cr1.h	1.1.1.1"

#include <crypt.h>
#include <sys/types.h>
#include <rpc/types.h>
#include <rpc/xdr.h>

/*  Declarations used in cr1 module implementations  */

#define CLEN BUFSIZ		/* # bytes in an encrypted message */
#define MLEN BUFSIZ		/* # bytes in a complete message */
#define KEYLEN 8		/* # bytes in a cryptographic key  */
#define PRINLEN 128		/* # bytes in a principal name */

typedef char Principal[PRINLEN];
typedef char Key[KEYLEN];

#define P_LOCAL		01
#define P_REMOTE	02

#define DEF_SCHEME	"cr1"
#define DEF_KEYDIR	"/etc/iaf"
#define DEF_KMPIP	".kmpipe"

enum ktype {
	     START,		/* Start the daemon */
	     STOP,		/* Stop the daemon */
	     MASTER_KEY,	/* Change the master key */
	     ADD_KEY,		/* Add a shared key */
	     CHANGE_KEY,	/* Change a shared key */
	     DELETE_KEY,	/* Delete a shared key */
	     GET_KEY,		/* Request a shared key */
	     SEND_KEY,		/* Provide a shared key */
	     CONFIRM,		/* Confirm requested action was performed */
	     REJECT		/* Reject requested action */
	   };   /*  The message type  */

enum xtype {
	     DES = X_DES,	/* use DES for encryption */
	     ENIGMA = X_ENIGMA	/* use ENIGMA for encryption */
	   };

typedef struct {
	enum ktype type;	/* The message type */
	enum xtype xtype;	/* Encryption type */
	Principal principal1;	/* Local principal */
	Principal principal2;	/* Remote principal */
	Key key1;		/* Old shared key */
	Key key2;		/* New shared key */
} Kmessage;

static bool_t
xdr_kmessage(XDR *xdrs, Kmessage *kmsg);

enum etype {
	CR_EOK,		/* Not an error */
	CR_USAGE,	/* Syntax error */
	CR_CRUSAGE,	/* scheme syntax error */
	CR_CKUSAGE,	/* cryptkey syntax error */
	CR_KMUSAGE,	/* keymaster syntax error */
	CR_PRINCIPAL,	/* Principal name error */
	CR_CONFIRM,	/* Key confirmation failed */
	CR_PIPE,	/* Pipe operation failed */
	CR_PUSH,	/* Module push operation failed */
	CR_MSGOUT,	/* write/t_snd of message failed */
	CR_MSGIN,	/* read/t_rcv of message failed */
	CR_XDRIN,	/* xdr decode function failed */
	CR_XDROUT,	/* xdr encode function failed */
	CR_BADREPLY,	/* Unexpected reply from keymaster daemon */
	CR_REJECT,	/* Reject from keymaster daemon */
	CR_KMSTART,	/* Failure starting daemon */
	CR_KMSTOP,	/* Failure stopping daemon */
	CR_KMLOCK,	/* Error obtaining kemaster lock */
	CR_DBOPEN,	/* Error opening keys database */
	CR_DBSTAT,	/* Error stating keys database */
	CR_DBREAD,	/* Error reading keys database */
	CR_DBBAD,	/* Error in keys database */
	CR_DBTEMP,	/* Error opening temporary keys database */
	CR_DBLINK,	/* Error renaming temporary keys database */
	CR_MEMORY,	/* Memory allocation failed */
	CR_FORK,	/* Fork operation allocation failed */
	CR_MASTER,	/* Master key mismatch */
	CR_FATTACH,	/* Fattach of pipe to entry failed */
	CR_PROTOCOL,	/* Protocol failure */
	CR_NOKEY,	/* No key for remote principal */
	CR_LOGNAME,	/* No logname for effective uid */
	CR_NAMEMAP,	/* Namemap of RLOGNAME failed */
	CR_LVLIN,	/* Lvlin of Lid failed */
	CR_XMACHINE,	/* Wrong machine name authenticated */
	CR_XLOGNAME,	/* Wrong logname name authenticated */
	CR_XSERVICE,	/* Wrong service name authenticated */
	CR_PUTAVA,	/* Putava failed */
	CR_SETAVA,	/* Setava failed */
	CR_END,		/* Unexpected end of deamon */
	CR_ATTRMAP,	/* Attrmap of RLID failed */
	CR_INKEY,	/* Failed to get key via getpass() */
	CR_CRYPT_TYPE,	/* Invalid encryption type */
	CR_CKSUM_TYPE,	/* Invalid checksum type */
	CR_UNKNOWN	/* Unknown error */
	};
