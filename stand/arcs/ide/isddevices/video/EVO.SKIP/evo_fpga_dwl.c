/*
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



#include <arcs/io.h>
#include <arcs/io.h>
#include "evo_diag.h" 


#define ALTERA_SIZE_FBC 90000 /* check the file */


__uint32_t
evo_download_file(int argc, char **argv)
{
    register char *bp;
    ULONG fd, cc;
    long rc;
    __uint32_t lderr=0;
    __uint32_t errors=0;
    __uint32_t XdataSize, reclen, pck_type, flag = 0;

    __uint32_t device, dev_addr;
    unsigned short conf_word, stat_word, done_word;
    __uint32_t byte_count = 0;
    __uint32_t total_bytes = 0;

    char fname[256], HexLen[5];
    uchar_t i;
    char server[100] , str[300];
    int buf[10240] ;
    char temp[128];



    flag = 0;
    /*Initialize Buffers with -999*/
    for(byte_count=0; byte_count<10240; byte_count++){
	buf[byte_count] = -999;
    }
    byte_count=0;

    if (argc < 4) {
	msg_printf(SUM, "usage:evo_dlf [-f file -s server -d device_number]\n");
	msg_printf(SUM, "Device numbers are - 	\n\
		0 - VOC \n\
		1 - SCl1 I/P \n\
		2 - SCL1 O/P \n\
		3 - SCL2 I/P \n\
		4 - SCL2 O/P \n\
		5 - Bitpicker \n\
		6 - Reference Generator	\n\
		7 - NSQ Filter \n\
		8 - Frame Reset \n");
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

	case 'd' :
        if (argv[0][2]=='\0') { /* has white space */
            device =  atoi(argv[1]);
            argc--; argv++;
        }
        else { /* no white space */
            device =  atoi(argv[0]);
        }
        break;

        default  :
        msg_printf(SUM, "usage: evo_dlf [-f file -s server -d device]\n");
	msg_printf(SUM, "Device numbers are - 	\n\
		0 - VOC \n\
		1 - SCl1 I/P \n\
		2 - SCL1 O/P \n\
		3 - SCL2 I/P \n\
		4 - SCL2 O/P \n\
		5 - Bitpicker \n\
		6 - Reference Generator	\n\
		7 - NSQ Filter \n\
		8 - Frame Reset \n");
    	}
	argc--; argv++;
    }

       str[0] = '\0' ; 
#if 1
        strcpy(str, "bootp()");

        if (flag) {
            strcat(str, server);
            strcat(str, ":");
        }
        strcat(str, fname);
#else
        strcat(str, fname);
#endif


    switch (device) {
        case 0 :
		dev_addr = CONF_VOC_FPGA; 
		conf_word = 0x1; /*Invert conf_word to force appropriate pin low*/
		break;
        case 1 :
		dev_addr = CONF_SCL1_IP_FPGA;
		conf_word = 0x2; /*Invert conf_word to force appropriate pin low*/
		break;
        case 2 :
		dev_addr = CONF_SCL1_OP_FPGA;
		conf_word = 0x4; /*Invert conf_word to force appropriate pin low*/
		break;
        case 3 :
		dev_addr = CONF_SCL2_IP_FPGA; 
		conf_word = 0x8; /*Invert conf_word to force appropriate pin low*/
		break;
        case 4 :
		dev_addr = CONF_SCL2_OP_FPGA; 
		conf_word = 0x10; /*Invert conf_word to force appropriate pin low*/
		break;
        case 5 :
		dev_addr = CONF_BPICKER_FPGA;
		conf_word = 0x20; /*Invert conf_word to force appropriate pin low*/
		break;
        case 6 :
		dev_addr = CONF_REFGEN_FPGA;
		conf_word = 0x40; /*Invert conf_word to force appropriate pin low*/
		break;
        case 7 :
		dev_addr = CONF_NSQ_FIL_FPGA;
		conf_word = 0x80; /*Invert conf_word to force appropriate pin low*/
		break;
        case 8 :
		dev_addr = CONF_FM_RST_FPGA;
		conf_word = 0x100; /*Invert conf_word to force appropriate pin low*/
		break;
    }

/*
		before downloading Altera, check the revision register 
		if the revision register is 1, then do not download , otherwise
		download
*/

    	msg_printf(SUM, "Using bootp to open file %s\n", str);
	i = 0;
	while ( i < 10) {

    	if ((rc = Open ((CHAR *)str, OpenReadOnly, &fd)) != ESUCCESS) {
			if ( i == 9 ) {
    			msg_printf(SUM, "Error: Unable to access file %s\n", str);
    			msg_printf(SUM, "Error: tried opening the file 10 times" );
				errors++;
				Close(fd);
    			return (0) ;
			}
    	}
		else { 
				i = 15; /* opened file successfully ; now quit while loop */ 
    				msg_printf(SUM, "Opened File %s successfully\n", str);
				}
		i++;
	}

    i = 0 ;
    bp = (char *)temp ;

    rc = Read(fd, bp, 1, &cc);
    while ((rc == ESUCCESS) && cc) {
		if ((*bp != '\n') && (*bp != ','))  {
			*bp++;
		} 
		else 	{
			if (*bp == ','){
				*bp = '\0' ;
				i = atoi(temp);
				/*msg_printf (SUM, "Byte is %d\n", i);*/
				buf[byte_count] = i;
				i=0;
				byte_count++;
			}
				bp = (char *)temp ;
		}
    		rc = Read(fd, bp, 1, &cc);
    }
    if (rc != ESUCCESS) {
   	msg_printf(SUM,"Again Trouble in Reading \n");
	Close(fd);
	return (0);
    }
	
    *bp--;
    *bp = '\0' ;
    i = atoi(temp);
    msg_printf (SUM, "Last Byte is %d\n", i);
    buf[byte_count] = i;
    byte_count++;
    total_bytes=byte_count;
    buf[byte_count] = -999; 
    msg_printf(SUM,"Buffer has %d bytes\n", byte_count);

    i=0;
    byte_count=0;
    _evo_outhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF, 0xffff); /*write ff first */
    us_delay(50); /*hold pins low for 10 microseconds*/
    /*_evo_outhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF, 0x0);  ~(conf_word) drives pin low*/

    _evo_outhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF, (~(conf_word))); /*~(conf_word) drives pin low*/ 
    us_delay(50); /*hold pins low for 10 microseconds*/
    _evo_outhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF, 0xffff); /*return pin to high*/


    while(buf[byte_count] != -999){
	stat_word = 0x0;
	i = buf[byte_count];
	/*Wait for fpga to be ready before downloading next byte*/
	us_delay(50);
 	stat_word = _evo_inhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF);
	while( (stat_word & conf_word) != conf_word ) {
		msg_printf(DBG, "waiting for ready status, stat_word is 0x%x\n", stat_word);
		stat_word = 0x0;
 		stat_word = _evo_inhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF);
	}
	us_delay(50);
	if( (stat_word & conf_word) == conf_word) {
		msg_printf(DBG, "FPGA ready - stat_word is 0x%x\n", stat_word);
		_evo_outb(((__paddr_t)evobase->gio_pio_base+(__paddr_t)dev_addr), i);
		msg_printf(DBG, "Byte No. %d, data 0x%x\n", byte_count, buf[byte_count]);
		byte_count++;
	}
	/*Checking done word*/
	us_delay(50);
	done_word=0x0;
    	done_word = _evo_inhw(evobase->evo_misc+(__paddr_t)MISC_FPGA_DONE);
	msg_printf(DBG, "Done word is 0x%x\n", done_word);
    	if( ((done_word & conf_word) == conf_word) && (byte_count < total_bytes) ){ 
		msg_printf(SUM, "\n WARNING: Altera Down Load Completed Before End of Buffer\n");
		msg_printf(SUM, "WARNING: Only %d bytes dowloaded\n",byte_count);
		break;
    	} 

    }

    done_word = _evo_inhw(evobase->evo_misc+(__paddr_t)MISC_FPGA_DONE);
    if((done_word & conf_word) == conf_word){ 
	msg_printf(SUM, "\n Altera Down Load Successful \n");
    } 
    else {
		msg_printf(SUM, "\n Altera Down Load Incomplete \n");
		msg_printf(SUM, "\n %d Bytes downloaded\n", byte_count);
		Close(fd);
		return (0);
	}

     Close(fd);
     return (1);
}


