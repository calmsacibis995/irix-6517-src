#ifndef _NV_EV_LOGGER_H
#define _NV_EV_LOGGER_H
/*
 * logger.h -- definition of the class that manages the event daemon event log 
		facility
 * 
 * $Revision: 1.1 $
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

#include <stdio.h>
#include "event.h"

#define EV_LOG_SIZE		2000	    //default
#define EV_MAX_LOG_SIZE		10000	    //max
#define EV_NUM_LOGS		2	    //default
#define EV_MAX_NUM_LOGS		10	    //max
#define EV_LOGFILE_NAME		"/usr/lib/netvis/eventlog"  //default
#define EV_MAX_LOG_REC_SIZE	256
#define FILE_OPEN_TYPE_APPEND	"a+"


class EV_logrec {
public:
    void format (EV_event *ep, char *logrec);
};

class EV_logger {
public:
    EV_logger(int logSize = EV_MAX_LOG_SIZE, 
	      int numLogs = EV_NUM_LOGS, 
	      const char *logfname = EV_LOGFILE_NAME);
    ~EV_logger(void);
    EV_stat openLog(void);
    void logit (EV_event *ep);
private:
    int logsize;    //actual
    int numlogs;    //actual
    int currsize;   // records
    int flushcnt;   // controls buffering of the stream with FILE *log
    FILE *log;
    char *logFileNames[EV_MAX_NUM_LOGS];
    EV_logrec logrec;
    void cycleLogs(void);
       
};

#endif
