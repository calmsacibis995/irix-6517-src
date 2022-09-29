/* --------------------------------------------------------------------------- */
/* -                             SSRV_UTIL.C                                 - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1999 Silicon Graphics, Inc.                                  */
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
/* $Revision: 1.11 $ */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "ssrv.h"
#include "ssrv_http.h"
#include "ssrv_main.h"
#include "ssrv_util.h"
#include "ssrv_mime.h"
#include "ssrv_base64.h"
#include "ssrv_swc.h"

/* --------------------------------------------------------------------------- */
extern int volatile debug_mode;              /* Debug mode flag */
extern int volatile silenceMode;             /* Silence mode flag */
extern int volatile cResourcePathSize;       /* Resource path size */
extern int volatile listen_port;             /* Server "Listen" port */
extern int volatile dir_listing_flg;         /* Directory listing enable/disable flag */
extern unsigned long volatile exit_phase;    /* Exit phase */
extern SSRVTATINFO est;                      /* Statistics storage struct */
#ifdef SSRV_SWITCHER_ON
extern SWCEXPORT swexf;                      /* "Switcher" export function structure */
#endif
extern char szServerVersionDate[];           /* Server Version Date string */
extern char szResourcePath[];                /* Resource path */
extern char szSwitcherLibPath[];             /* Switcher library path and name */
extern char szDefaultHtmlFileName[];         /* Default resource name instead "/" */
extern const char szDaemonName[];            /* WEB server name */
extern const char szLegalName[];             /* WEB server legal name */
extern const char szSSServerInfoSockPath[];  /* SSServer Info Socket name */
/* --------------------------------------------------------------------------- */
/* HTTP methods pair structure                                                 */
typedef struct s_sss_methods_pair {
 const char *str;                    /* Method name string:"GET","PUT",etc. */
 int num;                            /* Method number value: HTTPM_... */
} METHODPAIR;
/* --------------------------------------------------------------------------- */
/* Internal Resource Control Block                                             */
typedef struct s_sss_ircb {
 const char *name;                   /* Internal resource name */
 const char *mime_type;              /* Mime type (can be 0) */
 char *buffer;                       /* Resource buffer */
 int size;                           /* Size */
} IRCB;
/* --------------------------------------------------------------------------- */
static const char szServerNameInternal[]          = "LVAHTTPD";             /* Internal server name for HTTP response header */
static const char szCRLFString[]                  = "\015\012";             /* CRLF */
static const char szSlash[]                       = "/";                    /* Slash */
static const char szProtectedPrefix[]             = "/$protected";          /* Protected prefix */
static const char szNoCachePrefix[]               = "/$nocache";            /* No Cache prefix */
static const char szInternalInfoPrefix[]          = "/$ssrv/$internalinfo"; /* Internal server information */
static const char szSecuredArea[]                 = "Secured Area";         /* Realm name */
static const char szInternalResNameBackGif[]      = "/$ssrv/$internal_resource/back.gif";
static const char szInternalResNameFileGif[]      = "/$ssrv/$internal_resource/file.gif";
static const char szInternalResNameFolderGif[]    = "/$ssrv/$internal_resource/folder.gif";
static int ciProtectedPrefixSize                  = 0;
static int ciNoCachePrefixSize                    = 0;
static int ciInternalInfoPrefixSize               = 0;
/* --------------------------------------------------------------------------- */
/* Request fields:                                                             */
static const char szParName_Accept[]              = "Accept";
static const char szParName_Accept_Charset[]      = "Accept-Charset";
static const char szParName_Accept_Encoding[]     = "Accept-Encoding";
static const char szParName_Accept_Language[]     = "Accept-Language";
static const char szParName_Authorization[]       = "Authorization";
static const char szParName_From[]                = "From";
static const char szParName_Host[]                = "Host";
static const char szParName_If_Modified_Since[]   = "If-Modified-Since";
static const char szParName_If_Match[]            = "If-Match";
static const char szParName_If_None_Match[]       = "If-None-Match";
static const char szParName_If_Range[]            = "If-Range";
static const char szParName_If_Unmodified_Since[] = "If-Unmodified-Since";
static const char szParName_Max_Forwards[]        = "Max-Forwards";
static const char szParName_Proxy_Authorization[] = "Proxy-Authorization";
static const char szParName_Range[]               = "Range";
static const char szParName_Referer[]             = "Referer";
static const char szParName_User_Agent[]          = "User-Agent";
static const char szParName_User_IPaddr[]         = "User-IPaddr";             /* SSC specific key name */
static const char szParName_Connection[]          = "Connection";              /* Request/Response */
static const char szParName_Negotiate[]           = "Negotiate";
static const char szParName_Pragma[]              = "Pragma";
static const char szParName_WWW_Authenticate[]    = "WWW-Authenticate";        /* Response */
static const char szParName_Allow[]               = "Allow";                   /* Response */
static const char szParName_Location[]            = "Location";                /* Response */
/* --------------------------------------------------------------------------- */
/* Response fields:                                                            */
static const char szParName_Content_Length[]      = "Content-length";
static const char szParName_Content_Type[]        = "Content-type";
static const char szParName_Cache_Control[]       = "Cache-Control";
/* --------------------------------------------------------------------------- */
static const char szClose[]                       = "close";
static const char szKeepAlive[]                   = "keep-alive";
static const char szNoCache[]                     = "no-cache";
/* --------------------------------------------------------------------------- */
static const char szHTMLErrorBody_NotFound[] = "<HTML><HEAD><TITLE>Not Found</TITLE></HEAD><H1>Not Found</H1>\
<BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\">\
The requested object <B>\"%s\"</B> does not exist on this server. \
The link you followed is either outdated, inaccurate, \
or the server has been instructed not to let you have it.</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szHTMLErrorBody_InternalErr_CantLoadSwc[] = "<HTML><HEAD><TITLE>Internal Server Error</TITLE></HEAD>\
<H1>Internal Server Error</H1><BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\">\
Server can't load module <B>\"%s\"</B> for request processing.<br>%s</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szHTMLErrorBody_InternalErr_PostRequestBodySizeTooBig[] = "<HTML><HEAD><TITLE>Internal Server Error</TITLE></HEAD>\
<H1>Internal Server Error</H1><BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\">Too big \"POST\" request body.</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szHTMLErrorBody_InternalErr_PostRequestBodySizeIsBad[] = "<HTML><HEAD><TITLE>Internal Server Error</TITLE></HEAD>\
<H1>Internal Server Error</H1><BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\">\"POST\" request body has incorrect length.\
<B>(must be: %d, real size: %d)</B></BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szHTMLErrorBody_ReqURITooLarge[] = "<HTML><HEAD><TITLE>Request-URI Too Large</TITLE></HEAD>\
<H1>Request-URI Too Large</H1><BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\">\
Server can't handle URI longer than <B>%d</B> bytes. (real size: %d)</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szHTMLErrorBody_Unauthorized[] = "<HTML><HEAD><TITLE>Unauthorized Request</TITLE></HEAD>\
<H1>Unauthorized Request</H1><BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\">\
Your <B>\"UserName\"</B> or/and <B>\"Password\"</B> is/are incorrect.</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szHTMLErrorBody_MethodNotAllowed[] = "<HTML><HEAD><TITLE>Method Not Allowed</TITLE></HEAD>\
<H1>Method not allowed</H1><BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\">\
Server can't process current request. Please, correct request method.</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szHTMLErrorBody_ContentLengthRequired[] = "<HTML><HEAD><TITLE>Length Required</TITLE></HEAD>\
<H1>\"Content-Length\" Required</H1><BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\">\
Server can't process current request. Please, correct request parameter(s).</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szHTMLErrorBody_szCantParseHttpHeader[] = "<HTML><HEAD><TITLE>HTTP Error %d</TITLE></HEAD>\
<H1>HTTP Error %d</H1><BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\">\
Server can't process current request. Please, correct request parameter(s).</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szHTMLDirectoryListing_Header[] = "<HTML><HEAD><TITLE>Directory listing</TITLE></HEAD>\
<BODY TEXT=\"#000000\" BGCOLOR=\"#F9F9F9\"><H2>Directory listing: %s</H2><TABLE CELLPADDING=3 COLSPAN=6 WIDTH=\"100%%\">\
<TR><TD></TD>\
<TD>File name</TD><TD WIDTH=\"150\"></TD><TD>Size</TD><TD WIDTH=\"150\"></TD><TD>Date & Time</TD></TR>\
<TR><TD COLSPAN=\"6\"><HR></TD></TR>\n";
/* --------------------------------------------------------------------------- */
static const char szHTMLDirectoryListing_BodyDir[] = "\
<TR><TD WIDHT=100><IMG SRC=\"%s\" ALT=\"%s\"><TD><A HREF=\"%s/%s\">%s</A><TD><TD>Directory<TD><TD>%s\n";
/* --------------------------------------------------------------------------- */
static const char szHTMLDirectoryListing_BodyFile[] = "\
<TR><TD WIDHT=100><IMG SRC=\"%s\" ALT=\"%s\"><TD><A HREF=\"%s/%s\">%s</A><TD><TD>%lu bytes<TD><TD>%s\n";
/* --------------------------------------------------------------------------- */
static const char szHTMLDirectoryListing_End[] = "\n</TABLE></BODY></HTML>";

/* --------------------------------------------------------------------------- */
static METHODPAIR methodsTable[] = {
 { "GET",     HTTPM_GET },
 { "HEAD",    HTTPM_HEAD },
 { "PUT",     HTTPM_PUT },
 { "POST",    HTTPM_POST },
 { "DELETE",  HTTPM_DELETE },
 { "CONNECT", HTTPM_CONNECT },
 { "OPTIONS", HTTPM_OPTIONS },
 { "TRACE",   HTTPM_TRACE },
 { 0,         HTTPM_UNKNOWN } };
/* --------------------------------------------------------------------------- */
static METHODPAIR retcodesStrTable[] = {
 { "Continue",                      HTTP_CONTINUE },                      /* 100 */
 { "Switching Protocols",           HTTP_SWITCHING_PROTOCOLS },           /* 101 */
 { "Processing",                    HTTP_PROCESSING },                    /* 102 */
 { "OK",                            HTTP_OK },                            /* 200 */
 { "Created",                       HTTP_CREATED },                       /* 201 */
 { "Accepted",                      HTTP_ACCEPTED },                      /* 202 */
 { "Non Authoritative Info",        HTTP_NON_AUTHORITATIVE },             /* 203 */
 { "No Content",                    HTTP_NO_CONTENT },                    /* 204 */
 { "Reset Content",                 HTTP_RESET_CONTENT },                 /* 205 */
 { "Partial Content",               HTTP_PARTIAL_CONTENT },               /* 206 */
 { "Multi Status",                  HTTP_MULTI_STATUS },                  /* 207 */
 { "Multiple Choices",              HTTP_MULTIPLE_CHOICES },              /* 300 */
 { "Moved Permanently",             HTTP_MOVED_PERMANENTLY },             /* 301 */
 { "Moved Temporarily",             HTTP_MOVED_TEMPORARILY },             /* 302 */
 { "See Other",                     HTTP_SEE_OTHER },                     /* 303 */
 { "Not Modified",                  HTTP_NOT_MODIFIED },                  /* 304 */
 { "Use Proxy",                     HTTP_USE_PROXY },                     /* 305 */
 { "Temporary Redirected",          HTTP_TEMPORARY_REDIRECT  },           /* 307 */
 { "Bad Request",                   HTTP_BAD_REQUEST },                   /* 400 */
 { "Unauthorized",                  HTTP_UNAUTHORIZED },                  /* 401 */
 { "Payment Required",              HTTP_PAYMENT_REQUIRED },              /* 402 */
 { "Forbidden",                     HTTP_FORBIDDEN },                     /* 403 */
 { "Not Found",                     HTTP_NOT_FOUND },                     /* 404 */
 { "Method not allowed",            HTTP_METHOD_NOT_ALLOWED },            /* 405 */
 { "Not Acceptable",                HTTP_NOT_ACCEPTABLE },                /* 406 */
 { "Proxy Authentication Required", HTTP_PROXY_AUTHENTICATION_REQUIRED }, /* 407 */
 { "Request Timeout",               HTTP_REQUEST_TIME_OUT },              /* 408 */
 { "Conflict",                      HTTP_CONFLICT },                      /* 409 */
 { "Gone",                          HTTP_GONE },                          /* 410 */
 { "Length Required",               HTTP_LENGTH_REQUIRED },               /* 411 */
 { "Precondition Failed",           HTTP_PRECONDITION_FAILED },           /* 412 */
 { "Request Entity Too Large",      HTTP_REQUEST_ENTITY_TOO_LARGE },      /* 413 */
 { "Request-URI Too Large",         HTTP_REQUEST_URI_TOO_LARGE },         /* 414 */
 { "Unsupported Media Type",        HTTP_UNSUPPORTED_MEDIA_TYPE },        /* 415 */
 { "Range Not Satisfiable",         HTTP_RANGE_NOT_SATISFIABLE },         /* 416 */
 { "Expectation Failed",            HTTP_EXPECTATION_FAILED },            /* 417 */
 { "Unprocessable Entity",          HTTP_UNPROCESSABLE_ENTITY },          /* 422 */
 { "Locked",                        HTTP_LOCKED },                        /* 423 */
 { "Internal Server Error",         HTTP_INTERNAL_SERVER_ERROR },         /* 500 */
 { "Not Implemented",               HTTP_NOT_IMPLEMENTED },               /* 501 */
 { "Bad Gateway",                   HTTP_BAD_GATEWAY },                   /* 502 */
 { "Service Unavailable",           HTTP_SERVICE_UNAVAILABLE },           /* 503 */
 { "Gateway Timeout",               HTTP_GATEWAY_TIME_OUT },              /* 504 */
 { "Version Not supported",         HTTP_VERSION_NOT_SUPPORTED },         /* 505 */
 { "Variant Also Varies",           HTTP_VARIANT_ALSO_VARIES },           /* 506 */
 { "Not Extended",                  HTTP_NOT_EXTENDED },                  /* 510 */
 { 0,                               0 } };                                /* 000 */ 
/* --------------------------------------------------------------------------- */
static METHODPAIR argnameTable[] = {
 { szParName_Accept,              HHATTRN_ACCEPT },
 { szParName_Accept_Charset,      HHATTRN_ACCEPT_CHARSET },
 { szParName_Accept_Encoding,     HHATTRN_ACCEPT_ENCODING },
 { szParName_Accept_Language,     HHATTRN_ACCEPT_LANGUAGE },
 { szParName_Authorization,       HHATTRN_AUTHORIZATION },
 { szParName_From,                HHATTRN_FROM },
 { szParName_Host,                HHATTRN_HOST },
 { szParName_If_Modified_Since,   HHATTRN_IF_MODIFIED_SINCE },
 { szParName_If_Match,            HHATTRN_IF_MATCH },
 { szParName_If_None_Match,       HHATTRN_IF_NONE_MATCH },
 { szParName_If_Range,            HHATTRN_IF_RANGE },
 { szParName_If_Unmodified_Since, HHATTRN_IF_UNMODIFIED_SINCE },
 { szParName_Max_Forwards,        HHATTRN_MAX_FORWARDS },
 { szParName_Proxy_Authorization, HHATTRN_PROXY_AUTHORIZATION },
 { szParName_Range,               HHATTRN_RANGE },
 { szParName_Referer,             HHATTRN_REFERER },
 { szParName_User_Agent,          HHATTRN_USER_AGENT },
 { szParName_Connection,          HHATTRN_CONNECTION },
 { szParName_Negotiate,           HHATTRN_NEGOTIATE },
 { szParName_Pragma,              HHATTRN_PRAGMA }, 
 { szParName_Content_Length,      HHATTRN_CONTENT_LENGTH },
 { 0,                             HHATTRN_UNKNOWN } };
/* --------------------------------------------------------------------------- */
static char resource_back_gif[216] = {
'\x47', '\x49', '\x46', '\x38', '\x39', '\x61', '\x14', '\x00', '\x16', '\x00', 
'\xc2', '\x00', '\x00', '\xff', '\xff', '\xff', '\xcc', '\xff', '\xff', '\x99', 
'\x99', '\x99', '\x66', '\x66', '\x66', '\x33', '\x33', '\x33', '\x00', '\x00', 
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x21', '\xfe', '\x4e', 
'\x54', '\x68', '\x69', '\x73', '\x20', '\x61', '\x72', '\x74', '\x20', '\x69', 
'\x73', '\x20', '\x69', '\x6e', '\x20', '\x74', '\x68', '\x65', '\x20', '\x70', 
'\x75', '\x62', '\x6c', '\x69', '\x63', '\x20', '\x64', '\x6f', '\x6d', '\x61', 
'\x69', '\x6e', '\x2e', '\x20', '\x4b', '\x65', '\x76', '\x69', '\x6e', '\x20', 
'\x48', '\x75', '\x67', '\x68', '\x65', '\x73', '\x2c', '\x20', '\x6b', '\x65', 
'\x76', '\x69', '\x6e', '\x68', '\x40', '\x65', '\x69', '\x74', '\x2e', '\x63', 
'\x6f', '\x6d', '\x2c', '\x20', '\x53', '\x65', '\x70', '\x74', '\x65', '\x6d', 
'\x62', '\x65', '\x72', '\x20', '\x31', '\x39', '\x39', '\x35', '\x00', '\x21', 
'\xf9', '\x04', '\x01', '\x00', '\x00', '\x01', '\x00', '\x2c', '\x00', '\x00', 
'\x00', '\x00', '\x14', '\x00', '\x16', '\x00', '\x00', '\x03', '\x4b', '\x18', 
'\xba', '\xdc', '\xfe', '\x23', '\x10', '\xf2', '\x6a', '\x10', '\x33', '\x53', 
'\xbb', '\xa2', '\xce', '\xdc', '\xf5', '\x69', '\x9c', '\x37', '\x4e', '\x16', 
'\x76', '\x82', '\x90', '\x74', '\x06', '\x05', '\x37', '\x2a', '\x45', '\x1c', 
'\x6e', '\x58', '\x6d', '\x2f', '\xfb', '\x92', '\xeb', '\x35', '\x46', '\x50', 
'\xf1', '\x03', '\xea', '\x78', '\xc6', '\xa4', '\x91', '\xa6', '\x6c', '\x16', 
'\x28', '\x4e', '\xa7', '\x20', '\xda', '\x8c', '\xc0', '\xa8', '\x3a', '\x82', 
'\x40', '\x18', '\xd5', '\x56', '\x92', '\xde', '\xd0', '\x55', '\xfc', '\xe8', 
'\x91', '\xcf', '\x8b', '\x04', '\x00', '\x3b'};
/* --------------------------------------------------------------------------- */
static char resource_file_gif[221] = {
'\x47', '\x49', '\x46', '\x38', '\x39', '\x61', '\x14', '\x00', '\x16', '\x00', 
'\xc2', '\x00', '\x00', '\xff', '\xff', '\xff', '\xcc', '\xff', '\xff', '\x99', 
'\x99', '\x99', '\x33', '\x33', '\x33', '\x00', '\x00', '\x00', '\x00', '\x00', 
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x21', '\xfe', '\x4e', 
'\x54', '\x68', '\x69', '\x73', '\x20', '\x61', '\x72', '\x74', '\x20', '\x69', 
'\x73', '\x20', '\x69', '\x6e', '\x20', '\x74', '\x68', '\x65', '\x20', '\x70', 
'\x75', '\x62', '\x6c', '\x69', '\x63', '\x20', '\x64', '\x6f', '\x6d', '\x61', 
'\x69', '\x6e', '\x2e', '\x20', '\x4b', '\x65', '\x76', '\x69', '\x6e', '\x20', 
'\x48', '\x75', '\x67', '\x68', '\x65', '\x73', '\x2c', '\x20', '\x6b', '\x65', 
'\x76', '\x69', '\x6e', '\x68', '\x40', '\x65', '\x69', '\x74', '\x2e', '\x63', 
'\x6f', '\x6d', '\x2c', '\x20', '\x53', '\x65', '\x70', '\x74', '\x65', '\x6d', 
'\x62', '\x65', '\x72', '\x20', '\x31', '\x39', '\x39', '\x35', '\x00', '\x21', 
'\xf9', '\x04', '\x01', '\x00', '\x00', '\x01', '\x00', '\x2c', '\x00', '\x00', 
'\x00', '\x00', '\x14', '\x00', '\x16', '\x00', '\x00', '\x03', '\x50', '\x38', 
'\xba', '\xbc', '\xf1', '\x30', '\x0c', '\x40', '\xab', '\x9d', '\x23', '\xbe', 
'\x69', '\x3b', '\xcf', '\x11', '\xd7', '\x55', '\x22', '\xb8', '\x8d', '\xd7', 
'\x05', '\x89', '\x28', '\x31', '\xb8', '\xf0', '\x89', '\x52', '\x44', '\x6d', 
'\x13', '\xf2', '\x4c', '\x09', '\xbc', '\x80', '\x4b', '\x3a', '\x4b', '\xef', 
'\xc7', '\x0a', '\x02', '\x7c', '\x39', '\xe3', '\x91', '\xa8', '\xac', '\x20', 
'\x81', '\xcd', '\x65', '\xd2', '\xf8', '\x2c', '\x06', '\xab', '\x51', '\x29', 
'\xb4', '\x89', '\x8d', '\x76', '\xb9', '\x4c', '\x2f', '\x71', '\xd0', '\x2b', 
'\x9b', '\x79', '\xaf', '\xc8', '\x6d', '\xcd', '\xfe', '\x25', '\x00', '\x00', 
'\x3b'};
/* --------------------------------------------------------------------------- */
static char resource_folder_gif[225] = {
'\x47', '\x49', '\x46', '\x38', '\x39', '\x61', '\x14', '\x00', '\x16', '\x00', 
'\xc2', '\x00', '\x00', '\xff', '\xff', '\xff', '\xff', '\xcc', '\x99', '\xcc', 
'\xff', '\xff', '\x99', '\x66', '\x33', '\x33', '\x33', '\x33', '\x00', '\x00', 
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x21', '\xfe', '\x4e', 
'\x54', '\x68', '\x69', '\x73', '\x20', '\x61', '\x72', '\x74', '\x20', '\x69', 
'\x73', '\x20', '\x69', '\x6e', '\x20', '\x74', '\x68', '\x65', '\x20', '\x70', 
'\x75', '\x62', '\x6c', '\x69', '\x63', '\x20', '\x64', '\x6f', '\x6d', '\x61', 
'\x69', '\x6e', '\x2e', '\x20', '\x4b', '\x65', '\x76', '\x69', '\x6e', '\x20', 
'\x48', '\x75', '\x67', '\x68', '\x65', '\x73', '\x2c', '\x20', '\x6b', '\x65', 
'\x76', '\x69', '\x6e', '\x68', '\x40', '\x65', '\x69', '\x74', '\x2e', '\x63', 
'\x6f', '\x6d', '\x2c', '\x20', '\x53', '\x65', '\x70', '\x74', '\x65', '\x6d', 
'\x62', '\x65', '\x72', '\x20', '\x31', '\x39', '\x39', '\x35', '\x00', '\x21', 
'\xf9', '\x04', '\x01', '\x00', '\x00', '\x02', '\x00', '\x2c', '\x00', '\x00', 
'\x00', '\x00', '\x14', '\x00', '\x16', '\x00', '\x00', '\x03', '\x54', '\x28', 
'\xba', '\xdc', '\xfe', '\x30', '\xca', '\x49', '\x59', '\xb9', '\xf8', '\xce', 
'\x12', '\xba', '\xef', '\x45', '\xc4', '\x7d', '\x64', '\xa6', '\x29', '\xc5', 
'\x40', '\x7a', '\x6a', '\x89', '\x06', '\x43', '\x2c', '\xc7', '\x2b', '\x1c', 
'\x8e', '\xf5', '\x1a', '\x13', '\x57', '\x9e', '\x0f', '\x3c', '\x9c', '\x8f', 
'\x05', '\xec', '\x0d', '\x49', '\x45', '\xe1', '\x71', '\x67', '\x3c', '\xb2', 
'\x82', '\x4e', '\x22', '\x34', '\xda', '\x49', '\x52', '\x61', '\x56', '\x98', 
'\x56', '\xc5', '\xdd', '\xc2', '\x78', '\x82', '\xd4', '\x6c', '\x3c', '\x26', 
'\x80', '\xc3', '\xe6', '\xb4', '\x7a', '\xcd', '\x23', '\x2c', '\x4c', '\xf0', 
'\x8c', '\x3b', '\x01', '\x00', '\x3b'};
/* --------------------------------------------------------------------------- */
static IRCB srv_resource_tbl[] = {
{ szInternalResNameBackGif,   NULL, resource_back_gif,    sizeof(resource_back_gif) }, 
{ szInternalResNameFileGif,   NULL, resource_file_gif,    sizeof(resource_file_gif) },
{ szInternalResNameFolderGif, NULL, resource_folder_gif,  sizeof(resource_folder_gif) }
};
/* -------------------- sscIsInternalServerResource -------------------------- */
static IRCB *sscIsInternalServerResource(const char *url)
{ int i;
  if(url && url[0] == '/' && url[1] == '$')
  { for(i = 0;i < (sizeof(srv_resource_tbl)/sizeof(IRCB));i++)
    { if(!strcmp(srv_resource_tbl[i].name,url)) return &srv_resource_tbl[i];
    }
  }
  return 0;
}
/* -------------------------- ssrvRecvSocket --------------------------------- */
static int ssrvRecvSocket(TCBENTRY *tcb,char *buf,int bufsize)
{ int i;
  for(i = 0;!exit_phase;) if(((i = recv(tcb->as,buf,bufsize,0)) >= 0) || (errno != EINTR)) break;
  return i;
}
/* ----------------------- ssrvSendBuffer ------------------------------------ */
static int ssrvSendBuffer(TCBENTRY *tcb,void *buf,int size)
{ int retcode = 0;
  if(tcb && buf && size) retcode = send(tcb->as,buf,size,0);
#ifdef INCDEBUG    
  dprintf("SendBuffer: %d bytes, retcode = %d\n",size,retcode);
#endif
  return retcode;
}
/* ---------------------- ssrvSendRespHeader --------------------------------- */
static int ssrvSendRespHeader(TCBENTRY *tcb)
{ int retcode = 0;
  if(tcb && tcb->resphdr && tcb->rhrdactsize)
  { if(send(tcb->as,tcb->resphdr,tcb->rhrdactsize,0) == tcb->rhrdactsize) retcode++;
    else est.statConTotalSendRespHdrError++;
#ifdef INCDEBUG    
    dprintf("-- Send HTTP Header Response = retcode: %d, size: %d --\n\"%s\"\n",retcode,tcb->rhrdactsize,tcb->resphdr);
#endif
  }
  return retcode;
}
/* ---------------- ssrvGetReturnStatusStringByRetcode ----------------------- */
static const char *ssrvGetReturnStatusStringByRetcode(int retcode)
{ int i;
  for(i = 0;retcodesStrTable[i].str;i++)
   if(retcodesStrTable[i].num == retcode) return retcodesStrTable[i].str;
  return "Unknown Return Code";
}
/* ------------------------- ssrvGetHTTPMethodNumber ------------------------- */
static int ssrvGetHTTPMethodNumber(const char *str)
{ int i;
  int method = HTTPM_UNKNOWN;
  if(str && str[0])
  { for(i = 0;methodsTable[i].str;i++)
     if(!strcasecmp(str,methodsTable[i].str))
     { method = methodsTable[i].num;
       break;
     }
  }
  return method;
}
/* ------------------ ssrvGetHTTPRequestAttrNumber --------------------------- */
static int ssrvGetHTTPRequestAttrNumber(const char *str)
{ int i;
  int attrnum = HHATTRN_UNKNOWN;
  if(str && str[0])
  { for(i = 0;argnameTable[i].str;i++)
     if(!strcasecmp(str,argnameTable[i].str))
     { attrnum = argnameTable[i].num;
       break;
     }
  }
  return attrnum;
}
/* ------------------ ssrvFindRequestAttrNumber ------------------------------ */
static int ssrvFindRequestAttrNumber(TCBENTRY *tcb,int reqAttrNumber,int startIdx)
{ if(startIdx >= 0)
  { for(;startIdx < tcb->pactsize;startIdx++)
    { if(tcb->pbuf[startIdx].attrnum == reqAttrNumber) return startIdx;
    }
  }
  return -1;
}
/* -------------------- validateURLInternal ---------------------------------- */
static int validateURLInternal(char *url)
{ int i,o,justcopy;
  char tmp[4],*in,*oldurl;
  
  if(!url || !url[0]) return 0;
  justcopy = 0;

  for(in = (oldurl = url);;)
  { if(!justcopy)
    { if(*in == '%')
      { in++;
        if(isxdigit(*in) && isxdigit(*(in+1)))
        { tmp[0] = *in++; tmp[1] = *in++; tmp[2] = 0;
          if(sscanf(tmp,"%x",&i) != 1) return 0;
          *url++ = (char)(i&0xff);
        }
        else return 0; /* incorrect %2HEX */
      }
      else if(*in == '+') { *url++ = ' '; in++; }
      else if(*in == '?') { *url++ = *in++; justcopy++; }
      else { if((*url++ = *in++) == 0) break; }
    }
    else { if((*url++ = *in++) == 0) break; }
  }

  /* Check lead ".." */
  if((oldurl[0] == '.' && oldurl[1] == '.') &&
     (oldurl[2] == '/' || oldurl[2] == '\\' || !oldurl[2])) return 0;

  justcopy = 0;
  in = (url = oldurl);

  for(i = (o = 0);;)
  { if(!justcopy)
    { if(in[i] == '/')
      { while(in[i] == '/') i++;
        if(in[i] == '.' && in[i+1] == '.')
	{ if(!in[i+2]) return 0; /* incorrect "xxx/.." path */
          if(in[i+2] == '/')
	  { if(!o) return 0; /* incorrect "/../xxx" path */
	    i += 2;
            for(--o;o >= 0 && url[o] != '/';o--);	  
	  }
          else url[o++] = '/';
	}
	else url[o++] = '/';
      }
      else if(in[i] == '?') { url[o++] = in[i++]; justcopy++; }
      else
      { if((url[o++] = in[i++]) == 0) break;
      }
    }
    else { if((url[o++] = in[i++]) == 0) break; }
  }
  return 1;
}
/* --------------------------- ssrvValidateURL ------------------------------- */
static int ssrvValidateURL(TCBENTRY *tcb)
{ int i;
  char *url;

  if(tcb->url && tcb->urlsize)
  { for(i = 0;tcb->url[i] && tcb->url[i] != '?' && i < tcb->urlsize;i++)
    { if(tcb->url[i] == '\\') tcb->url[i] = '/';
    }
    if(tcb->urlsize >= 7 && tcb->url[4] == ':' && tcb->url[5] == '/' && tcb->url[6] == '/')
    { url = &tcb->url[7];
      while(*url && *url != '/') url++;
      if(!*url) { --url; *url = '/'; }
      tcb->url = url;
    }
    if(validateURLInternal(tcb->url))
    { tcb->urlsize = strlen(tcb->url);
      return 1;
    }
  }
  return 0;
}
/* --------------------------- ssrvGetHTTPVersion ---------------------------- */
static int ssrvGetHTTPVersion(TCBENTRY *tcb,char *hstr)
{ char *bp;
  for(bp = hstr;*hstr && *hstr != '/';hstr++);
  if(!*hstr) return 0;
  *hstr++ = 0;
  if(strcasecmp("HTTP",bp)) return 0;
  for(bp = hstr;*hstr && *hstr != '.';hstr++)
   if(!isdigit(*hstr)) return 0;
  if(!*hstr) return 0;
  *hstr++ = 0;
  if(sscanf(bp,"%d",&tcb->httpver_major) != 1) return 0;
  for(bp = hstr;isdigit(*hstr);hstr++);
  *hstr = 0;
  return (sscanf(bp,"%d",&tcb->httpver_minor) == 1) ? 1 : 0;
}
/* ----------------------- ssrvParseHTTPheaderParameter ---------------------- */
static int ssrvParseHTTPheaderParameter(HTTPPAR *pbuf,char *buf)
{ int i;
  if(!pbuf || !buf) return 0;
  pbuf->attrname_str = 0;
  pbuf->attrval_str  = 0;

  while(*buf == ' ' || *buf == '\t') buf++; /* Skip lead space */
  if(!*buf) return 0;
  pbuf->attrname_str = buf;
  for(i = 0;*buf && *buf != ':';i++,buf++); /* Reach end of "argument name" */
  if(!*buf) return 0;
  *buf++ = 0; 
  while(*buf == ' ' || *buf == '\t') buf++;
  if(*buf) pbuf->attrval_str = buf;

  pbuf->attrnum = ssrvGetHTTPRequestAttrNumber((const char*)pbuf->attrname_str);

  while(--i >= 0)
  { if(pbuf->attrname_str[i] != ' ' && pbuf->attrname_str[i] != '\t') break;
    pbuf->attrname_str[i] = 0;
  }
  return 1;
}
/* ---------------------- parseHttpRequestHeader ----------------------------- */
static int parseHttpRequestHeader(TCBENTRY *tcb,char *buf)
{ int i,idx;
  char *bp;

  tcb->pactsize = 0;
  idx = 0;

  /* Extract Method */
  for(bp = &buf[idx];buf[idx] && buf[idx] != ' ' && buf[idx] != '\t';idx++);
  if(!buf[idx]) return HTTP_BAD_REQUEST;
  buf[idx++] = 0;

  if((tcb->curmethod = ssrvGetHTTPMethodNumber((const char *)bp)) == HTTPM_UNKNOWN) return HTTP_BAD_REQUEST;
  while(buf[idx] == ' ' || buf[idx] == '\t') idx++;

  /* Extract URI and HTTP version */
  tcb->url = &buf[idx]; /* save URL pointer */
  while(buf[idx] && buf[idx] != '\015' && buf[idx] != '\012') idx++; /* find end of first string */
  i = (int)buf[idx];  /* save last symbol */
  bp = &buf[idx-1];
  buf[idx++] = 0;
  while(*bp && *bp != ' ' && *bp != '\t') bp--;
  if(!(*bp)) return HTTP_BAD_REQUEST;

  /* Check HTTP version string */
  if(!ssrvGetHTTPVersion(tcb,(bp+1))) return HTTP_VERSION_NOT_SUPPORTED;
  while(*bp == ' ' || *bp == '\t') *bp-- = 0;
  tcb->urlsize = strlen(tcb->url);

  if(!ssrvValidateURL(tcb)) return HTTP_BAD_REQUEST;

  if(!i) return HTTP_OK;

  while(tcb->pactsize < tcb->pbufsize && buf[idx])
  { while(buf[idx] == ' ' || buf[idx] == '\t' || buf[idx] == '\015' || buf[idx] == '\012') idx++;
    for(i = 0,bp = &buf[idx];buf[idx] && buf[idx] != '\015' && buf[idx] != '\012';idx++,i++);
    if(buf[idx]) buf[idx++] = 0;
    if(i)  
    { if(!ssrvParseHTTPheaderParameter(&tcb->pbuf[tcb->pactsize],bp)) return HTTP_BAD_REQUEST;
      tcb->pactsize++;
    }
  }
  return HTTP_OK;
}
/* ------------------------- ssrvAddRespHeader ------------------------------- */
static int ssrvAddRespHeader(TCBENTRY *tcb,const char *fieldname,char *fmt,...)
{ va_list args;
  int free;
  
  if(tcb && tcb->resphdr && tcb->rhrdsize)
  { if((free = (tcb->rhrdsize - (tcb->rhrdactsize+2))) < 0) free = 0;
    if(fieldname && (strlen(fieldname) < free))
    { tcb->rhrdactsize += sprintf(&tcb->resphdr[tcb->rhrdactsize],"%s: ",fieldname);
      if((tcb->rhrdsize > tcb->rhrdactsize) && (tcb->rhrdactsize >= 0))
      { va_start(args,fmt);
#ifndef FORK_VER
        tcb->rhrdactsize += vsnprintf(&tcb->resphdr[tcb->rhrdactsize],(tcb->rhrdsize-tcb->rhrdactsize),fmt,args);
#else
        tcb->rhrdactsize += vsprintf(&tcb->resphdr[tcb->rhrdactsize],fmt,args);
#endif
        va_end(args);
        if((tcb->rhrdsize > (tcb->rhrdactsize + 2)) && (tcb->rhrdactsize >= 0))
        { strcat(&tcb->resphdr[tcb->rhrdactsize],szCRLFString);
          tcb->rhrdactsize += 2;
        }
      }
    }
    return tcb->rhrdactsize;
  }
  return 0;
}
/* ------------------------- ssrvBeginRespHeader ----------------------------- */
static int ssrvBeginRespHeader(TCBENTRY *tcb,int retcode)
{ if(tcb && tcb->resphdr && (tcb->rhrdsize > 64))
  { tcb->rhrdactsize = sprintf(tcb->resphdr,"HTTP/%d.%d %d %s\015\012Server: %s/ver%d.%d\015\012",
                               tcb->httpver_major,tcb->httpver_minor,
                               retcode,ssrvGetReturnStatusStringByRetcode(retcode),
                               szServerNameInternal,SSRV_VMAJOR,SSRV_VMINOR);
    ssrvAddRespHeader(tcb,szParName_Connection,tcb->keep_alive ? (char*)szKeepAlive : (char*)szClose);
    if(tcb->contentLen != (-1)) ssrvAddRespHeader(tcb,szParName_Content_Length,"%d",tcb->contentLen);
    if(tcb->realm[0])           ssrvAddRespHeader(tcb,szParName_WWW_Authenticate,"Basic realm=\"%s\"",tcb->realm);
    if(tcb->location[0])        ssrvAddRespHeader(tcb,szParName_Location,tcb->location);
    if(tcb->nocacheflg)
    { if(tcb->httpver_major == 1 && tcb->httpver_minor == 0) ssrvAddRespHeader(tcb,szParName_Pragma,(char*)szNoCache);
      ssrvAddRespHeader(tcb,szParName_Cache_Control,(char*)szNoCache);
    }
    return tcb->rhrdactsize;
  }
  return 0;
}
/* ------------------------ ssrvEndRespHeader -------------------------------- */
static int ssrvEndRespHeader(TCBENTRY *tcb)
{ if(tcb && tcb->resphdr && tcb->rhrdsize)
  { if((tcb->rhrdsize > (tcb->rhrdactsize + 2)) && (tcb->rhrdactsize >= 0))
    { strcpy(&tcb->resphdr[tcb->rhrdactsize],szCRLFString);
      tcb->rhrdactsize += 2;
    }
    return tcb->rhrdactsize;
  }
  return 0;
}
/* -------------------- ssrvValidateKeepAliveStatus -------------------------- */
static int ssrvValidateKeepAliveStatus(TCBENTRY *tcb)
{ int idx = 0;
  if((tcb->httpver_major < 1) || (tcb->httpver_major == 1 && tcb->httpver_minor == 0)) tcb->keep_alive = 0;
  while(idx >= 0)
  { if((idx = ssrvFindRequestAttrNumber(tcb,HHATTRN_CONNECTION,idx)) >= 0)
    { if(tcb->pbuf[idx].attrval_str)
      { if(!strcasecmp(tcb->pbuf[idx].attrval_str,szClose)) tcb->keep_alive = 0;
        else if(!strcasecmp(tcb->pbuf[idx].attrval_str,szKeepAlive)) tcb->keep_alive++;
      }
      idx++;
    }
  }
  tcb->keep_alive = 0;
  return tcb->keep_alive;
}

/* ---------------------- ssrvValidateAuthorization -------------------------- */
static void ssrvValidateAuthorization(TCBENTRY *tcb)
{ char *c;
  int idx = 0;

  tcb->authorstr[0] = 0;
  tcb->username = (tcb->passwd = 0);

  while(idx >= 0)
  { if((idx = ssrvFindRequestAttrNumber(tcb,HHATTRN_AUTHORIZATION,idx)) >= 0)
    { if(tcb->pbuf[idx].attrval_str)
      { for(c = &tcb->pbuf[idx].attrval_str[0];(*c == ' ') || (*c == '\t');c++);
        while(*c && *c != ' ' && *c != '\t') c++;
        while(*c == ' ' || *c == '\t') c++;
        if(ssrvDecodeBase64String((const char *)c,tcb->authorstr,sizeof(tcb->authorstr)))
	{ if((c = strrchr(tcb->authorstr,':')) != 0)
	  { tcb->username = &tcb->authorstr[0]; *c++ = 0;
	    tcb->passwd = c;
	  }
	}
	else tcb->authorstr[0] = 0;
      }
      idx++;
    }
  }
}
/* ------------------- ssrvValidateRcvContentLength -------------------------- */
static void ssrvValidateRcvContentLength(TCBENTRY *tcb)
{ int idx = 0;
  tcb->rcvContentLen = (-1);
  if(tcb->curmethod == HTTPM_POST)
  { while(idx >= 0)
    { if((idx = ssrvFindRequestAttrNumber(tcb,HHATTRN_CONTENT_LENGTH,idx)) >= 0)
      { if(tcb->pbuf[idx].attrval_str)
        { if(sscanf(tcb->pbuf[idx].attrval_str,"%d",&tcb->rcvContentLen) != 1) tcb->rcvContentLen = (-1);
	}
	idx++;
      }
    }
  }
}
#ifdef SSRV_SWITCHER_ON
/* ------------------------ ssrvIsSSStagInUrl -------------------------------- */
static int ssrvIsSSStagInUrl(TCBENTRY *tcb)
{ return ((tcb->urlsize >= 5) && (tcb->url[0] == '/') && (tcb->url[1] == '$') &&
          ((tcb->url[2] == 'S') || (tcb->url[2] == 's')) &&
	  ((tcb->url[3] == 'S') || (tcb->url[3] == 's')) &&
	  ((tcb->url[4] == 'S') || (tcb->url[4] == 's'))) ? 1 : 0;
}
/* ---------------------- ssrvIsSecureSSSStagInUrl --------------------------- */
static int ssrvIsSecureSSSStagInUrl(TCBENTRY *tcb)
{ return ((tcb->urlsize >= 9) && (tcb->url[5] == '/') &&
          ((tcb->url[6] == 'R') || (tcb->url[6] == 'r')) &&
	  ((tcb->url[7] == 'G') || (tcb->url[7] == 'g')) &&
	  ((tcb->url[8] == 'S') || (tcb->url[8] == 's'))) ? 1 : 0;
}
#endif
/* ----------------------- ssrvIsProtectedResource --------------------------- */
static int ssrvIsProtectedResource(TCBENTRY *tcb) 
{ if(!ciProtectedPrefixSize) ciProtectedPrefixSize = strlen(szProtectedPrefix);
  if((tcb->urlsize >= ciProtectedPrefixSize) && !strncasecmp(tcb->url,szProtectedPrefix,ciProtectedPrefixSize))
  { if(tcb->url[ciProtectedPrefixSize] == '/' || tcb->url[ciProtectedPrefixSize] == '\\')
    { tcb->url = &tcb->url[ciProtectedPrefixSize];
      tcb->urlsize = strlen(tcb->url);
      return 1;
    }
  }
  return 0;
}
/* ------------------------ ssrvIsNoCacheResource ---------------------------- */
static int ssrvIsNoCacheResource(TCBENTRY *tcb) 
{ if(!ciNoCachePrefixSize) ciNoCachePrefixSize = strlen(szNoCachePrefix);
  if((tcb->urlsize >= ciNoCachePrefixSize) && !strncasecmp(tcb->url,szNoCachePrefix,ciNoCachePrefixSize))
  { if(tcb->url[ciNoCachePrefixSize] == '/' || tcb->url[ciNoCachePrefixSize] == '\\')
    { tcb->url = &tcb->url[ciNoCachePrefixSize];
      tcb->urlsize = strlen(tcb->url);
      return 1;
    }
  }
  return 0;
}
/* --------------------- ssrvIsInternalInfoResource -------------------------- */
static int ssrvIsInternalInfoResource(TCBENTRY *tcb) 
{ if(!ciInternalInfoPrefixSize) ciInternalInfoPrefixSize = strlen(szInternalInfoPrefix);
  return ((tcb->urlsize >= ciInternalInfoPrefixSize) && !strcmp(tcb->url,szInternalInfoPrefix)) ? 1 : 0;
}
/* ----------------------- ssrvURLNotFoundResponse --------------------------- */
static void ssrvURLNotFoundResponse(TCBENTRY *tcb,char *wbuffer,int wbuffer_size)
{
#ifndef FORK_VER
  tcb->contentLen = snprintf(wbuffer,wbuffer_size,szHTMLErrorBody_NotFound,tcb->url);
#else
  tcb->contentLen = sprintf(wbuffer,szHTMLErrorBody_NotFound,tcb->url);
#endif
  ssrvBeginRespHeader(tcb,HTTP_NOT_FOUND);  ssrvEndRespHeader(tcb);
  if(ssrvSendRespHeader(tcb)) ssrvSendBuffer(tcb,wbuffer,tcb->contentLen);
}
/* --------------------------- my_ctime_r ------------------------------------ */
static char *my_ctime_r(time_t *clock,char *buf,int size)
{
#ifndef FORK_VER
  return ctime_r(clock,buf);
#else
  return ctime_r(clock,buf,size); /* IRIX 5.3 has additional parameter - size */
#endif
}
/* ------------------------- ssrvProcessRequest ------------------------------ */
int ssrvProcessRequest(TCBENTRY *tcb)
{ struct stat statbuf;
  struct dirent *direntp;
  time_t time_val;
  char wurl[1024*4],time_buffer[64],*c;
  int i,j;
  IRCB *ircb;
  DIR *dirp;
  FILE *fp = NULL;
  int swcflg = 0;

  i = (tcb->rcvContentLen > 0) ? tcb->rcvContentLen : 0;
  i += (tcb->urlsize + (cResourcePathSize + 1) + 1);
  
  /* Check URL size */
  if(i > sizeof(wurl))
  { tcb->contentLen = sprintf(wurl,szHTMLErrorBody_ReqURITooLarge,sizeof(wurl),i);
    ssrvBeginRespHeader(tcb,HTTP_REQUEST_URI_TOO_LARGE);
    ssrvEndRespHeader(tcb);
    if(ssrvSendRespHeader(tcb)) ssrvSendBuffer(tcb,wurl,tcb->contentLen);
    return 0; /* incorrect URL size - too big */
  }

#ifdef SSRV_SWITCHER_ON
  /* Check URL - may be it is a work for "switcher"? */
  if(ssrvIsSSStagInUrl(tcb))
  { swcflg++;
    if(ssrvIsSecureSSSStagInUrl(tcb) && !ssrvIsUserValid(tcb->username,tcb->passwd)) /* Check password and username */
    { tcb->contentLen = sprintf(wurl,szHTMLErrorBody_Unauthorized);
      ssrvSetStringByTcb((hTaskControlBlock)tcb,SSRVSETS_REALMSTR,0,szSecuredArea);   /* Set "realm" name */
      ssrvBeginRespHeader(tcb,HTTP_UNAUTHORIZED);
      ssrvEndRespHeader(tcb);
      if(ssrvSendRespHeader(tcb)) ssrvSendBuffer(tcb,wurl,tcb->contentLen);
      return 0;
    }
    strcpy(wurl,tcb->url);
  }
  else /* not /$sss prefix */
#endif
  { if(ssrvIsProtectedResource(tcb)) /* /$protected/ */
    { if(!ssrvIsUserValid(tcb->username,tcb->passwd))
      { tcb->contentLen = sprintf(wurl,szHTMLErrorBody_Unauthorized);
        ssrvSetStringByTcb((hTaskControlBlock)tcb,SSRVSETS_REALMSTR,0,szSecuredArea);   /* Set "realm" name */
        ssrvBeginRespHeader(tcb,HTTP_UNAUTHORIZED);
        ssrvEndRespHeader(tcb);
        if(ssrvSendRespHeader(tcb)) ssrvSendBuffer(tcb,wurl,tcb->contentLen);
        return 0;
      }
    }
    tcb->nocacheflg = ssrvIsNoCacheResource(tcb) ? 1 : 0;
    strcpy(wurl,szResourcePath);
    if(tcb->url[0] != '/') strcat(wurl,szSlash);
    if(!strcmp(tcb->url,szSlash))
    { strcat(wurl,szSlash);
      if(!dir_listing_flg) strcat(wurl,szDefaultHtmlFileName);
    }
    else strcat(wurl,tcb->url);
#ifdef SSRV_SWITCHER_ON
    if(ssrvIsHtmlResource(wurl)) swcflg++;
#endif
  }

#ifdef SSRV_SWITCHER_ON
  if(swcflg)
  { j = 0;
    if(tcb->reqbodyptr)
    { if((c = strrchr(wurl,'?')) != 0) strcat(wurl,tcb->reqbodyptr);
      else { strcat(wurl,"?"); strcat(wurl,tcb->reqbodyptr); }
    }
    if(((i = ssrvFindRequestAttrNumber(tcb,HHATTRN_USER_AGENT,0)) >= 0) && (tcb->pbuf[i].attrval_str))
    { if((c = strrchr(wurl,'?')) != 0) { if(*(c+1)) strcat(wurl,"&"); }
      else strcat(wurl,"?");
      strcat(wurl,szParName_User_Agent); strcat(wurl,"=");
      if((c = strchr(tcb->pbuf[i].attrval_str,'/')) != 0)
      { *c = 0; strcat(wurl,tcb->pbuf[i].attrval_str); *c = '/';
      }
      else strcat(wurl,tcb->pbuf[i].attrval_str);
      j = strlen(wurl);
    }
    if(tcb->caddr_str[0])
    { if(!j)
      { if((c = strrchr(wurl,'?')) != 0) { if(*(c+1)) strcat(wurl,"&"); }
        else strcat(wurl,"?");
	j = strlen(wurl);
      }
      else { strcat(wurl,"&"); j++; }
      if((sizeof(wurl)-j) > 32)
#ifndef FORK_VER
       snprintf(&wurl[j],sizeof(wurl)-j,"%s=%s",szParName_User_IPaddr,tcb->caddr_str);
#else
       sprintf(&wurl[j],"%s=%s",szParName_User_IPaddr,tcb->caddr_str);
#endif
    }
  }
#endif  
  tcb->wurl = wurl;

#ifdef SSRV_SWITCHER_ON
  /* Check special URL (all HTML files and all URL with "/$SSS" prefix) */
  if(swcflg)
  { char tmperrmsg[256];
    if(!ssrvLoadSwitcherModule(tmperrmsg,sizeof(tmperrmsg))) /* Load "switcher" module */
    { tcb->contentLen = sprintf(wurl,szHTMLErrorBody_InternalErr_CantLoadSwc,szSwitcherLibPath,tmperrmsg);
      ssrvBeginRespHeader(tcb,HTTP_INTERNAL_SERVER_ERROR); ssrvEndRespHeader(tcb);
      if(ssrvSendRespHeader(tcb)) ssrvSendBuffer(tcb,wurl,tcb->contentLen);
      return 0;
    }
    else
    { swexf.fp_swcBeginProc((hTaskControlBlock)tcb);
      ssrvBeginRespHeader(tcb,tcb->retcode);
      if(tcb->contentLen) ssrvAddRespHeader(tcb,szParName_Content_Type,"%s",tcb->mimetype[0] ? tcb->mimetype : szMimeText_Html);
      ssrvEndRespHeader(tcb);
      if(ssrvSendRespHeader(tcb))
      { do
        { if(tcb->wbufactsize)
	  { if(!ssrvSendBuffer(tcb,tcb->wbuf,tcb->wbufactsize)) break;
	    tcb->wbufactsize = 0;
	  }
	} while(swexf.fp_swcContProc((hTaskControlBlock)tcb));
      }
      swexf.fp_swcEndProc((hTaskControlBlock)tcb);
      return 1;
    }
  }
#endif

  if(tcb->curmethod != HTTPM_GET && tcb->curmethod != HTTPM_HEAD)
  { tcb->contentLen = sprintf(wurl,szHTMLErrorBody_MethodNotAllowed);
    ssrvBeginRespHeader(tcb,HTTP_METHOD_NOT_ALLOWED);
    ssrvAddRespHeader(tcb,szParName_Allow,"GET, HEAD");
    ssrvEndRespHeader(tcb);
    if(ssrvSendRespHeader(tcb)) ssrvSendBuffer(tcb,wurl,tcb->contentLen);
    return 0;
  }

  /* Check internal resources */
  if((ircb = sscIsInternalServerResource(tcb->url)) != 0)
  { tcb->contentLen = ircb->size;
    ssrvBeginRespHeader(tcb,HTTP_OK);
    if((c = (char*)ircb->mime_type) == 0) c = (char*)ssrvFindContentTypeByFname(tcb->url);
    ssrvAddRespHeader(tcb,szParName_Content_Type,"%s",c ? c : szMimeText_Html);
    ssrvEndRespHeader(tcb);
    if(!ssrvSendRespHeader(tcb)) return 0;
    return (ssrvSendBuffer(tcb,ircb->buffer,ircb->size) == ircb->size);
  }
  
  /* Check internal server info request */
  if(ssrvIsInternalInfoResource(tcb))
  { if((tcb->contentLen = ssrvMakeReportBuffer(wurl,sizeof(wurl)-1)) > 0)
    { ssrvBeginRespHeader(tcb,HTTP_OK);
      ssrvAddRespHeader(tcb,szParName_Content_Type,"%s",szMimeText_Plain);
      ssrvEndRespHeader(tcb);
      if(!ssrvSendRespHeader(tcb)) return 0;
      return (ssrvSendBuffer(tcb,wurl,tcb->contentLen) == tcb->contentLen);
    }
    ssrvURLNotFoundResponse(tcb,wurl,sizeof(wurl));    
    return 0;
  }
  
  
  /* Check file existance */
  if(stat(wurl,&statbuf) || (!S_ISREG(statbuf.st_mode) && !S_ISDIR(statbuf.st_mode)))
  { ssrvURLNotFoundResponse(tcb,wurl,sizeof(wurl));
    return 0;
  }

  /* Check directory listing mode */  
  if(dir_listing_flg && S_ISDIR(statbuf.st_mode))
  { if((dirp = opendir(wurl)) == NULL)
    { ssrvURLNotFoundResponse(tcb,wurl,sizeof(wurl));
      return 0;
    }
    if((i = strlen(wurl)) != 0)
    { if((wurl[i-1] != '/') && (wurl[i-1] != '\\') && ((i+1) < sizeof(wurl))) { strcat(wurl,szSlash); i++; }
    }

    ssrvSetIntegerByTcb((hTaskControlBlock)tcb,SSRVSETI_NOCACHEFLG,0,1);
    ssrvBeginRespHeader(tcb,HTTP_OK);
    ssrvAddRespHeader(tcb,szParName_Content_Type,"%s",szMimeText_Html);
    ssrvEndRespHeader(tcb);
    if(!ssrvSendRespHeader(tcb)) { closedir(dirp); return 0; }
#ifndef FORK_VER
    j = snprintf(tcb->wbuf,tcb->wbufsize,szHTMLDirectoryListing_Header,tcb->url);
#else
    j = sprintf(tcb->wbuf,szHTMLDirectoryListing_Header,tcb->url);
#endif
    if(ssrvSendBuffer(tcb,tcb->wbuf,j) != j) { closedir(dirp); return 0; }

    if((j = strlen(tcb->url)) > 0 && (tcb->url[j-1] == '/' || tcb->url[j-1] == '\\')) tcb->url[j-1] = 0;

    /* Folders selection */
    while((direntp = readdir(dirp)) != NULL)
    { if(!strcmp(direntp->d_name,".") || ((strlen(direntp->d_name)+i) >= sizeof(wurl))) continue;
      strcpy(&wurl[i],direntp->d_name);
      if(!stat(wurl,&statbuf) && S_ISDIR(statbuf.st_mode))
      { time_val = (time_t)statbuf.st_mtim.tv_sec;
        if(!strcmp(direntp->d_name,".."))
#ifndef FORK_VER
         j = snprintf(tcb->wbuf,tcb->wbufsize,szHTMLDirectoryListing_BodyDir,szInternalResNameBackGif, 
#else
         j = sprintf(tcb->wbuf,szHTMLDirectoryListing_BodyDir,szInternalResNameBackGif, 
#endif
					     direntp->d_name,tcb->url,"../",
					     direntp->d_name,my_ctime_r(&time_val,time_buffer,sizeof(time_buffer)));
        else
#ifndef FORK_VER
         j = snprintf(tcb->wbuf,tcb->wbufsize,szHTMLDirectoryListing_BodyDir,szInternalResNameFolderGif, 
#else
         j = sprintf(tcb->wbuf,szHTMLDirectoryListing_BodyDir,szInternalResNameFolderGif, 
#endif
		     direntp->d_name,tcb->url,direntp->d_name,
		     direntp->d_name,my_ctime_r(&time_val,time_buffer,sizeof(time_buffer)));
      }
      else j = 0;
      if(j) { if(ssrvSendBuffer(tcb,tcb->wbuf,j) != j) break; }
    }

    rewinddir(dirp);

    while((direntp = readdir(dirp)) != NULL)
    { if(!strcmp(direntp->d_name,".") || ((strlen(direntp->d_name)+i) >= sizeof(wurl))) continue;
      strcpy(&wurl[i],direntp->d_name);
      if(!stat(wurl,&statbuf) && S_ISREG(statbuf.st_mode))
      { time_val = (time_t)statbuf.st_mtim.tv_sec;
#ifndef FORK_VER
        j = snprintf(tcb->wbuf,tcb->wbufsize,szHTMLDirectoryListing_BodyFile,szInternalResNameFileGif,
#else
        j = sprintf(tcb->wbuf,szHTMLDirectoryListing_BodyFile,szInternalResNameFileGif,
#endif
		     direntp->d_name,tcb->url,direntp->d_name,
		     direntp->d_name,(unsigned long)statbuf.st_size,my_ctime_r(&time_val,time_buffer,sizeof(time_buffer)));
      }
      else j = 0;
      if(j) { if(ssrvSendBuffer(tcb,tcb->wbuf,j) != j) break; }
    }
    closedir(dirp);

    j = strlen(szHTMLDirectoryListing_End);
    return ssrvSendBuffer(tcb,(void *)szHTMLDirectoryListing_End,j) == j;
  }

  if(!S_ISREG(statbuf.st_mode) || (fp = fopen(wurl,"rb")) == NULL)
  { ssrvURLNotFoundResponse(tcb,wurl,sizeof(wurl));
    return 0;
  }

  tcb->contentLen = (int)statbuf.st_size;
  ssrvBeginRespHeader(tcb,HTTP_OK);
  if((c = (char*)ssrvFindContentTypeByFname(tcb->url)) != 0) ssrvAddRespHeader(tcb,szParName_Content_Type,"%s",c);
  ssrvEndRespHeader(tcb);
  if(!ssrvSendRespHeader(tcb)) { fclose(fp); return 0; }

  if(tcb->curmethod == HTTPM_GET)
  { for(i = tcb->contentLen;i;)
    { if((j = tcb->wbufsize) > i) j = i;
      if(fread(tcb->wbuf,1,j,fp) != j) break;
      if(ssrvSendBuffer(tcb,tcb->wbuf,j) != j) break;
      i -= j;
    }
  }
  else i = 0;
  fclose(fp);
  return i == 0;  
}

/* ------------------------- ssrvConnectionProcessor ------------------------- */
void ssrvConnectionProcessor(TCBENTRY *tcb)  
{ int i,j,idx,hdrsize,bodysize;
  HTTPPAR pbuf[SSRV_MAXHTTPREQ_PAIR];
  char buf[1024*6],resphdr[1024*2], workbuf[1024*8];
  char *c = 0;

  for(tcb->keep_alive = 0;!exit_phase;)
  { tcb->pbuf        = pbuf;
    tcb->pbufsize    = SSRV_MAXHTTPREQ_PAIR;
    tcb->pactsize    = 0;
    tcb->resphdr     = resphdr;
    tcb->rhrdsize    = sizeof(resphdr);
    tcb->rhrdactsize = 0;
    tcb->wbuf        = workbuf;
    tcb->wbufsize    = sizeof(workbuf);
    tcb->wbufactsize = 0;
    tcb->retcode     = HTTP_INTERNAL_SERVER_ERROR;
    tcb->contentLen  = (-1);
    tcb->reqbodyptr  = 0;
    
    buf[0] = (resphdr[0] = (workbuf[0] = 0));
    
    /* Get HTTP Header (and may be part of request body) */
    for(c = 0,idx = 0;idx < (sizeof(buf)-1) && !exit_phase;)
    { if((i = ssrvRecvSocket(tcb,&buf[idx],sizeof(buf)-idx)) < 0) break;
      if((idx = (idx + i)) >= sizeof(buf)) break;
      buf[idx] = 0;
      if((c = strstr(buf,"\015\012\015\012")) != 0) { *c = 0; break; }
      if(!i) break;
    }
    /* Check the "end of header" marker */
    if(!c) { est.statConTotalRcvError++; break; }

    /* calculate header size and current request body size */
    hdrsize = strlen(buf) + 4; /* 4 == "\015\012\015\012" */
    if((bodysize = (idx - hdrsize)) < 0) bodysize = 0;

#ifdef INCDEBUG
    dprintf("^^^^^Header Size: %d  Rcv Body Size: %d ^^^^^^^\n\"%s\"\n",hdrsize,bodysize,buf);
#endif
    
    /* Parse HTTP request header */
    if((j = parseHttpRequestHeader(tcb,buf)) != HTTP_OK)
    { est.statConTotalParseHeaderError++;
#ifndef FORK_VER
      tcb->contentLen = snprintf(buf,sizeof(buf),szHTMLErrorBody_szCantParseHttpHeader,j,j);
#else
      tcb->contentLen = sprintf(buf,szHTMLErrorBody_szCantParseHttpHeader,j,j);
#endif
      ssrvBeginRespHeader(tcb,j); ssrvEndRespHeader(tcb);
      if(ssrvSendRespHeader(tcb)) ssrvSendBuffer(tcb,buf,tcb->contentLen);
      break;
    }
    ssrvValidateKeepAliveStatus(tcb);
    ssrvValidateAuthorization(tcb);
    ssrvValidateRcvContentLength(tcb);

    /* Check "Content-Length" for "POST" method */
    if(tcb->curmethod == HTTPM_POST && tcb->rcvContentLen < 0)
    { est.statConTotalInvalidPostBodyLen++;
#ifndef FORK_VER
      tcb->contentLen = snprintf(buf,sizeof(buf),szHTMLErrorBody_ContentLengthRequired);
#else
      tcb->contentLen = sprintf(buf,szHTMLErrorBody_ContentLengthRequired);
#endif
      ssrvBeginRespHeader(tcb,HTTP_LENGTH_REQUIRED); ssrvEndRespHeader(tcb);
      if(ssrvSendRespHeader(tcb)) ssrvSendBuffer(tcb,buf,tcb->contentLen);
      break;      
    }

    /* Check request body size */
    if(tcb->rcvContentLen > 0 && tcb->rcvContentLen > (sizeof(buf)-(idx-bodysize)))
    { est.statConTotalRcvContentLenTooBig++;
#ifndef FORK_VER
      tcb->contentLen = snprintf(buf,sizeof(buf),szHTMLErrorBody_InternalErr_PostRequestBodySizeTooBig);
#else
      tcb->contentLen = sprintf(buf,szHTMLErrorBody_InternalErr_PostRequestBodySizeTooBig);
#endif
      ssrvBeginRespHeader(tcb,HTTP_INTERNAL_SERVER_ERROR); ssrvEndRespHeader(tcb);
      if(ssrvSendRespHeader(tcb)) ssrvSendBuffer(tcb,buf,tcb->contentLen);
      break;      
    }    
    
    /* Try read request body */
    if(tcb->rcvContentLen > 0)
    { /* Get HTTP reques body */
      while((tcb->rcvContentLen > bodysize) && (idx < (sizeof(buf)-1)) && !exit_phase)
      { if((i = ssrvRecvSocket(tcb,&buf[idx],sizeof(buf)-idx)) < 0) break;
        idx += i; buf[idx] = 0;
        bodysize += i;
	if(!i) break;
      }
      if(tcb->rcvContentLen > bodysize)
      { est.statConTotalRcvContentLenIsBad++;
#ifndef FORK_VER
        tcb->contentLen = snprintf(buf,sizeof(buf),szHTMLErrorBody_InternalErr_PostRequestBodySizeIsBad,tcb->rcvContentLen,bodysize);
#else
        tcb->contentLen = sprintf(buf,szHTMLErrorBody_InternalErr_PostRequestBodySizeIsBad,tcb->rcvContentLen,bodysize);
#endif
        ssrvBeginRespHeader(tcb,HTTP_INTERNAL_SERVER_ERROR); ssrvEndRespHeader(tcb);
        if(ssrvSendRespHeader(tcb)) ssrvSendBuffer(tcb,buf,tcb->contentLen);
        break;      
      }
      tcb->reqbodyptr = &buf[hdrsize];
      tcb->reqbodyptr[tcb->rcvContentLen] = 0;
    }
    if(!ssrvProcessRequest(tcb)) break;
    if(!tcb->keep_alive) break;
  } /* end of for(;;) */
}

/* --------------------------- ssrvGetIntegerByTcb --------------------------- */
int ssrvGetIntegerByTcb(hTaskControlBlock hTcb, int valueIdx, int valueIdx2)
{ TCBENTRY *tcb = (TCBENTRY*)hTcb;
  if(!tcb || (tcb->struct_size != sizeof(TCBENTRY))) tcb = 0;
  if(tcb)
  { switch(valueIdx) {
    case SSRVGETI_HTTPMETHOD      : return tcb->curmethod;
    case SSRVGETI_URLSIZE         : return tcb->urlsize;
    case SSRVGETI_HTTPVMAJOR      : return tcb->httpver_major;
    case SSRVGETI_HTTPVMINOR      : return tcb->httpver_minor;
    case SSRVGETI_KEEPALIVE       : return tcb->keep_alive;
    case SSRVGETI_USERARGMAX      : return SSRV_TCBMAX_USERARGS;
    case SSRVGETI_MAXREALMSIZE    : return SSRV_TCBMAX_REALMSIZE;
    case SSRVGETI_MAXWORKBUFSIZE  : return tcb->wbufsize;
    case SSRVGETI_ACTWORKBUFSIZE  : return tcb->wbufactsize;
    case SSRVGETI_CONTENTLENGTH   : return tcb->contentLen;
    };
    return 0;
  }
  switch(valueIdx) {
  case SSRVGETI_MAXMIMETYPESIZE : return SSRV_TCBMAX_MIMETSIZE;
  case SSRVGETI_WEBSRVVMAJOR    : return SSRV_VMAJOR;
  case SSRVGETI_WEBSRVVMINOR    : return SSRV_VMINOR;
  case SSRVGETI_WEBSRVPORT      : return listen_port;
  case SSRVGETI_USERNAMEKEY     : return SSRV_UNAME_KEY_VALUE;
  case SSRVGETI_PASSWORDKEY     : return SSRV_PASSWD_KEY_VALUE;
  };
  return 0;
}
/* ---------------------------- ssrvGetStringByTcb --------------------------- */
const char *ssrvGetStringByTcb(hTaskControlBlock hTcb,int valueIdx, int valueIdx2)
{ TCBENTRY *tcb = (TCBENTRY*)hTcb;
  if(!tcb || (tcb->struct_size != sizeof(TCBENTRY))) tcb = 0;
  if(tcb)
  { switch(valueIdx) {
    case SSRVGETS_USERARGS     : return ((valueIdx2 >= 0) && (valueIdx2 < SSRV_TCBMAX_USERARGS)) ?
                                         (const char *)(tcb->userArg[valueIdx2]) : 0;
    case SSRVGETS_URL          : return (const char *)tcb->wurl;
    case SSRVGETS_CLIENTIPADDR : return (const char *)tcb->caddr_str;
    case SSRVGETS_USERNAME     : return (const char *)tcb->username;
    case SSRVGETS_PASSWD       : return (const char *)tcb->passwd;
    case SSRVGETS_WORKBUFPTR   : return (const char *)tcb->wbuf;
    case SSRVGETS_BODYPARAMS   : return (const char *)tcb->reqbodyptr;
    };
    return 0;
  }
  switch(valueIdx) {  
  case SSRVGETS_SERVERNAME   : return szDaemonName;
  case SSRVGETS_SRVLEGALNAME : return szLegalName;
  case SSRVGETS_SRVVERTIME   : return szServerVersionDate;
  case SSRVGETS_SRVRESPATH   : return szResourcePath;
  case SSRVGETS_SRVISOCKNAME : return szSSServerInfoSockPath;
  };
  return 0;
} 
/* --------------------------- ssrvSetIntegerByTcb --------------------------- */
int ssrvSetIntegerByTcb(hTaskControlBlock hTcb,int valueIdx, int valueIdx2, int value)
{ TCBENTRY *tcb = (TCBENTRY*)hTcb;
  if(!tcb || (tcb->struct_size != sizeof(TCBENTRY))) return 0;
  switch(valueIdx) {
  case SSRVSETI_HTTPRETCODE    : tcb->retcode = value;
                                 return 1;
  case SSRVSETI_ACTWORKBUFSIZE : tcb->wbufactsize = value;
                                 return 1;
  case SSRVSETI_CONTENTLENGTH  : tcb->contentLen = value;
                                 return 1;
  case SSRVSETI_NOCACHEFLG     : tcb->nocacheflg = value;
                                 return 1;
  };
  return 0;
} 
/* ---------------------------- ssrvSetStringByTcb --------------------------- */
int ssrvSetStringByTcb(hTaskControlBlock hTcb,int valueIdx,int valueIdx2,const char *value)
{ TCBENTRY *tcb = (TCBENTRY*)hTcb;
  if(!tcb || (tcb->struct_size != sizeof(TCBENTRY))) return 0;
  switch(valueIdx) {
  case SSRVSETS_USERARGS      : if((valueIdx2 >= 0) && (valueIdx2 < SSRV_TCBMAX_USERARGS))
                                { tcb->userArg[valueIdx2] = (void*)value;
				  return 1;
				}
				break;
  case SSRVSETS_REALMSTR      : if(!value) { tcb->realm[0] = 0; return 1; }
				if(strlen(value) < SSRV_TCBMAX_REALMSIZE)
                                { strcpy(tcb->realm,value);
				  return 1;
				}
				break;
  case SSRVSETS_MIMETYPE      : if(!value) { tcb->mimetype[0] = 0; return 1; }
				if(strlen(value) < SSRV_TCBMAX_MIMETSIZE)
                                { strcpy(tcb->mimetype,value);
				  return 1;
				}
				break;
  case SSRVSETS_LOCATION      : if(!value) { tcb->location[0] = 0; return 1; }
				if(strlen(value) < SSRV_TCBMAX_LOCATION)
                                { strcpy(tcb->location,value);
				  return 1;
				}
				break;

  };
  return 0;
}
  
