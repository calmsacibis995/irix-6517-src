/* xdr_ld.h - xdr for additional dbxWorks structures */

/*
modification history
--------------------
01b,29aug92,maf  minor cleanups for ANSI compliance.
01a,05jun90,llk  extracted from xdr_dbx.h.
*/

#ifndef __INCxdrldh
#define __INCxdrldh 1

#define MAXSTRLEN 256
#define MAXTBLSZ 100

/*
 * structure used to pass back the information for a single file
 * loaded in VxWorks
 */
struct ldfile {
	char 	*name;
	int 	txt_addr;
	int 	data_addr;
	int 	bss_addr;
};
typedef struct ldfile ldfile;

/*
 * structure used to return a list of all files loaded over to 
 * VxWorks. (VX_STATE_INQ return)
 */
struct ldtabl {
	u_int tbl_size;
	ldfile *tbl_ent;
};
typedef struct ldtabl ldtabl;


bool_t xdr_ldfile();
bool_t xdr_ldtabl();

#endif	/* __INCxdrldh */
