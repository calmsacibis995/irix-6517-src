/*-- fpdevice.h ---------------
  
   This is a class that is used to support any device accessible
   from the DS driver.
*/

#ifndef DSDEVICE_H
#define DSDEVICE_H


#include <dslib.h>

#include "DiskDevice.h"

#define MAXLBLLEN 128

struct DsDevice 
   {
    struct DiskDevice diskdevice;
    char volumeLabel[MAXLBLLEN + 1];
    struct dsreq *dsp;
   };

/* SCSI data structures and macros */

struct esense_reply 
   {
    unsigned char er_ibvalid:1,        /* information bytes valid */
                  er_class:3,        /* error class */
                  er_code:4;        /* error code */
    unsigned char    er_segment;        /* segment number for copy cmd */
    unsigned char    er_filemark:1,        /* file mark */
                     er_endofmedium:1,    /* end-of-medium */
                     er_badlen:1,        /* incorrect length */
                     er_rsvd2:1,        /* reserved */
                     er_sensekey:4;        /* sense key */
    unsigned char    er_infomsb;        /* MSB of information byte */
    unsigned int     er_info:24,        /* bits 23 - 0 of info "byte" */
                     er_addsenselen:8;    /* additional sense length */
    unsigned int     er_rsvd8;        /* copy status (unused) */
    unsigned char    er_addsensecode;    /* additional sense code */
    
    /* the following are used for tape only as of 27-Feb-89 */
    
    unsigned char    er_qualifier;        /* sense code qualifier */
    unsigned char  er_rsvd_e;
    unsigned char  er_rsvd_f;
    unsigned int   er_err_count:24,    /* three bytes of data error counter */
                   er_stat_13:8;        /* byte 0x13 - discrete status bits */
    unsigned char  er_stat_14;        /* byte 0x14 - discrete status bits */
    unsigned char  er_stat_15;        /* byte 0x15 - discrete status bits */
    unsigned int   er_rsvd_16:8,
                   er_remaining:24;    /* bytes 0x17..0x19 - remaining tape */
        
    /* technically, there can be additional bytes of sense info
     * here, but we don't check them, so we don't define them
     */
};

/*
 * sense keys
 */
#define    SENSE_NOSENSE        0x0    /* no error to report */
#define    SENSE_RECOVERED        0x1    /* recovered error */
#define    SENSE_NOTREADY        0x2    /* target not ready */
#define    SENSE_MEDIA        0x3    /* media flaw */
#define    SENSE_HARDWARE        0x4    /* hardware failure */
#define    SENSE_ILLEGALREQUEST    0x5    /* illegal request */
#define    SENSE_UNITATTENTION    0x6    /* drive attention */
#define    SENSE_DATAPROTECT    0x7    /* drive access protected */
#define    SENSE_ABORTEDCOMMAND    0xb    /* target aborted command */
#define    SENSE_VOLUMEOVERFLOW    0xd    /* eom, some data not transfered */
#define    SENSE_MISCOMPARE    0xe    /* source/media data mismatch */

/*
 * inquiry data
 */
struct inquiry_reply
   {
    unsigned char    ir_devicetype;        /* device type, see below */
    unsigned char    ir_removable:1,        /* removable media */
                     ir_typequalifier:7;    /* device type qualifier */
    unsigned char    ir_zero1:2,        /* reserved */
                     ir_ecmaversion:3,    /* ECMA version number */
                     ir_ansiversion:3;    /* ANSI version number */
    unsigned char    ir_zero2:4,        /* reserved */
                     ir_rspdatafmt:4;    /* response data format */
    unsigned char    ir_addlistlen;        /* additional list length */
    unsigned char    ir_zero3[3];        /* vendor unique field */
    char             ir_vendorid[8];        /* vendor name in ascii */
    char             ir_productid[16];    /* product name in ascii */
    char             ir_revision[32];    /* revision level info in ascii */
    char             ir_endofid[1];        /* just a handle for end of id info */
   };
#endif

