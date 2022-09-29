#ifndef _EV_MSG_H_
#define _EV_MSG_H_
/*
 * msg.h -- class definition for interprocess event message passing mechanism
 *          for netvis client/server communication
 */

 
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <sys/time.h> 
#include <tt_c.h>
#include "event.h"


#define EV_MSG_ERR_CONNECT	-1
#define EV_MSG_ERR_SEND         -2
#define EV_MSG_ERR_RECEIVE      -3
#define EV_MSG_ERR_REG_RESP     -4

class EV_msg {
public:
    void close (void);
    virtual EV_handle open (void);
    virtual EV_stat send (EV_event  *event, char *noSends = NULL);
    virtual EV_stat recv (EV_event *event, char *noSends = NULL) = 0;
    ~EV_msg(void);
protected:
    EV_msg (void);
    EV_stat receiveStart (void);
    EV_stat receiveEnd (EV_event *event);
    Tt_message m, msgin;	
    int mark;
    int ttreplymark;
    char *ttop;
    Tt_status tterr;
    Tt_status ttstatus;
    int opnum; 
    int ttfd;
    char *ttpid;
};

class EV_clnt_msg : public EV_msg {
public:
    int open(void);
    EV_stat send(EV_event *ep, char *noSends = NULL);
    EV_stat recv(EV_event *ep, char *noSends = NULL);
    
};

class EV_srvr_msg : public EV_msg {
public:
    int open(void);
    EV_stat send(EV_event *ep, char *noSends = NULL);
    EV_stat recv(EV_event *ep, char *noSends = NULL);
    
};
#endif
