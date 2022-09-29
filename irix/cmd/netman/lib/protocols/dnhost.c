#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>

#include "protocols/dnhost.h"

/*
 * Copy the appropriate definitions and data types to use the 4DDN 2.0
 * private network management (NCP) ioctl to access to the 4DDN kernel's
 * session node table. Note that the ioctl may change between releases!
 * Such was the case between 4DDN release 1.2 and 2.0.
 *
 * ---------------------------
 * from cmd/decnet/dn/tcistd.h
 * ---------------------------
 */

typedef	char		Tiny;		/* 1 byte signed numbers */
typedef	unsigned char	Utiny;		/* 1 byte unsigned numbers */
typedef short		Short;		/* 2 byte integer */
typedef short		Sbool;		/* 2 byte Boolean value 0 or 1 test */

/* ---------------------------
 * from cmd/decnet/dn/nodename.h
 * ---------------------------
 */

#define NODE_LENGTH     6       /* Maximum length of node name string */

/* ---------------------------
 * from nmdefs.h
 * ---------------------------
 */

#define NODE 0
#define EXEC 6

/* Network Management Response Codes */
#define NM_SUCCESS          0x01
#define NM_FILE_IO_ERROR    0xEE
#define NM_CLOSE_ERR        NM_FILE_IO_ERROR
#define NM_IOCTL_ERROR      NM_FILE_IO_ERROR

/* 
 * Make this the correct size to long word align this structure 
 * Minimum size is NODE_LENGTH + 1
 */
#define MAX_ENTITY_SIZE NODE_LENGTH + 2
typedef struct m_common
	{
	Sbool        mc_priv;        /* Privilige Level set by /dev/nm */
	Short        mc_cmd;         /* FUNCTION REQUESTED */
	Short        mc_status;      /* Result of operation */
	Short        mc_et;          /* Entity Type : NODE,CIRC,LOGGING,LINE */
	Short        mc_nn;          /* Node Number */
	Short        mc_st_at;       /* Start at entity # */
	Short        mc_curr;        /* Current entity entity # */
	Utiny        mc_ver[6];      /* Nmars version number */
	Tiny         mc_en[MAX_ENTITY_SIZE];
	                         /* Entity Name - 1st byte contains length */
	} M_Com;

typedef struct ses_node_table_record
    {
    union {
        unsigned short sntr_snn;
        unsigned char  sntr_cnn[2];
        }sntr_na;		/* Node Address */
    short sntr_adj;		/* Adjacency Flag */
    short sntr_circ;		/* Circuit Type */
    short sntr_ht;		/* Hello Timer */
    short sntr_tout;		/* Routing Hello timeout value */
    char  sntr_nn[7];		/* Node Name */
    char  sntr_ntype;		/* Node Type */
    char  word_align[2] ;	/* Long Word Align */
    } Ses_Node_Rec;

typedef struct
	{
	struct m_common m_snt_c;
	Ses_Node_Rec m_snt_d;
	} M_Snt;

#define UNETM	0x30
#undef IOCPARM_MASK
#define IOCPARM_MASK 0xff
#ifdef sun
#define NMARS_READ_SNT		_IOWR(	N, UNETM + 1 , M_Snt )
#else
#define NMARS_READ_SNT		_IOWR(	'N', UNETM + 1 , M_Snt )
#endif

/*
 * --------------------------------------------------------------------------
 * End of the copying of header file values.
 * --------------------------------------------------------------------------
 */

#define	NMARS_NM_DEVICE	"/dev/dn_netman"

/*
 * Macros
 */
#define	SWAP16(a)	( (((a) >> 8) & 0x00FF) | (((a) << 8) & 0xFF00) )
#define	SWAP16_STORE(a)	((a) = SWAP16(a))

#define	MAKE_SHORT(a, b)	( (((a)<<8)&0xFF00) | (0x00FF&(b)) )

/*
 * Global Variables
 */
M_Snt	v_t_snt;		/* holds Session Node Table read data */

unsigned char	nm_ver[6] = { 0x01, 0x04, 0x01, 0x00, 0x01, 0x00 };


void
set_common(short entity_type, unsigned short node_number,
	   char *entity_name, M_Com *cmn_ptr)
{
	bzero(cmn_ptr, sizeof(M_Com));		/* clear common portion */
	cmn_ptr->mc_priv = ( !(getuid()) );	/* volatile dbase privileges */
	bcopy((char *)&nm_ver[0], (char *)&cmn_ptr->mc_ver[0], 6);
	cmn_ptr->mc_et = entity_type;
	cmn_ptr->mc_nn = node_number;
	if (entity_name != NULL) 
		strncpy(&cmn_ptr->mc_en[0], &entity_name[0], MAX_ENTITY_SIZE);
} /* end of set_common() */


/*
 * Make the call to the DECnet kernel; if there are problems with the
 * open() or ioctl() system call, the variable <errno> should still contain
 * the error code. Caller can use <errno> to generate an appropiate error
 * message.
 */
int
netman_ll(int cmd, char *data)
{
#ifdef sun
	return(NM_FILE_IO_ERROR);
#else
	int	ret;
	int	ll;
	extern int errno;
    
	if ((ll = open(NMARS_NM_DEVICE, O_RDWR)) == -1 )
		return(NM_FILE_IO_ERROR);

	if ((ret = ioctl((int)ll, (int)cmd, (char *)data)) != 0)
		return(NM_IOCTL_ERROR);

	if ((ret = close(ll)) != 0)
		return(NM_CLOSE_ERR);

	return(NM_SUCCESS);
#endif
} /* end of netman_ll() */


int
v_read_SNT(unsigned short node_num, char *entity_name, short entity_type)
{
	int status;
        
	/* Fill in common block. */
	set_common(entity_type, node_num, entity_name, &v_t_snt.m_snt_c);
              
	/* Clear the Record Space */
	bzero(&v_t_snt.m_snt_d, sizeof(Ses_Node_Rec));

	/* Read local nsp database */
	status = netman_ll(NMARS_READ_SNT, (char *)&v_t_snt);

	if (status != NM_SUCCESS)
		return(status);

	SWAP16_STORE(v_t_snt.m_snt_d.sntr_ht);
	SWAP16_STORE(v_t_snt.m_snt_d.sntr_tout);
	SWAP16_STORE(v_t_snt.m_snt_d.sntr_adj);
	SWAP16_STORE(v_t_snt.m_snt_d.sntr_circ);

	return((int)v_t_snt.m_snt_c.mc_status);
} /* end of v_read_SNT() */


/*
 =====================================================================
 E X P O R T E D   F U N C T I O N S
 =====================================================================
 */

/*
 * Address/Name translation routines. The <mode> agrument indicates the
 * byte order of the DECnet address. When the name/address cannot be
 * retreived for whatever reasons, NULL is returned.
 */

char *
dn_getnodename(unsigned short nodeaddr, char mode)
{
	unsigned short	addr;

	/* address must be in net order */
	addr = (mode == ORD_HOST) ? SWAP16(nodeaddr) : nodeaddr;
	if ( v_read_SNT(addr, (char *)0, NODE) != NM_SUCCESS )
		return (char *)NULL;
	return &v_t_snt.m_snt_d.sntr_nn[0];
} /* end of dn_getnodename() */


unsigned short
dn_getnodeaddr(char *nodename, char mode)
{
	unsigned short	nodeaddr;

	if( v_read_SNT( 0, nodename, (nodename ? NODE:EXEC) ) != NM_SUCCESS )
		return (unsigned short)NULL;
	nodeaddr = MAKE_SHORT(v_t_snt.m_snt_d.sntr_na.sntr_cnn[0],
				v_t_snt.m_snt_d.sntr_na.sntr_cnn[1]);
	if (mode == ORD_HOST)
		SWAP16_STORE(nodeaddr);
	return nodeaddr;
} /* end of dn_getnodeaddr() */

