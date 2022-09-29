/* ----------------------------------------------------------------- */
/*                           SSDBOSDEP.H                             */
/*                        SSDB API OS Dependence                     */
/* ----------------------------------------------------------------- */
#ifndef H_SSDBOSDEP_H
#define H_SSDBOSDEP_H

#ifdef OS_WINDOWS
#define ssdb_vsnprintf _vsnprintf
#else
#define ssdb_vsnprintf vsnprintf
#endif

#endif
