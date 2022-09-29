#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libutil/RCS/stream.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#define _KERNEL
#include <sys/types.h>
#include <sys/param.h>
#include <sys/uio.h>
#undef _KERNEL
#include <sys/vnode.h>
#include <sys/stream.h>
#include "icrash.h"
#include "stream.h"
#include "extern.h"

/* Global variables
 */
stream_rec_t strmtab[1024];
int strmtab_valid = 0;
int strcnt = 0;

/* 
 * clean_strmtab()
 */
void
clean_strmtab()
{
	int i;
	file_rec_t *file, *nextfile;

	if (ACTIVE(K) && strmtab_valid) {
		for (i = 0; i < 1024; i++) {
			file = strmtab[i].file;
			while (file) {
				nextfile = file->next;
				free_block((k_ptr_t)file);
				file = nextfile;
			}
		}
		bzero(strmtab, sizeof(strmtab));
		strmtab_valid = 0;
		strcnt = 0;
	}
}

/*
 * streamcmp() -- Stream comparison function for qsort().
 */
int
streamcmp(stream_rec_t *s1, stream_rec_t *s2)
{
	if ((unsigned)s1->str < (unsigned)s2->str) {
		return (-1);
	} else if ((unsigned)s1->str > (unsigned)s2->str) {
		return (1);
	} else {
		return(0);
	}
}

/* 
 * find_stream_rec() 
 */
stream_rec_t *
find_stream_rec(kaddr_t stream)
{
	int i;

	if (DEBUG(DC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "find_stream_rec: stream=0x%llx\n", stream);
	}

	if (!strmtab_valid) {
		if (build_stream_tab()) {
			if (DEBUG(DC_STREAM, 1)) {
			}
			return(0);
		}
	}
	for (i = 0; i < strcnt; i++) {
		if (strmtab[i].str == stream) {
			return(&strmtab[i]);
		}
	}
	return((stream_rec_t *)NULL);
}

/*
 * build_stream_tab()
 */
int
build_stream_tab()
{
	int i = 0, j, k, gap, first_time = 1;
	stream_rec_t *srec;
	file_rec_t *frec;
	kaddr_t file, stream, vnode, snode;
	k_ptr_t strp, qp, kfp, vp, sp;

	if (ACTIVE(K) || !strmtab_valid) {

		strcnt = 0;

		/* Allocate some buffer space.
		 */
		strp = alloc_block(STDATA_SIZE(K), B_TEMP);
		qp = alloc_block(QUEUE_SIZE(K), B_TEMP);
		kfp = alloc_block(VFILE_SIZE(K), B_TEMP);
		vp = alloc_block(VNODE_SIZE(K), B_TEMP);
		sp = alloc_block(LSNODE_SIZE(K), B_TEMP);

		/* Get the first file pointer in open file chain. Then walk the list
		 * of open files looking for streams. Capture all stream pointers in
		 * stream table -- then sort it and remove duplicates. Next locate
		 * all files that point to each stream (along with vnode, snode and
		 * commonvp. On an active system, this has to be done each time through.
		 * When analyzing vmcore dumps, it only has to be done once.
		 */
		file = K_ACTIVEFILES(K);
		while (file) {
			if (!get_vfile(file, kfp, 0)) {
				if (DEBUG(DC_STREAM, 1)) {
					fprintf(KL_ERRORFP, "list_streams: file=0x%llx\n", file);
				}
				file = kl_kaddr(K, kfp, "vfile", "vf_next");
				continue;
			}
			if (KL_INT(K, kfp, "vfile", "vf_count")) {
				if (!get_vnode(kl_kaddr(K, kfp, "vfile", "vf_vnode"), vp, 0)) {
					if (DEBUG(DC_STREAM, 1)) {
						fprintf(KL_ERRORFP,
							"vnode virtual address (0x%llx) out of range!\n",
							kl_kaddr(K, kfp, "vfile", "vf_vnode"));
					}
					file = kl_kaddr(K, kfp, "vfile", "vf_next");
					continue;
				}
				if ((KL_UINT(K, vp, "vnode", "v_type") == VCHR) && 
						kl_kaddr(K, vp, "vnode", "v_stream")) {
					if (!get_lsnode(kl_kaddr(K, vp, "vnode", "v_data"), sp, 0)) {
						if (DEBUG(DC_STREAM, 1)) {
							fprintf(KL_ERRORFP,
							"snode virtual address (0x%llx) out of range!\n",
							kl_kaddr(K, vp, "vnode", "v_data"));
						}
						file = kl_kaddr(K, kfp, "vfile", "vf_next");
						continue;
					}
					strmtab[i].str = kl_kaddr(K, vp, "vnode", "v_stream");
					strcnt++, i++;
				}
			}
			file = kl_kaddr(K, kfp, "vfile", "vf_next");
		} 

		/* sort the stream pointers  
		 */
		qsort((char*)&strmtab[0], strcnt, sizeof(stream_rec_t), streamcmp);

		/* remove duplicates  
		 */
		i = 0; 
		j = 1;
		while (i < strcnt - 1) {
			while (strmtab[i].str == strmtab[j].str) {
				if (j == strcnt - 1) {
					strcnt = i + 1;
					goto rmvdups_done;
				}
				j++;
			}
			i++;
			gap = j - i;
			if (gap) {
				for (k = i; k < strcnt - gap; k++) {
					strmtab[k] = strmtab[k + gap];
				}
				strcnt -= gap;
			}
			j = i + 1;
		}
		strmtab_valid = 1;

rmvdups_done:

		/* Now go back through the list of open files and capture 
		 * information on each file pointing to each stream.
		 */
		file = K_ACTIVEFILES(K);
		while (file) {
			if (!get_vfile(file, kfp, 0)) {
				if (DEBUG(DC_STREAM, 1)) {
					fprintf(KL_ERRORFP, "list_streams: file=0x%llx\n", file);
				}
				file = kl_kaddr(K, kfp, "vfile", "vf_next");
				continue;
			}
			if (KL_INT(K, kfp, "vfile", "vf_count")) {
				vnode = kl_kaddr(K, kfp, "vfile", "vf_vnode");
				if (!get_vnode(vnode, vp, 0)) {
					if (DEBUG(DC_STREAM, 1)) {
						fprintf(KL_ERRORFP, "vnode virtual address (0x%llx) "
							"out of range!\n", vnode);
					}
					file = kl_kaddr(K, kfp, "vfile", "vf_next");
					continue;
				}
				if ((KL_UINT(K, vp, "vnode", "v_type") == VCHR) && 
						(stream = kl_kaddr(K, vp, "vnode", "v_stream"))) {

					snode = kl_kaddr(K, vp, "vnode", "v_data");
					if (!get_lsnode(snode, sp, 0)) {
						if (DEBUG(DC_STREAM, 1)) {
							fprintf(KL_ERRORFP, "snode virtual address "
								"(0x%llx) out of range!\n", snode);
						}
						file = kl_kaddr(K, kfp, "vfile", "vf_next");
						continue;
					}

					/* Locate the stream_rec in the strmtab[]
					 */
					if (!(srec = find_stream_rec(stream))) {
						if (DEBUG(DC_STREAM, 1)) {
							fprintf(KL_ERRORFP, "Virtual address (0x%llx) "
								"not found in stream table!\n", stream);
						}
        				free_block(strp);
						free_block(qp);
						free_block(qp);
						free_block(kfp);
						free_block(vp);
						free_block(sp);
						return(1);
					}

					/* Allocate a file_rec for this file and link it into the
					 * stream_rec.
					 */
					if (!(frec = srec->file)) {
						if (ACTIVE(K)) {
							srec->file = 
								alloc_block(sizeof(file_rec_t), B_TEMP);
						}
						else {
							srec->file = 
								alloc_block(sizeof(file_rec_t), B_PERM);
						}
						frec = srec->file;
					}
					else {
						while(1) {
							if (frec->next) {
								frec = frec->next;
								continue;
							}
							break;
						}
						if (ACTIVE(K)) {
							frec->next = 
								alloc_block(sizeof(file_rec_t), B_TEMP);
						}
						else {
							frec->next = 
								alloc_block(sizeof(file_rec_t), B_PERM);
						}
						frec = frec->next;
					}
					frec->fp = file;
					frec->vp = vnode;
					frec->sp = snode;
					frec->cvp = kl_kaddr(K, sp, "snode", "s_commonvp");
				}
			}
			file = kl_kaddr(K, kfp, "vfile", "f_next");
		} 
		free_block(strp);
		free_block(qp);
		free_block(kfp);
		free_block(vp);
		free_block(sp);
	}
	strmtab_valid = 2;
	return(0);
}
