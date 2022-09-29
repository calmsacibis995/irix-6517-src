
/*
 *	(C) COPYRIGHT CRAY RESEARCH, INC.
 *	UNPUBLISHED PROPRIETARY INFORMATION.
 *	ALL RIGHTS RESERVED.
 */
#ifdef DEBUG
#define RPC_MUTEX_DEBUG
#endif

#include <dce/dce.h>
#include <commonp.h>
#include <com.h>
#include <cominit.h>
#include <sys/ioctl.h>
#include <krpc_helper.h>
#include <krpc_helper_data.h>
#include "sys/idbgentry.h"


extern int rpc_PrintedOne;		/* Flag to say we found entries */

/*  The following are defined in krpc_helper.c, and thus must be
	defined here.  These need to be changed if krpc_helper.c
	changes. */

extern unsigned long krpc_helper_gc_time;
extern unsigned long krpc_helper_buffer_size;


extern rpc_mutex_t krpc_helperlock;
extern rpc_cond_t krpc_helperCond;

extern int krpc_helperflag;
extern int krpc_helper_inited;
extern int krpc_queues_inited;
extern rpc_mutex_t krpc_helperlock;

extern struct krpc_helper *pending, *freelist, *in_service;

extern long lwm, freecount, highpriority;

/*
 * krpc_helpers's states bits
 */
#define KRPC_HELPER_DONE          0       /* buffer not in use */
#define KRPC_HELPER_REQREADY      1       /* client has finished request buffer */
#define KRPC_HELPER_RESPREADY     2       /* response/error ready for kernel client */
#define KRPC_HELPER_WORKING       4       /* helper is working on request */

/* states of the krpc-helper subsystem (krpc_helperflag) */

#define KRPC_HELPER_CLOSED             0x01 /* subsystem is shutdown */
#define KRPC_HELPER_CLOSE_IN_PROGRESS  0x02 /* subsystem shutting down */
#define KRPC_HELPER_OPEN               0x04 /* subsystem ready to process reques
ts */
#define KRPC_HELPER_OPEN_IN_PROGRESS   0x08 /* subsystem coming on line */



/*
 *	Print global krpc_helper information
 */
/*ARGSUSED*/
int
pr_global_helper (
int	verboseLevel,
char	*spstr)
{

 if (krpc_helper_inited) {
   qprintf ("%skrpc_helperlock * = 0x%x, should be a mutex lock\n",
		spstr, &krpc_helperlock);
   qprintf ("%skrpc_helperCond * = 0x%x, should be a sv_t \n",
		spstr, &krpc_helperCond);
   qprintf ("%skrpc_helperflag = %d, ", spstr, krpc_helperflag);
   switch(krpc_helperflag) {
	case KRPC_HELPER_OPEN:
		qprintf("KRPC_HELPER_OPEN\n");
		break;
	case KRPC_HELPER_CLOSED:
		qprintf("KRPC_HELPER_CLOSED\n");
		break;
	case KRPC_HELPER_OPEN_IN_PROGRESS:
		qprintf("KRPC_HELPER_OPEN_IN_PROGRESS\n");
		break;
	case KRPC_HELPER_CLOSE_IN_PROGRESS:
		qprintf("KRPC_HELPER_CLOSE_IN_PROGRESS\n");
		break;
	default:
		qprintf("UNKNOWN\n");
    }
    qprintf ("%skrpc_helper_buffer_size = %d\n", 
		spstr, krpc_helper_buffer_size);
    qprintf ("%skrpc_helper_gc_time (max time to wait for response) = %d\n\n",
	spstr, krpc_helper_gc_time);
  } else {
    qprintf("%s krpc_helper subsystem not initialized\n\n");
  }
    qprintf("%spending=0x%x\n", spstr, pending);
    qprintf("%sin_service=0x%x\n", spstr, in_service);
    qprintf("%sfreelist=0x%x\n", spstr, freelist);
  if (krpc_queues_inited) {
    if (QUEUE_EMPTY(pending)) {
	qprintf("%sNo pending requests\n",spstr);
    } else {
	qprintf("\n%s Head of pending requests = 0x%x\n", spstr,
		pending);
    }
    if (QUEUE_EMPTY(in_service)) {
	qprintf("%sNo in_service requests\n",spstr);
    } else {
	qprintf("\n%s Head of in_service requests = 0x%x\n", spstr,
		in_service);
    }
    if ((freecount+highpriority) <= lwm) {
	qprintf("%sONLY NEW HIGH PRIORITY REQUESTS EXECUTING\n", 
		spstr);
    }
    if (QUEUE_EMPTY(freelist)) {
	qprintf("%sNO NEW REQUESTS EXECUTING\n", 
		spstr);
    }
    qprintf("%sfreecount %d + highpriority %d < lwm %d = %d\n",
		spstr, freecount, highpriority, lwm, 
		((freecount+highpriority) <= lwm));
  } else {
    qprintf("%sno kprc_helper queues not initialized\n",
		spstr);
  }


   return(0);
}

void
pr_helper_entry(struct krpc_helper *ent)
{
	qprintf("Entry 0x%x, next 0x%x, prev 0x%x\n", ent,
		ent->next, ent->prev);
	qprintf("State is ");
	switch(ent->states) {
	   case KRPC_HELPER_DONE: 
		qprintf("KRPC_HELPER_DONE\n");
		qprintf("   buffer not in use\n");
	   case KRPC_HELPER_REQREADY: 
		qprintf("KRPC_HELPER_REQREADY\n");
		qprintf("   client has finished request buffer\n");
	   case KRPC_HELPER_RESPREADY: 
		qprintf("KRPC_HELPER_RESPREADY\n");
		qprintf("   response/error ready for kernel client\n");
	   case KRPC_HELPER_WORKING: 
		qprintf("KRPC_HELPER_WORKING\n");
		qprintf("   helper is working on request\n");
	   debug:
		qprintf("Unknown\n");
        }
	qprintf("returnCode 0x%x, requestID 0x%x, opcode 0x%x\n",
		ent->returnCode, ent->requestID, ent->opcode);
	qprintf("Started at 0x%x, basic buffer 0x%x\n", ent->started,
		ent->buffer);
	qprintf("marshalling/unmarshalling position 0x%x\n", ent->bufptr);
	qprintf("client process condition variable at 0x%x\n", ent->ccond);
}

void
idbg_helper_queues(__psint_t addr)
{

   int          addrset=0;
   int          verboseLevel=0;
   char         spstr[40];
   struct krpc_helper *q_ptr;
   struct krpc_helper *ent;


   spstr[0]='\0';
   if (addr != 0) {
      addrset++;
   }

      q_ptr=pending;
      qprintf("pending queue entries:\n");
      for (ent=q_ptr->next; ent != pending; ent=ent->next ) {
	pr_helper_entry(ent);
      }
      q_ptr=in_service;
      qprintf("\n\nin_service queue entries:\n");
      for (ent=q_ptr->next; ent != in_service; ent=ent->next ) {
	pr_helper_entry(ent);
      }
      qprintf("\n\n");

   return;
}

void
idbg_helper(__psint_t addr)
{

   int          addrset=0;
   int          verboseLevel=0;
   char         spstr[40];


   spstr[0]='\0';
   if (addr != 0) {
      addrset++;
   }

   rpc_PrintedOne = 0;
   if (addrset) {
      qprintf("Use parameter of 0 to get global entries.\n");
   }
   else {
      strcat (spstr, "   ");
      pr_global_helper(verboseLevel, spstr);
      spstr[strlen(spstr)-3]='\0';
   }

   return;
}
