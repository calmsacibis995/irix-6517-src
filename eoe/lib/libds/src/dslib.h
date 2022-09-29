#ifndef __DSLIB_H__
#define __DSLIB_H__
#ifdef __cplusplus
extern "C" {
#endif
/*
 *  dslib.h -- devscsi library include file
 *
 *	Copyright 1988, 1989, by
 *	Gene Dronek (Vulcan Laboratory) and
 *	Rich Morin  (Canta Forda Computer Laboratory).
 *	All Rights Reserved.
 *
 *	Ported to SGI by Dave Olson 3/89
 *	Copyright 1989 Silicon Graphics, Inc. All rights reserved.
 *
 *	This file is included by user programs using dslib
 *      to control SCSI Common Command Set Devices.
 *
 */

#ident "dslib.h: $Revision: 1.15 $ "

#include <sys/dsreq.h>

/*
|| context jazz
*/

#define FDSIZ    64	/* this is no longer used in dsopen, as of irix 6.5.6.
	* It is left for compatibility since it may be used by other programs */
#define CMDSIZ   16
#define SENSESIZ 100

struct context {
  struct dsreq dsc_dsreq;
  char
    dsc_cmd[CMDSIZ],
    dsc_sense[SENSESIZ];
  int
    dsc_fd;
};

#define getfd(dsp) (((struct context *) dsp->ds_private)->dsc_fd)


/*
|| string lookup jazz
*/

struct ctab{
  uint val;
  char  *string;
};

struct vtab{
  char  *symbol;
  uint val;
  char  *string;
};

extern struct vtab
  cmdnametab[],
  cmdstatustab[],
  dsrqnametab[],
  dsrtnametab[],
  msgnametab[];

extern struct ctab
  sensekeytab[];

extern uint  dsreqflags;	/* flag to set on all calls to filldsreq(),
				   such as DSRQ_SENSE */

extern int dsdebug;	/* if & 1, DSRQ_TRACE bit will be set by filldsreq(),
			   if & 2, DSRQ_PRINT bit will be set */

/* debug code in the ds library, and often programs that use it */
#define DSDBG(x) {if (dsdebug) {x;}}

extern int ds_default_timeout;  /* timeout (in seconds) used by dsfillreq();
	may be changed globally, or set in individual functions after 
	dsfillreq() is called for specific commands. */

/* 
|| Common Command Set jazz
*/

struct g0_cmd {
  unsigned char g0_b0, g0_b1, g0_b2, g0_b3, g0_b4, g0_b5;
};


#define  g0_op_code	g0_b0	/* operation code		     */
#define  g0_lun_etc	g0_b1	/* logical unit, etc.		     */
#define  g0_lba1	g0_b2	/* logical block address (part 1)    */
#define  g0_len2	g0_b2	/* allocation length (part 2)	     */
#define  g0_resid	g0_b2	/* reservation identification	     */
#define  g0_lba0	g0_b3	/* logical block address (part 0)    */
#define  g0_len1	g0_b3	/* allocation (etc.) length (part 1) */
#define  g0_len0	g0_b4	/* allocation (etc.) length (part 0) */
#define  g0_flag_link	g0_b5	/* flag & link bits	    	     */


#define  G0_TEST	 0x00 	/* Test Unit Ready	*/
#define  G0_REWI	 0x01 	/* Rewind		*/
#define  G0_REZE	 0x01 	/* Rezero Unit		*/
#define  G0_REQU	 0x03 	/* Request Sense	*/
#define  G0_FORM	 0x04 	/* Format Unit		*/
#define  G0_RBL	 	 0x05 	/* Read Block Limits	*/
#define  G0_REAS	 0x07 	/* Reassign Blocks	*/
#define  G0_READ	 0x08 	/* Read			*/
#define  G0_RECE	 0x08 	/* Receive		*/
#define  G0_PRIN	 0x0A 	/* Print		*/
#define  G0_WRIT	 0x0A 	/* Write		*/
#define  G0_SEEK	 0x0B 	/* Seek			*/
#define  G0_SLEW	 0x0B 	/* Slew & Print		*/
#define  G0_TSEL	 0x0B 	/* Track Select		*/
#define  G0_RR	 	 0x0F 	/* Read Reverse		*/
#define  G0_FLUS	 0x10 	/* Flush Buffer		*/
#define  G0_WF	 	 0x10 	/* Write Filemark	*/
#define  G0_SPAC	 0x11 	/* Space		*/
#define  G0_INQU	 0x12 	/* Inquiry		*/
#define  G0_VERI	 0x13 	/* Verify		*/
#define  G0_RBD	 	 0x14 	/* Recover Buffer Data	*/
#define  G0_MSEL	 0x15 	/* Mode Select		*/
#define  G0_RESU	 0x16 	/* Reserve Unit		*/
#define  G0_RELU	 0x17 	/* Release Unit		*/
#define  G0_COPY	 0x18 	/* Copy			*/
#define  G0_ERAS	 0x19 	/* Erase		*/
#define  G0_MSEN	 0x1A 	/* Mode Sense		*/
#define  G0_LOAD	 0x1B 	/* Load/Unload		*/
#define  G0_STOP	 0x1B 	/* Stop Unit		*/
#define  G0_STPR	 0x1B 	/* Stop Print		*/
#define  G0_SCAN	 0x1B 	/* Scan			*/
#define  G0_RDR	 	 0x1C 	/* Read Diag. Resp.	*/
#define  G0_SD	 	 0x1D 	/* Send Diag.		*/
#define  G0_PREV	 0x1E 	/* Prevent Removal	*/
#define  G0_RLOG	 0x1F 	/* Read Log		*/


struct cmd_g1 {
  unsigned char g1_b0, g1_b1, g1_b2, g1_b3, g1_b4, g1_b5,
		g1_b6, g1_b7, g1_b8, g1_b9;
};

#define  g1_op_code	g1_b0	/* operation code		   */
#define  g1_lun_dfr	g1_b1	/* logical unit / D / F / R	   */
#define  g1_lba3	g1_b2	/* logical block address (part 3)  */
#define  g1_lba2	g1_b3	/* logical block address (part 2)  */
#define  g1_lba1	g1_b4	/* logical block address (part 1)  */
#define  g1_lba0	g1_b5	/* logical block address (part 0)  */
#define  g1_res6	g1_b6	/* reserved			   */
#define  g1_len1	g1_b7	/* transfer (etc.) length (part 1) */
#define  g1_len0	g1_b8	/* transfer (etc.) length (part 0) */
#define  g1_flag_link	g1_b9	/* flag & link bits		   */


#define  G1_DWP	    0x24 	/* Define Window Parms.	*/
#define  G1_GWP	    0x25 	/* Get Window Parms.	*/
#define  G1_RCAP    0x25 	/* Read Capacity	*/
#define  G1_REAS    0x27 	/* Reassign Blocks	*/
#define  G1_READ    0x28 	/* Read			*/
#define  G1_RX	    0x28 	/* Read Extended	*/
#define  G1_WRIT    0x2A 	/* Write		*/
#define  G1_WX	    0x2A 	/* Write Extended	*/
#define  G1_LOCA    0x2B 	/* Locate		*/
#define  G1_SEEK    0x2B 	/* Seek			*/
#define  G1_SX	    0x2B 	/* Seek Extended	*/
#define  G1_WVER    0x2E 	/* Write & Verify	*/
#define  G1_VERI    0x2F 	/* Verify		*/
#define  G1_SDH	    0x30 	/* Search Data High	*/
#define  G1_SDE	    0x31 	/* Search Data Equal	*/
#define  G1_SDL	    0x32 	/* Search Data Low	*/
#define  G1_MPOS    0x32 	/* Medium Position	*/
#define  G1_SLIM    0x33 	/* Set Limits		*/
#define  G1_PF	    0x34 	/* Pre-Fetch		*/
#define  G1_RPOS    0x34 	/* Read Position	*/
#define  G1_GDS	    0x34 	/* Get Data Status	*/
#define  G1_FCAC    0x35 	/* Flush Cache		*/
#define  G1_LCAC    0x36 	/* Lock/Unlock Cache	*/
#define  G1_RDD	    0x37 	/* Read Defect Data	*/
#define  G1_COMP    0x39 	/* Compare		*/
#define  G1_CVER    0x3A 	/* Copy & Verify	*/
#define  G1_WBUF    0x3B 	/* Write Buffer		*/
#define  G1_RBUF    0x3C 	/* Read Buffer		*/
#define  G1_RLON    0x3E 	/* Read Long		*/
#define  G1_WLON    0x3F 	/* Write Long		*/


/*
|| macros to convert binary to big-endian (msb) values, etc.
*/

#define get_b0(x)	( x      & 0xff)
#define get_b1(x)	((x>>8)  & 0xff)
#define get_b2(x)	((x>>16) & 0xff)


/*
|| define some returns from dslib.c
*/

struct dsreq *dsopen(const char *opath, uint oflags);

void dsclose(struct dsreq *dsp);

int testunitready00(struct dsreq *dsp);
int rewind01(struct dsreq *dsp);
int requestsense03(struct dsreq *dsp, caddr_t data, uint datalen, int vu);
int read08(struct dsreq *dsp, caddr_t data, uint datalen, uint lba, int vu);
int write0a(struct dsreq *dsp, caddr_t data, uint datalen, uint lba, int vu);
int inquiry12(struct dsreq *dsp, caddr_t data, uint datalen, int vu);
int modeselect15(struct dsreq *dsp, caddr_t data, uint datalen,
	          int save, int vu);
int reservunit16(struct dsreq *dsp, caddr_t data, uint datalen,
	          int tpr, int tpdid, int extent, int res_id, int vu);
int releaseunit17(struct dsreq *dsp, int tpr, int tpdid, int extent,
		  int res_id, int vu);
int modesense1a(struct dsreq *dsp, caddr_t data, uint datalen,
	         int pagectrl, int pagecode, int vu);
int senddiagnostic1d(struct dsreq *dsp, caddr_t data, uint datalen,
		      int self, int dofl, int uofl, int vu);

int readcapacity25(struct dsreq *dsp, caddr_t data, uint datalen,
	            uint lba, int pmi, int vu);
int readextended28(struct dsreq *dsp, caddr_t data, uint datalen,
	            uint lba, int vu);
int writeextended2a(struct dsreq *dsp, caddr_t data, uint datalen,
	            uint lba, int vu);

int doscsireq(int fd, struct dsreq *dsp);
void fillg0cmd(struct dsreq *dsp, uchar_t *cmd, uchar_t b0, uchar_t b1,
	       uchar_t b2, uchar_t b3, uchar_t b4, uchar_t b5);
void fillg1cmd(struct dsreq *dsp, uchar_t *cmd, uchar_t b0, uchar_t b1,
	       uchar_t b2, uchar_t b3, uchar_t b4, uchar_t b5, uchar_t b6,
	       uchar_t b7, uchar_t b8, uchar_t b9);
void filldsreq(struct dsreq *dsp, uchar_t *data, uint datalen, uint flags);

/* used primarily when you do not want to have dsdebug!=0, but want to
 * print most of the "useful" info after an error occurs.  Prints 
 * a subset of what dsdebug=1 prints after command completes in doscsireq() 
*/
void ds_showcmd(dsreq_t *dsp);

char *ds_vtostr(uint v, struct vtab *table);
char *ds_ctostr(uint v, struct ctab *table);

#ifdef __cplusplus
}
#endif
#endif /* __DSLIB_H__ */
