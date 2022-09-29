#ifndef __COMMON_H__
#define __COMMON_H__

/*
** common.h
**
** I really should pay attention to what's needed where, but for now, this
** seems to be a common set of includes (based off XFS and EFS stuff).
**
** Certainly need to clean this up later.
**
*/





#include <arcs/io.h>
#include <arcs/dirent.h>
#include <arcs/pvector.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/sbd.h>
#include <sys/dkio.h>
#include <sys/dvh.h>

#include <filehdr.h>
#include <elf.h>
#include <saio.h>
#include <arcs/fs.h>
#include <ctype.h>
#include <values.h>
#include <libsc.h>
#include <libsc_internal.h>

#include <sys/uuid.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/cred.h>

#endif
