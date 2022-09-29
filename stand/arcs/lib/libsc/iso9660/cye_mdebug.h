#ifndef __CYE_MDEBUG_H__
#define __CYE_MDEBUG_H__

/*
** cye_mdebug.h
**
** Just some debugging wrappers.  Simple linked list implementation.
**
*/





extern void * cye_dmabuf_malloc(size_t nbytes);
extern void * cye_malloc(size_t nbytes);
extern void cye_dmabuf_free(void *address);
extern void cye_free(void *address);
extern void cye_outstanding_nbytes(void);

#endif
