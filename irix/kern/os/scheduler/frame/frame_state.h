/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __FRAME_STATE_H__
#define __FRAME_STATE_H__

/*
 * Frame Scheduler State
 */
#define FRS_FLAG_ENABLED    0x0001  /* frame scheduled process */
#define FRS_FLAG_CONTROLLER 0x0002  /* frame scheduler controller process */

#define FRS_FLAG_RUN        0x0004  /* process has been run within minor frame */
#define FRS_FLAG_YIELD      0x0008  /* process has yielded */
#define FRS_FLAG_JOIN       0x0010  /* process has joined */
#define FRS_FLAG_EXIT       0x0020  /* process is exiting */
  
#define _isFrameScheduled(ut)    ((ut)->ut_frsflags & FRS_FLAG_ENABLED)
#define _initFrameScheduled(ut)  (ut)->ut_frsflags = FRS_FLAG_ENABLED
#define _endFrameScheduled(ut)   (ut)->ut_frsflags = 0

#define _isFRSController(ut)     ((ut)->ut_frsflags & FRS_FLAG_CONTROLLER)
#define _makeFRSController(ut)   (ut)->ut_frsflags |= FRS_FLAG_CONTROLLER
#define _unFRSController(ut)     (ut)->ut_frsflags &= ~FRS_FLAG_CONTROLLER
#define _endFRSController(ut)    (ut)->ut_frsflags = 0

#define _isFSRun(ut)             ((ut)->ut_frsflags & FRS_FLAG_RUN)
#define _makeFSRun(ut)           (ut)->ut_frsflags |= FRS_FLAG_RUN
#define _unFSRun(ut)             (ut)->ut_frsflags &= ~FRS_FLAG_RUN

#define _isFSYield(ut)           ((ut)->ut_frsflags & FRS_FLAG_YIELD)
#define _makeFSYield(ut)         (ut)->ut_frsflags |= FRS_FLAG_YIELD
#define _unFSYield(ut)           (ut)->ut_frsflags &= ~FRS_FLAG_YIELD

#define _isFSJoin(ut)            ((ut)->ut_frsflags & FRS_FLAG_JOIN)
#define _makeFSJoin(ut)          (ut)->ut_frsflags |= FRS_FLAG_JOIN

#define _isFSExiting(ut)         ((ut)->ut_frsflags & FRS_FLAG_EXIT)
#define _makeFSExiting(ut)       (ut)->ut_frsflags |= FRS_FLAG_EXIT

#endif /* __FRAME_STATE_H__ */
