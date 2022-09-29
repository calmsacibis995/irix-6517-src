/* --------------------------------------------------------------------------- */
/* -                             SSRV_MIME.C                                 - */
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
#include <string.h>

/* --------------------------------------------------------------------------- */
/* String pair structure                                                       */
typedef struct s_sss_pair_string {
 const char *name;
 const char *val;
} PSTRING;
/* --------------------------------------------------------------------------- */
const char szMimeText_Plain[]      = "text/plain";
const char szMimeText_Html[]       = "text/html";
static const char szMimeText_X_Sgml[]     = "text/x-sgml";
static const char szMimeImage_Gif[]       = "image/gif";
static const char szMimeImage_Jpeg[]      = "image/jpeg";
static const char szMimeImage_Tiff[]      = "image/tiff";
static const char szMimeApp_OctetStr[]    = "application/octet-stream";
static const char szMimeApp_PostScript[]  = "application/postscript";
static const char szMimeApp_XDirector[]   = "application/x-director";
static const char szMimeApp_XTexinfo[]    = "application/x-texinfo";
static const char szMimeApp_XKoan[]       = "application/x-koan";
static const char szMimeApp_XTroff[]      = "application/x-troff";
static const char szMimeApp_XJavaScrpt[]  = "application/x-javascript";
static const char szMimeApp_XDosMsExcel[] = "application/x-dos_ms_excel";
static const char szMimeAudio_Basic[]     = "audio/basic";
static const char szMimeAudio_Midi[]      = "audio/midi";
static const char szMimeAudio_Mpeg[]      = "audio/mpeg";
static const char szMimeAudio_XAiff[]     = "audio/x-aiff";
static const char szMimeAudio_XPnRaud[]   = "audio/x-pn-realaudio";
static const char szMimeVideo_Quickt[]    = "video/quicktime";
static const char szMimeVideo_Mpeg[]      = "video/mpeg";
static const char szMimeXWorld_XVrml[]    = "x-world/x-vrml";
/* --------------------------------------------------------------------------- */
static const char szExtHtml[]        = "html";
static const char szExtHtm[]         = "htm";
/* --------------------------------------------------------------------------- */
/* Mime table                                                                  */
static PSTRING mimeTable[] = {
 { szMimeText_Plain,                   "txt" },       /* text/plain */
 { szMimeText_Plain,                   "h" },       /* text/plain */
 { szMimeText_Html,                    szExtHtml },   /* text/html */
 { szMimeText_Html,                    szExtHtm },    /* text/html */
 { szMimeImage_Gif,                    "gif" },       /* image/gif */
 { szMimeImage_Jpeg,                   "jpg" },       /* image/jpeg */
 { szMimeImage_Jpeg,                   "jpeg" },      /* image/jpeg */
 { szMimeImage_Jpeg,                   "jpe" },       /* image/jpeg */
 { szMimeApp_OctetStr,                 "bin" },       /* application/octet-stream */
 { szMimeApp_OctetStr,                 "dms" },       /* application/octet-stream */
 { szMimeApp_OctetStr,                 "lha" },       /* application/octet-stream */
 { szMimeApp_OctetStr,                 "lzh" },       /* application/octet-stream */
 { szMimeApp_OctetStr,                 "exe" },       /* application/octet-stream */
 { szMimeApp_OctetStr,                 "class" },     /* application/octet-stream */
 { "image/x-xbitmap",                  "xbm" },
 { "image/ief",                        "ief" },
 { "image/x-png",                      "png" },
 { "image/x-portable-pixmap",          "ppm" },
 { "image/x-xwindowdump",              "xwd" },
 { szMimeImage_Tiff,                   "tiff" },      /* "image/tiff" */
 { szMimeImage_Tiff,                   "tif" },       /* "image/tiff" */
 { "image/x-xpixmap",                  "xpm" },
 { "image/x-portable-anymap",          "pnm" },
 { "image/x-portable-bitmap",          "pbm" },
 { "image/x-portable-graymap",         "pgm" },
 { "image/x-rgb",                      "rgb" },
 { "image/x-MS-bmp",                   "bmp" },
 { "image/x-photo-cd",                 "pcd" },
 { "application/fractals",             "fif" },
 { "video/x-msvideo",                  "avi" },
 { szMimeVideo_Quickt,                 "mov" },       /* "video/quicktime" */
 { szMimeVideo_Quickt,                 "qt" },        /* "video/quicktime" */
 { szMimeVideo_Mpeg,                   "mpeg" },      /* "video/mpeg" */
 { szMimeVideo_Mpeg,                   "mpg" },       /* "video/mpeg" */
 { szMimeVideo_Mpeg,                   "mpe" },       /* "video/mpeg" */
 { "video/x-mpeg2",                    "mpv2" },
 { "video/x-sgi-movie",                "movie" },
 { "audio/x-wav",                      "wav" },
 { szMimeAudio_XAiff,                  "aif" },
 { szMimeAudio_XAiff,                  "aiff" },
 { szMimeAudio_XAiff,                  "aifc" },
 { szMimeAudio_Basic,                  "au" },
 { szMimeAudio_Basic,                  "snd" },
 { szMimeAudio_Midi,                   "mid" },
 { szMimeAudio_Midi,                   "midi" },
 { szMimeAudio_Midi,                   "kar" },
 { szMimeAudio_Mpeg,                   "mpga" },
 { szMimeAudio_Mpeg,                   "mp2" },
 { szMimeAudio_XPnRaud,                "ra" },        /* "audio/x-pn-realaudio" */
 { szMimeAudio_XPnRaud,                "ram" },       /* "audio/x-pn-realaudio" */
 { "audio/x-mpeg",                     "mpa" },
 { szMimeXWorld_XVrml,                 "wrl" },
 { szMimeXWorld_XVrml,                 "vrml" },
 { "application/pdf",                  "pdf" },
 { "application/rtf",                  "rtf" },
 { "application/x-tex",                "tex" },
 { "application/x-latex",              "latex" },
 { "application/dvi",                  "dvi" },
 { szMimeApp_XTexinfo,                 "texi" },
 { szMimeApp_XTexinfo,                 "texinfo" },
 { "application/msword",               "doc" },
 { "image/x-cmu-raster",               "ras" },
 { szMimeText_X_Sgml,                  "sgml" },
 { szMimeText_X_Sgml,                  "sgm" },
 { "application/mac-binhex40",         "hqx" },
 { "application/mac-compactpro",       "cpt" },
 { "application/oda",                  "oda" },
 { szMimeApp_PostScript,               "ai" },         /* "application/postscript" */
 { szMimeApp_PostScript,               "eps" },        /* "application/postscript" */
 { szMimeApp_PostScript,               "ps" },         /* "application/postscript" */
 { "application/powerpoint",           "ppt" },
 { "application/x-bcpio",              "bcpio" },
 { "application/x-cdlink",             "vcd" }, 
 { "application/x-cpio",               "cpio" },
 { "application/x-csh",                "csh" },
 { szMimeApp_XDirector,                "dcr" },
 { szMimeApp_XDirector,                "dir" },
 { szMimeApp_XDirector,                "dxr" },
 { "application/x-dvi",                "dvi" },
 { "application/x-gtar",               "gtar" },
 { "application/x-hdf",                "hdf" },
 { "application/x-mif",                "mif" },
 { "application/x-sh",                 "sh" },
 { "application/x-shar",               "shar" },
 { "application/x-stuffit",            "sit" },
 { "application/x-sv4cpio",            "sv4cpio" },
 { "application/x-tar",                "tar" },
 { "application/x-tcl",                "tcl" },
 { "application/x-troff-man",          "man" },
 { "application/x-troff-me",           "me" },
 { "application/x-troff-ms",           "ms" },
 { "application/x-ustar",              "ustar" },
 { "application/x-wais-source",        "src" },
 { "application/zip",                  "zip" },
 { szMimeApp_XKoan,                    "skp" },
 { szMimeApp_XKoan,                    "skd" },
 { szMimeApp_XKoan,                    "skt" },
 { szMimeApp_XKoan,                    "skm" },
 { szMimeApp_XTroff,                   "t" },
 { szMimeApp_XTroff,                   "tr" },
 { szMimeApp_XTroff,                   "roff" },
 { "text/richtext",                    "rtx" },
 { "text/x-setext",                    "etx" },
 { "text/tab-separated-values",        "tsv" },
 { "application/x-sv4crc",             "sv4crc" },
 { "application/x-sgi-lpr",            "sgi-lpr" },
 { szMimeApp_XJavaScrpt,               "js" },        /* application/x-javascript */
 { szMimeApp_XJavaScrpt,               "ls" },        /* application/x-javascript */
 { szMimeApp_XJavaScrpt,               "mocha" },     /* application/x-javascript */
 { "application/x-install",            "inst" },      /* application/x-install */
 { "application/x-tardist",            "tardist" },   /* application/x-tardist */
 { "application/x-dos_ms_word",        "msw" },       /* application/x-dos_ms_word */
 { szMimeApp_XDosMsExcel,               "xl" },       /* application/x-dos_ms_excel */
 { szMimeApp_XDosMsExcel,              "xls" },       /* application/x-dos_ms_excel */
 { "application/x-dos_ms_powerpoint2", "ppt" },       /* application/x-dos_ms_powerpoint2 */
 { "application/x-dos_ms_msaccess",    "adb" },       /* application/x-dos_ms_msaccess */
 { "application/x-dos_claris_fm",      "cfm" },       /* application/x-dos_claris_fm */
 { 0,                            0 } };
/* --------------------- ssrvFindContentTypeByExt ---------------------------- */
const char *ssrvFindContentTypeByExt(const char *ext)
{ int i;
  if(ext)
  { while(*ext == '.') ext++;
    if(*ext)
    { for(i = 0;mimeTable[i].name;i++)
       if(!strcasecmp(mimeTable[i].val,ext)) return mimeTable[i].name;
    }
  }
  return szMimeApp_OctetStr;
}
/* ------------------- ssrvFindContentTypeByFname ---------------------------- */
const char *ssrvFindContentTypeByFname(const char *fname)
{ char *c;
  if(fname && fname[0])
  { if((c = strrchr(fname,(int)'.')) != 0) return ssrvFindContentTypeByExt((const char*)c);
  }
  return 0;
}
/* --------------------- ssrvIsHtmlResource ---------------------------------- */
int ssrvIsHtmlResource(const char *fname)
{ char *c,*q,tmp[16];
  int i,retcode = 0;
  if(fname && fname[0])
  { if((q = strchr(fname,(int)'?')) != 0) *q = 0;
    if((c = strrchr(fname,(int)'.')) != 0)
    { if(*(++c))
      { for(i = 0;c[i] && c[i] != ' ' && c[i] != '\t' && i < sizeof(tmp)-1;i++) tmp[i] = c[i];
	tmp[i] = 0;
        if(!strcasecmp(tmp,szExtHtml) || !strcasecmp(tmp,szExtHtm)) retcode++;
      }
    }
    if(q) *q = '?';
  }
  return retcode;
}
