/* --------------------------------------------------------------------------- */
/* -                             SSRV_MIME.H                                 - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                  */
/* All Rights Reserved.                                                        */
/*                                                                             */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;      */
/* the contents of this file may not be disclosed to third parties, copied or  */
/* duplicated in any form, in whole or in part, without the prior written      */
/* permission of Silicon Graphics, Inc.                                        */
/*                                                                             */
/* RESTRICTED RIGHTS LEGEND:                                                   */
/* Use, duplication or disclosure by the Government is subject to restrictions */
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data      */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or    */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -     */
/* rights reserved under the Copyright Laws of the United States.              */
/*                                                                             */
/* --------------------------------------------------------------------------- */
#ifndef H_SSRV_MIME_H
#define H_SSRV_MIME_H

/* --------------------------------------------------------------------------- */
/* Well known MIME types */
extern const char szMimeText_Plain[];
extern const char szMimeText_Html[];

#ifdef __cplusplus
extern "C" {
#endif

const char *ssrvFindContentTypeByExt(const char *ext);
const char *ssrvFindContentTypeByFname(const char *fname);
int ssrvIsHtmlResource(const char *fname);

#ifdef __cplusplus
}

#endif
#endif /* #ifndef H_SSRV_MIME_H */
