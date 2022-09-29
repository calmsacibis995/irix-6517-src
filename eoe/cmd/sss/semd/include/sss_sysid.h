/*----------------------------------------------------------*/
/* sss_sysid.h                                              */
/*----------------------------------------------------------*/

#ifndef __SSS_SYSID_H
#define __SSS_SYSID_H

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include "ssdbapi.h"

/*----------------------------------------------------------*/
/* some defines                                             */
/*----------------------------------------------------------*/

#define SSS_HOSTNAME                            1
#define SSS_SERIALNUM                           2
#define SSS_IPADDRESS                           3
#define SYSTEM_TABLE                            "system"
#define SYSTEM_SYSID				"sys_id"
#define SYSTEM_SERIAL				"sys_serial"
#define SYSTEM_HOST				"hostname"
#define SYSTEM_IP				"ip_addr"
#define SYSTEM_ACTIVE				"active"
#define SYSTEM_LOCAL				"local"
#define MAX_STR_LEN                             1024
#define LOCAL					1

/*----------------------------------------------------------*/
/* system_info structure definitions                        */
/*----------------------------------------------------------*/

typedef struct system_info_s {
    __uint64_t       system_id;              /* unique id for a system      */
    char             *hostname_full;         /* Hostname of the system      */
    char             *hostname_1dot;         /*         ""                  */
    char             *hostname_nodots;       /*         ""                  */
    char             *serialnum;             /* Serial number of the system */
					     /* This serial number is a char*/
					     /* * for the reason that high  */
					     /* end systems like IP19..IP27 */
					     /* have serial numbers starting*/
					     /* with a character (S or K)   */
    char             *ipaddr;                /* IP Address of the machine   */
    __uint32_t       active;                 /* active flag                 */
    __uint32_t       local;                  /* local flag                  */

} system_info_t;

typedef struct system_info_list_s {
    struct system_info_s      *node;
    struct system_info_list_s *next;
} system_info_list_t;

/*----------------------------------------------------------*/
/* Function prototypes                                      */
/*----------------------------------------------------------*/

__uint64_t init_sysinfolist(ssdb_Client_Handle c, ssdb_Error_Handle e, int i);
void free_sysinfolist(void);
__uint64_t sgm_get_sysid(system_info_t **ppSys, char *str);

#endif /* __SSS_SYSID_H */
