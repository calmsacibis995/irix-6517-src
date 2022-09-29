/*
 *
 * Cosmo2 gio 1 chip hardware definitions
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */



#include "cosmo2_defs.h"
#include "cosmo2_mcu_def.h"
#include <arcs/io.h>
#include <arcs/errno.h>


#define XILINX_SIZE_FBC 90000 /* check the file */

extern int DwnLdXilinx (UWORD, UBYTE *, UWORD, UBYTE, UBYTE);
extern void Reset_SCALER_UPC(UWORD );
extern  void Reset_050_016_FBC_CPC(UWORD );
extern UBYTE getCMD(UWORD *);
extern UBYTE mcu_READ (__uint32_t);

__uint32_t
downld_xilinx(int argc, char **argv)
{
    register char *bp;
    ULONG fd, cc;
    long rc;
    __uint32_t lderr=0;
    __uint32_t i,  XdataSize, reclen, pck_type, flag = 0;
    UBYTE xilinx_download = 0;
    char fname[256], HexLen[5];
	char server[100] , str[300];
	UBYTE buf[1024] ;

	flag = mcu_READ ( VIDCH1_FBC_BASE + FBC_ID_REG);
	if ((flag&ID_CODE_MASK) != FBC_ID_REG_VERSION) 
		xilinx_download = 1; 
		

	flag = mcu_READ ( VIDCH2_FBC_BASE + FBC_ID_REG);
	if ((flag&ID_CODE_MASK) != FBC_ID_REG_VERSION)
		xilinx_download = 1; 

	if ( xilinx_download  == 0)
		return (1);

	flag = 0;

    if (argc < 3) {
   msg_printf(SUM, "usage:cosmo2_xilinx [-f bootp()machine:/path/file]\n");
    lderr++;
    return(lderr);
    }

/*
dksc(0,1,0)fbc.exo
*/

    argc--; argv++; /* Skip Test name */

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
        case 'f' :
        if (argv[0][2]=='\0') { /* has white space */
            sprintf(fname, "%s", &argv[1][0]);
            argc--; argv++;
        }
        else { /* no white space */
            sprintf(fname, "%s", &argv[0][2]);
        }
        break;
        case 's' :
        if (argv[0][2]=='\0') { /* has white space */
            sprintf(server, "%s", &argv[1][0]);
            argc--; argv++;
        }
        else { /* no white space */
            sprintf(server, "%s", &argv[0][2]);
        }
        flag=1;
        break;


        default  :
        msg_printf(SUM, "usage: d_xlx [-f bootp()machine:/path/fbc.file]\n");
    	}
	argc--; argv++;
    }

       str[0] = '\0' ; 
#if 0
        strcpy(str, "bootp()");

        if (flag) {
            strcat(str, server);
            strcat(str, ":");
        }
        strcat(str, fname);
#else
        strcat(str, fname);
#endif

/*
		before downloading Xilinx, check the revision register 
		if the revision register is 1, then do not download , otherwise
		download
*/

	i = 0;
	while ( i < 10) {

    	if ((rc = Open ((CHAR *)str, OpenReadOnly, &fd)) != ESUCCESS) {
			if ( i == 9 ) {
    			msg_printf(SUM, "Error: Unable to access file %s\n", str);
    			msg_printf(SUM, "Error: tried opening the file 10 times" );
				G_errors++;
				Close(fd);
    			return (0) ;
			}
    	}
		else { 
				i = 15; /* opened file successfully ; now quit while loop */ 
				}
		i++;
	}

    i = 0 ;
	bp = (char *)buf ;
	XdataSize = 0 ;

    while (((rc = Read(fd, bp, 1, &cc))==ESUCCESS) && cc) {
		if ((*bp != '\n') && (*bp != '\r') && (*bp != '\0') )  {
			msg_printf (DBG, "%c", *bp);
			*bp++;
		}
		else 	{
				msg_printf (DBG, "\n");
				HexLen[0] = buf[2] ; HexLen[1] = buf[3] ; HexLen[2] = '\0' ;
				atohex(HexLen, (int*)&reclen) ;
				HexLen[0] = buf[1] ; HexLen[1] = '\0' ;
				atohex(HexLen, (int*)&pck_type) ;

					if ((pck_type == 1) && (XdataSize == 0))
						pck_type = 0;
					else
					if ((pck_type == 1) && (XdataSize != 0))
						pck_type = 1;
					else
					if (pck_type == 9) 
						pck_type = 2;

				reclen = (reclen + 2) * 2;

				DwnLdXilinx(reclen, buf, cmd_seq, FBC_XILINX_CODE, pck_type);

		if ((XdataSize%50) == 0)
			msg_printf(SUM, "."); 

				XdataSize++ ;

				i = 0; bp = (char *)buf ;

				}
    	}
#if 0
	if (getCMD(resp) == 0)
		msg_printf(SUM, "\n Down Load successful \n");
	else {
		msg_printf(SUM, " \nDown Load not  successful \n");
		Close(fd);
		return (0);
	}
#endif

	if (rc != ESUCCESS) {
    		msg_printf(SUM,"Again Trouble in Reading \n");
			Close(fd);
			return (0);
	}
		msg_printf(SUM, "\n xilinx Down Load successful \n");

	Close(fd);
		return (1);
}


void
reset_channel0( )
{

    Reset_050_016_FBC_CPC(CHANNEL_0);
    Reset_SCALER_UPC (CHANNEL_0 );

}

void
reset_channel1( )
{

    Reset_050_016_FBC_CPC(CHANNEL_1);
    Reset_SCALER_UPC (CHANNEL_1 );

}


