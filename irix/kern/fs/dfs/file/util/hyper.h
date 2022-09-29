/* Copyright (C) 1996 Transarc Corporation - All rights reserved. */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/util/RCS/hyper.h,v 65.2 1998/03/19 23:47:26 bdr Exp $ */

#ifndef TRANSARC_UTIL_HYPER_H
#define TRANSARC_UTIL_HYPER_H

#ifdef SGIMIPS
int dfsh_StrToHyper(char *numString, afs_hyper_t *hyperP, char **cp);
#else  /* SGIMIPS */
int dfsh_StrToHyper(const char *numString, afs_hyper_t *hyperP, char **cp);
#endif /* SGIMIPOS */
char *dfsh_HyperToStr(afs_hyper_t *h, char *s);

/* When you absolutely have to have the bits of a hyper laid out they way they
 * used to be, use a dfsh_diskHyper_t.  Used to represent a hyper on disk or
 * tape.  It only needs to be 32-bit aligned and isn't affected by the
 * alignment requirements of the 64-bit scalar type. */

typedef struct {
    u_int32 dh_high;
    u_int32 dh_low;
} dfsh_diskHyper_t;

/* To convert back and forth two sets of to/from hyper macros are provided, the
 * first set preserves host order, this is used by Episode.  The second set
 * uses ntohl/htonl on the halves and is suitable when architecture neutrality
 * is needed: ubik databases and tapes. */

#define DFSH_MemFromDiskHyper(h,dh) AFS_hset64(h, (dh).dh_high, (dh).dh_low)
#define DFSH_DiskFromMemHyper(dh,h) \
    ((dh).dh_high = AFS_hgethi(h), (dh).dh_low = AFS_hgetlo(h))

#define DFSH_MemFromNetHyper(h,dh) \
    AFS_hset64(h, ntohl((dh).dh_high), ntohl((dh).dh_low))
#define DFSH_NetFromMemHyper(dh,h) \
    ((dh).dh_high = htonl(AFS_hgethi(h)), (dh).dh_low = htonl(AFS_hgetlo(h)))

#endif
