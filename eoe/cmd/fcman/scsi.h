#ifndef _SCSI_H_
#define _SCSI_H_

char *make_devscsi_path(int cid, int tid, int lun, char *filename);
char *make_scsiha_path(int cid, char *filename);

/* Device type values */
#define DIRECT_ACCESS_DEV   0
#define SEQ_ACCESS_DEV      1

#define MAX_INQ_VID 8
#define MAX_INQ_PID 16
#define MAX_INQ_PRL 4

typedef	struct
{
	unchar	pqt:3,		/* peripheral qual type */
		pdt:5;		/* peripheral device type */
	unchar	rmb:1,		/* removable media bit */
		dtq:7;		/* device type qualifier */
	unchar	iso:2,		/* ISO version */
		ecma:3,		/* ECMA version */
		ansi:3;		/* ANSI version */
	unchar	aenc:1,		/* async event notification supported */
		trmiop:1,	/* device supports 'terminate io process msg */
                naca:1,
		:1,		
		respfmt:4;	/* SCSI 1, CCS, SCSI 2 inq data format */
	unchar	ailen;		/* additional inquiry length */	
	unchar	res1;		/* reserved */
	unchar	:1,
                es:1,		/* 8045/8067 capable */
#if SG_WAR_2
                dualp:1,	/* this device dual-ported */
                port:1,		/* command received on port 0 or 1 */
#else
                port:1,		/* command received on port 0 or 1 */
                dualp:1,	/* this device dual-ported */
#endif
  	        :4;
	unchar	reladr:1,	/* supports relative addressing (linked cmds) */
	        :3,
		linked:1,	/* supports linked commands */
                :1,
		cmdq:1,		/* supports cmd queuing */
		softre:1;	/* supports soft reset */
	unchar	vid[MAX_INQ_VID]; /* vendor ID */
	unchar	pid[MAX_INQ_PID]; /* product ID */
	unchar	prl[MAX_INQ_PRL]; /* product revision level*/
	unchar  serial[8];	/* Serial number */
	unchar	vendsp[12];	/* vendor specific; typically firmware info */
	unchar	res4[40];	/* reserved for scsi 3, etc. */
	unchar	vun[2];		/* more vendor unique stuff */
	unchar	plant[10];	/* mfg info */
	/* more vendor specific information may follow */
} inq_struct_t;

int inquiry(char *devname, inq_struct_t *inqdata);

int send_diagnostics(char *devname, u_char *buf, u_int buflen);
int recv_diagnostics(char *devname, u_char pagecode, u_char *buf, u_int *buflen);
int startstop_drive(char *devname, char start);

int enable_pbc(char *devname, u_int tid);
int disable_pbc(char *devname, u_int tid);

#endif
