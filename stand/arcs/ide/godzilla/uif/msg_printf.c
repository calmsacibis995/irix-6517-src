/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.22 $"

#include <stdarg.h>
#include <saioctl.h>
#include <sys/cpu.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>
#include <ide_msg.h>
#include <sys/RACER/sflash.h>
#include <arcs/io.h>
#include <arcs/errno.h>


#define PUTBUFSIZE	0xc00
#define MAXMSGLENGTH	160

#ifdef PROM
static __psint_t spReportlevel;
__psint_t *Reportlevel=&spReportlevel;
#endif

short msg_where;  /* indicates destination of output */
static char putbuf[PUTBUFSIZE];  /* storage for msg_printf() output */
unsigned int putbufndx;  /* index to next character in putbuf */

unsigned int putbufndx;  /* index to next character in putbuf */
int failflag;

#ifndef PROM
/*********** MESSAGE LOGGING FOR SERVER SYSTEMS ***************/
static char *msglog = NULL; /* malloc'ed storage for msg log */
#define DEFAULT_MSG_LOG_SIZE (50 * 1024) 
int msglogsize = 0;
/* used by pager() when we print the msg log */
extern pagesize;
unsigned int msglogndx;
extern int pager(char *, int , int *);
/*********** MESSAGE LOGGING FOR SERVER SYSTEMS ***************/
#endif

/* Drop message in the PDS section of the flash.
 */
static void
flash_log(int prcpu, char *fmt, char *ap)
{
	char buf[256];
	char *p = buf;

	if (prcpu) {
		sprintf(buf,"[CPU %d] ",cpuid());
		p += 8;
	}
	vsprintf(p,fmt,ap);
	flash_pds_log(buf);
}

/*
 *  msg_printf - message printf
 *
 *  This function is intended to be the formatted print tool common to
 *  all diagnostic tests.  It is patterned after printf() but requires
 *  that a "message level" be provided with each call.  Actual output
 *  only occurs when this message level is less than or equal to the
 *  symbol value pointed to by the globally defined "Reportlevel".
 *  Also provided is the ability to temporarily buffer messages when
 *  buffering mode is turned on via the externally defined variable
 *  "msg_where".
 *
 *  WARNING:  For message buffering to work messages must not be longer
 *            than MAXMSGLENGTH.
 */
/*VARARGS2*/
void
msg_printf(int msglevel, char *fmt, ...)
{
    extern short msg_where;  /* indicates destination of output */
    static char errmsg[] = "\r\t\t\t\t\t\t\t\t- ERROR -\n";
    int prcpu = msglevel & PRCPU;
    int log = msglevel & LOG;
    va_list ap;
    int i, n;

#if MFG_USED
    if (msglevel == ERR) {
	log |= LOG;
    }
#endif

    msglevel &= ~(PRCPU|LOG);

    if (msglevel == ERR)
    {
	ip30_set_cpuled(LED_AMBER);
	/*
	 *  We use a failure flag to tell us whether the failure LED should
	 *  be on or off at any given time.  This way we can do things like
	 *  blink the LED during long tests and leave it in its proper state
	 *  at the end of the test.
	 */
	failflag = 1;
    }

    if ( !(msg_where & PRW_BUF) && putbufndx )
    {
	/*  We're NOT in "buffering" mode and there's something in the
	 *  buffer, so print it out. 
	 */
	printf("%s", putbuf);
	putbufndx = 0;
    }

    if (msglevel <= *Reportlevel)
    {
	va_start(ap,fmt);
	if (msg_where & PRW_BUF)
	{
	    /*
	     *  We`re in "buffering" mode (we must be waiting for the
	     *  graphics console to be initialized).
	     */

	    if ((putbufndx + sizeof(errmsg)) < (PUTBUFSIZE - MAXMSGLENGTH))
	    {
		if (msglevel == ERR)
		{
		    sprintf(&putbuf[putbufndx], "%s", errmsg);
		    /*
		     *  Move the buffer pointer to end of newly added string.
		     */
		    putbufndx += sizeof(errmsg) - 1;
		}
		if (prcpu) {
			sprintf(&putbuf[putbufndx],"[CPU %d] ",cpuid());
			vsprintf(&putbuf[putbufndx+8], fmt, ap);
		}
		else
			vsprintf(&putbuf[putbufndx], fmt, ap);

		/*
		 *  Move the buffer pointer to end of newly added string.
		 *  NOTE: The pointer is left pointing at the terminating
		 *        NULL char so that it will be overwritten and the
		 *        buffer will end up containing one big string.
		 */
		while ( (putbuf[putbufndx] != '\0') &&
		        (putbufndx < (PUTBUFSIZE-1)) )
		    putbufndx++;

	    }
	}

#ifndef PROM
	/********** BEGIN SERVER MESSAGE LOGGING *************/
	if (msg_where & MSG_LOG) 
	{
	  /* log and print output messages */
	  if ((msglogndx + sizeof(errmsg)) < (msglogsize - MAXMSGLENGTH))
	  {
	    if (msglevel == ERR)
	      {
		/* log the message */
		sprintf(msglog+msglogndx, "%s", errmsg);
		msglogndx += sizeof(errmsg) - 1;
	      }

	    /* log the message */
	    vsprintf(msglog+msglogndx, fmt, ap);
	    while ( (*(msglog+msglogndx) != '\0') &&
		   (msglogndx < (msglogsize-1)) ) {
	      msglogndx++;
	    }
	    
	  } else {
	    /* Ouch! message buffer overflow */
	    msg_where |= MSG_LOG_OVERFLOW;
	  }

	  /* just print the message out */
	  if (msglevel == ERR)
	    {
	      printf("%s", errmsg);
	    }
	  vprintf(fmt, ap);
	}
	/********** END SERVER MESSSAGE LOGGING **********/
#endif
	else if ((msg_where & PRW_BUF) == 0) {
	    if (msglevel == ERR)
	    {
		printf("%s", errmsg);
	    }
	    if (prcpu)
		printf("[CPU %d] ",cpuid());
	    vprintf(fmt, ap);

	    if (log)
		flash_log(prcpu,fmt,ap);

	    if (msg_where & PRW_SERIAL) {
		vttyprintf(fmt, ap);
	    }
	}
	va_end(ap);
    }
}

extern int gfx_testing;

/*
 *  buffon - msg_printf buffering ON
 *
 *  If using the graphics console, turn on buffering within msg_printf().
 */
void
buffon(void)
{
    if (console_is_gfx()) {
        gfx_testing = 1;
	msg_where |= PRW_BUF;  /* buffering is ON */
	msg_where &= ~PRW_CONS;
    }
}


/*
 *  buffoff - msg_printf buffering OFF
 */
void
buffoff(void)
{
    gfx_testing = 0;
    msg_where |= PRW_CONS;  /* buffering is OFF */
    msg_where &= ~PRW_BUF;
    msg_printf(VRB, "");   /* dummy msg_printf call to flush buffer */

}

/*
 * ttyprint - control printing of messages to the serial port.
 * 
 * Usage: ttyprint 1 -- enable messages to the serial port.
 *	  ttyprint 0 -- disable messaging to the serial port.
 *
 */

int
ttyprint(int argc, char *argv[])
{
   if (argc == 2) {
	switch (argv[1][0]) {
		case '1':
			if (console_is_gfx())
				msg_where |= PRW_SERIAL;
			break;
		case '0':
		default:
			msg_where &= ~PRW_SERIAL;
			break;
	}
	msg_printf(DBG, "ttyprint is set to %d\n",
		   (msg_where & PRW_SERIAL) != 0);
   }
   else {
	msg_printf(SUM, "Usage: ttyprint 0 -- disable messaging to the serial port.\n");
	msg_printf(SUM, "       ttyprint 1 -- enable messaging to the serial port.\n");
   }
   return 0;
}

#if !defined(PROM)

/*********************************
 * SERVER message logging code *
 *********************************/

/*
 * msglog_start
 *              Allocate a large buffer and start logging IDE
 *              msg_printfs
 *              
 * arguments:
 *       -b xxx  Buffer size in bytes.  Defaults to 100Kb
 *       -r      Restart msg logging. Don't allocate new buffer
 *
 */
msglog_start(int argc, char *argv[])
{
  int i;

  msg_where &= ~MSG_LOG;

  /* parse arguments */
  if (argc < 2) {
    if (msglog != NULL) {
      /* just restart */
      bzero(msglog, msglogsize);
    } else {
      /* no size specified; default is 100k */
      msglogsize = DEFAULT_MSG_LOG_SIZE;
      if ( (msglog = (char *)malloc(msglogsize)) == NULL) {
	msg_printf(JUST_DOIT,
		   "\nERR: Unable to allocate %d byte message log buffer\n",
		   msglogsize);
	msg_printf(JUST_DOIT,
		   "\nERR: Message log not started\n");
	if (msg_where & PRW_SERIAL) {
		ttyprintf(
		   "\nERR: Unable to allocate %d byte message log buffer\n",
		   msglogsize);
		ttyprintf("\nERR: Message log not started\n");
	}	
	return 1;
      }
    }
    msglogndx = 0;
    msg_where |= MSG_LOG;

  } else {
    for (i=1; i < argc; i++) {
      if (argv[i][0] != '-')
	goto msglog_start_usage;
      else
	switch (argv[i][1]) {
	case 'b':
	  i++;
	  /* verify the buffer size argument was passed */
	  if (i >= argc) {
	    goto msglog_start_usage;
	  }

	  (void)atob(argv[i], &msglogsize);

	  if (msglog != NULL) {
	    free(msglog);
	  }

	  if ( (msglog = (char *)malloc(msglogsize)) == NULL) {
	    msg_printf(JUST_DOIT,
		       "\nERR: Unable to alloc %d byte message log buffer\n",
		       msglogsize);
	    msg_printf(JUST_DOIT,
		       "\nERR: Message log not started\n");
	    if (msg_where & PRW_SERIAL) {
		ttyprintf(
		   "\nERR: Unable to allocate %d byte message log buffer\n",
		   msglogsize);
		ttyprintf( "\nERR: Message log not started\n");
	    }	
	    return 1;
	  }
	  msglogndx = 0;
	  msg_where |= MSG_LOG;
	  
	  break;

	default:
	  goto msglog_start_usage;
	}
    }
  }
  
  return 0;

msglog_start_usage:
    printf("usage: msglog_start [-b SIZE]\n");
    return 1;
}

#if NOTDEF  /* apparently unused */
/*
 * msglog_stop
 *            Disable IDE msg_printf logging 
 *
 */
msglog_stop(int argc, char *argv[])
{
  msg_where &= ~MSG_LOG;
  return 0;
}
#endif 

/*
 * msglog_info:
 *            print information about the msg log such as total
 *            size and size in use.
 */
msglog_info()
{

  printf("MsgLog Bufsize = %d[0x%x] bytes\n", msglogsize, msglogsize);
  printf("In Use = %d[0x%x] bytes\n", msglogndx, msglogndx);

  /* print log status */
  printf("Status = ");
  if (msg_where & MSG_LOG) {
    printf("Logging Messages");
  } else {
    printf("Not Logging Messages");
  }

  if (msg_where & MSG_LOG_OVERFLOW) {
    printf(", Log Overflow");
  }
    printf("\n");

  return 0;
  
}

/*
 * msglog_print
 *             Print out the message log.  Printing the log
 *             momentarily turns off message logging.
 *
 */
msglog_print()
{
  char linebuf[80];
  int line = 0;
  char reenable = 0;

	if (msglog == NULL) {
		msg_printf(JUST_DOIT, "No message log\n");
		if (msg_where & PRW_SERIAL) {
			ttyprintf("No message log\n");
	        }
		return 0;
	}

  if (msg_where & MSG_LOG) {
    msg_where &= ~MSG_LOG;
    reenable++;
  }
  if (msg_where & MSG_LOG_OVERFLOW) {
    pager("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", pagesize, &line);
    pager("!!!!!!!!!!! WARNING !!!!!!!!!!!!!", pagesize, &line);
    pager("!!!!!! MESSAGE LOG OVERFLOW !!!!!", pagesize, &line);
    pager("!!!!!! DATA HAS BEEN LOST !!!!!!!", pagesize, &line);
    pager("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", pagesize, &line);
    msg_printf(JUST_DOIT,
	       "\n    - Press <ENTER> to continue -  ");
    gets(linebuf);
  }

  pager("\n*********** START OF MESSAGE LOG ***********", pagesize, &line);
  pager(msglog, pagesize, &line);
  pager("************ END OF MESSAGE LOG ************", pagesize, &line);

  if (reenable)
    msg_where |= MSG_LOG;

  return 0;
}	

#endif /*** MESSAGE LOGGING ******/
