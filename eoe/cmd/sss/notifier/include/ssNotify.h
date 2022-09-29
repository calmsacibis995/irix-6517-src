#ifndef SSNOTIFY_H
#define SSNOTIFY_H

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


#include "globals.h"

#define xconfirmloc	"/usr/bin/X11/xconfirm"
#define ssplayloc	"/usr/sbin/ssplay"
#define qpageloc	"/usr/etc/qpage"
#define quedir		"/var/tmp/ssNque"

#define GUI_FAILURE 2 
#define PGR_FAILURE 4 

#define PGR_PORT 444

#define critical_text   "Critical System Error"
#define error_text      "System Error"
#define warning_text    "System Warning"
#define info_text       "System Information"
#define critical        "critical"
#define error           "error"
#define warning         "warning"
#define info          	"info"

#define default_service "default"

static short Gui   = OFF;
static short Exec  = OFF;
static short Pager = OFF;
static short Email = OFF;
static short Audio = OFF;

static short dflag = OFF;	/* Debug mode		*/
static short Kflag = OFF;       /* Delete flag          */
static short Pflag = OFF;	/* Arbitrary program	*/
static short pflag = OFF;	/* Pager id 		*/
static short sflag = OFF;	/* Email Subject	*/
static short mflag = OFF;	/* E-mail string	*/
static short fflag = OFF;	/* E-mail filename	*/
static short Mflag = OFF;	/* Exec message string	*/
static short Fflag = OFF;	/* Exec filename	*/
static short Cflag = OFF;	/* Pager content	*/
static short cflag = OFF;	/* GUI content		*/
static short Sflag = OFF;	/* Paging service 	*/
static short tflag = OFF;	/* GUI title		*/
static short Eflag = OFF;	/* email address	*/
static short oflag = OFF;	/* email options	*/
static short COMP  = OFF;	/* compression		*/
static short ENCO  = OFF;       /* compression          */
static short Dflag = OFF;	/* display		*/
static short Aflag = OFF;       /* Show message on Console*/
static short gflag = OFF;	/* geometry	 	*/ 
static short iflag = OFF;	/* icon		 	*/
static short aflag = OFF;	/* audio		*/
static short nflag = OFF;	/* priority		*/
static short Nflag = OFF;       /* # times to try failure */
static short Qflag = OFF;       /* Qpage server to use  */
static short qflag = OFF;	/* If ON - Q'ing is on  */

static short log_errors = 0;

char q_server[BUFFER];
char soundfile[BUFFER];
char priority[2];
char eaddrlist[BUFFER];	/* String of e-mail addresses		*/
char exec_this[BUFFER];	/* program to be executed		*/
char subject[BUFFER];	/* Title Email Subject			*/ 
char message[BUFFER];
char Message[BUFFER];
char *filename;
char *Filename;
char *orig_content;
char content[BUFFER];
char acontent[BUFFER];
char Content[BUFFER];
char title[BUFFER];
char *options;
char displayhost[BUFFER];
char geometry[BUFFER];
char iconimage[BUFFER];
char Service[20];
char pageridlist[BUFFER];
char char_tries[2];
int tries;

#endif
