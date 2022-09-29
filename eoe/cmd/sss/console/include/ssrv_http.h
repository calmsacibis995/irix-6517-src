/* --------------------------------------------------------------------------- */
/* -                              SSRV_HTTP.H                                - */
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
#ifndef H_SSRV_HTTP_H
#define H_SSRV_HTTP_H
/* ------------------------------------------------------------------------- */
/* HTTP Status Codes                                                         */
#define HTTP_CONTINUE                      100
#define HTTP_SWITCHING_PROTOCOLS           101
#define HTTP_PROCESSING                    102
#define HTTP_OK                            200
#define HTTP_CREATED                       201
#define HTTP_ACCEPTED                      202
#define HTTP_NON_AUTHORITATIVE             203
#define HTTP_NO_CONTENT                    204
#define HTTP_RESET_CONTENT                 205
#define HTTP_PARTIAL_CONTENT               206
#define HTTP_MULTI_STATUS                  207
#define HTTP_MULTIPLE_CHOICES              300
#define HTTP_MOVED_PERMANENTLY             301
#define HTTP_MOVED_TEMPORARILY             302
#define HTTP_SEE_OTHER                     303
#define HTTP_NOT_MODIFIED                  304
#define HTTP_USE_PROXY                     305
#define HTTP_TEMPORARY_REDIRECT            307
#define HTTP_BAD_REQUEST                   400
#define HTTP_UNAUTHORIZED                  401
#define HTTP_PAYMENT_REQUIRED              402
#define HTTP_FORBIDDEN                     403
#define HTTP_NOT_FOUND                     404
#define HTTP_METHOD_NOT_ALLOWED            405
#define HTTP_NOT_ACCEPTABLE                406
#define HTTP_PROXY_AUTHENTICATION_REQUIRED 407
#define HTTP_REQUEST_TIME_OUT              408
#define HTTP_CONFLICT                      409
#define HTTP_GONE                          410
#define HTTP_LENGTH_REQUIRED               411
#define HTTP_PRECONDITION_FAILED           412
#define HTTP_REQUEST_ENTITY_TOO_LARGE      413
#define HTTP_REQUEST_URI_TOO_LARGE         414
#define HTTP_UNSUPPORTED_MEDIA_TYPE        415
#define HTTP_RANGE_NOT_SATISFIABLE         416
#define HTTP_EXPECTATION_FAILED            417
#define HTTP_UNPROCESSABLE_ENTITY          422
#define HTTP_LOCKED                        423
#define HTTP_INTERNAL_SERVER_ERROR         500
#define HTTP_NOT_IMPLEMENTED               501
#define HTTP_BAD_GATEWAY                   502
#define HTTP_SERVICE_UNAVAILABLE           503
#define HTTP_GATEWAY_TIME_OUT              504
#define HTTP_VERSION_NOT_SUPPORTED         505
#define HTTP_VARIANT_ALSO_VARIES           506
#define HTTP_NOT_EXTENDED                  510
/* ------------------------------------------------------------------------- */
#define is_HTTP_INFO(x)                    (((x) >= 100)&&((x) < 200))
#define is_HTTP_SUCCESS(x)                 (((x) >= 200)&&((x) < 300))
#define is_HTTP_REDIRECT(x)                (((x) >= 300)&&((x) < 400))
#define is_HTTP_ERROR(x)                   (((x) >= 400)&&((x) < 600))
#define is_HTTP_CLIENT_ERROR(x)            (((x) >= 400)&&((x) < 500))
#define is_HTTP_SERVER_ERROR(x)            (((x) >= 500)&&((x) < 600))
/* ------------------------------------------------------------------------- */
#define is_need_close_connection(x) (((x) == HTTP_BAD_REQUEST)           || ((x) == HTTP_REQUEST_TIME_OUT)      || \
                                     ((x) == HTTP_LENGTH_REQUIRED)       || ((x) == HTTP_REQUEST_ENTITY_TOO_LARGE) || \
                                     ((x) == HTTP_REQUEST_URI_TOO_LARGE) || ((x) == HTTP_INTERNAL_SERVER_ERROR) || \
                                     ((x) == HTTP_SERVICE_UNAVAILABLE)   || ((x) == HTTP_NOT_IMPLEMENTED))
/* ------------------------------------------------------------------------- */
/* HTTP Methods                                                              */
#define HTTPM_UNKNOWN                      0  /* HTTP method: unknown */
#define HTTPM_GET                          1  /* HTTP method: GET */
#define HTTPM_HEAD                         2  /* HTTP method: HEAD */
#define HTTPM_PUT                          3  /* HTTP method: PUT */
#define HTTPM_POST                         4  /* HTTP method: POST */
#define HTTPM_DELETE                       5  /* HTTP method: DELETE */
#define HTTPM_CONNECT                      6  /* HTTP method: CONNECT */
#define HTTPM_OPTIONS                      7  /* HTTP method: OPTIONS */
#define HTTPM_TRACE                        8  /* HTTP method: TRACE */

#endif /* #ifndef H_SSRV_HTTP_H */
