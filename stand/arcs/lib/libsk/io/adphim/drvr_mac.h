/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/io/adphim/RCS/drvr_mac.h,v 1.1 1997/06/05 23:38:08 philw Exp $ */
/***************************************************************************
*                                                                          *
* Copyright 1993 Adaptec, Inc.,  All Rights Reserved.                      *
*                                                                          *
* This software contains the valuable trade secrets of Adaptec.  The       *
* software is protected under copyright laws as an unpublished work of     *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.  The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.                                                    *
*                                                                          *
***************************************************************************/

/***************************************************************************
*
*  Module Name:   DRVR_MAC.H
*
*  Description:
*                 Macro definitions referenced only when building HIM for
*                 driver (OPTIMA mode or STANDARD mode)
*
*  Programmers:   Paul von Stamwitz
*                 Chuck Fannin
*                 Jeff Mendiola
*                 Harry Yang
*    
*  Notes:         NONE
*
*  Revisions -
*
***************************************************************************/


/***************************************************************************
*  Macro referenced for common to driver
***************************************************************************/
#define  PHM_POSTCOMMAND(config_ptr)   Ph_PostCommand((config_ptr))
#define  PHM_INDEXCLEARBUSY(config_ptr,scb) \
				(config_ptr)->CFP_HaDataPtr->indexclearbusy((config_ptr),(scb))
#define  PHM_REQUESTSENSE(scb_ptr,base) \
				(scb_ptr)->SP_ConfigPtr->CFP_HaDataPtr->requestsense((scb_ptr),(base))
#define  PHM_BIOSACTIVE(config_ptr) (config_ptr)->CFP_BiosActive
#define  PHM_SCBACTIVE(config_ptr,scb)  \
            ((config_ptr)->CFP_HaDataPtr->a_ptr.active_ptr[(scb)] != \
            NOT_DEFINED)
#define  PHM_CLEARTARGETBUSY(config_ptr,scb) \
				(config_ptr)->CFP_HaDataPtr->cleartargetbusy((config_ptr),(scb))
#define  PHM_TARGETABORT(config_ptr,scb_ptr,base) \
               Ph_TargetAbort((config_ptr),(scb_ptr),(base))
#define  PHM_SCBPTRISBOOKMARK(config_ptr,scb_ptr) ((scb_ptr) == \
               &((config_ptr)->CFP_HaDataPtr->scb_mark))

