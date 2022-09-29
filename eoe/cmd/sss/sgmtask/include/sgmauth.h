/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                */
/* All Rights Reserved.                                                      */
/*                                                                           */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics Inc.;     */
/* contents of this file may not be disclosed to third parties, copied or    */
/* duplicated in any form, in whole or in part, without the prior written    */
/* permission of Silicon Graphics, Inc.                                      */
/*                                                                           */
/* RESTRICTED RIGHTS LEGEND:                                                 */
/* Use,duplication or disclosure by the Government is subject to restrictions*/
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data    */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or  */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -   */
/* rights reserved under the Copyright Laws of the United States.            */
/*                                                                           */
/*---------------------------------------------------------------------------*/

#ident           "$Revision: 1.4 $"

#ifndef __SGMAUTH_H_
#define __SGMAUTH_H_

/*---------------------------------------------------------------------------*/
/*   Standard Key and Mask to be used in case of setting authorization       */
/*   on remote machine                                                       */
/*---------------------------------------------------------------------------*/

#define    STANDARD_KEY           0x72AB9260
#define    STANDARD_MASK          0x6BAC72B8

/*---------------------------------------------------------------------------*/
/*   Some definitions for Encryption and Authentication                      */
/*---------------------------------------------------------------------------*/

#define    ERR_AUTH               0
#define    STANDARD_AUTH          (1 << 0)
#define    CUSTOM_AUTH            (1 << 1)
#define    NO_AUTH                (1 << 2)

#define    FULLENCR               (1 << 8)
#define    KEYENCR                (1 << 9)

/*---------------------------------------------------------------------------*/
/*   Some definitions to be used by sgmauth.c file and other files           */
/*---------------------------------------------------------------------------*/

#define    INSERTAUTH             1
#define    DELETEAUTH             2

#define   CREATEAUTHIFNOTFOUND                 1

#define   AUTHSIZE                50

#endif /* __SGMAUTH_H_ */
