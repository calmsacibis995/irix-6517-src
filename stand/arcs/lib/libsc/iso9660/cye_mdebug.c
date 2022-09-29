#include "common.h"





size_t cye_total_bytes = 0;
int cye_linked_list_len = 0;

typedef struct cye_mdebug_struct {
	void *address;
	size_t nbytes;
	struct cye_mdebug_struct *next;
} cye_mdebug_struct;

cye_mdebug_struct *cye_mdebug_head = NULL;



void *
get_memory(size_t nbytes, unsigned int is_dma)
{
#ifdef MDEBUG
	cye_mdebug_struct *ptr;
#endif
	void *ret;

	if(is_dma)
		ret = dmabuf_malloc(nbytes);
	else
		ret = malloc(nbytes);

#ifdef MDEBUG
	if(ret != NULL) {
		cye_linked_list_len++;
#ifdef DEBUG
		printf("len=%d %s(%d) returns 0x%x\n", cye_linked_list_len,
		is_dma ? "dmabuf_malloc" : "malloc", nbytes, ret);
#endif
		/* increment our total */
		cye_total_bytes += nbytes;

		/* add entry to head of list */
		ptr = (cye_mdebug_struct *) malloc(sizeof(cye_mdebug_struct));
		if(ptr != NULL) {
			ptr->address = ret;
			ptr->nbytes = nbytes;
			ptr->next = cye_mdebug_head;
			cye_mdebug_head = ptr;
		} else {
			printf("cye_%s() ERROR\n",
			is_dma ? "dmabuf_malloc" : "malloc");
		}
	}
#endif
	return(ret);
}



void
free_memory(void *address, unsigned int is_dma)
{

#ifdef MDEBUG
	size_t nbytes;
	cye_mdebug_struct *prev_ptr = NULL;
	cye_mdebug_struct *ptr = cye_mdebug_head;

	while(ptr != NULL && ptr->address != address) {
		prev_ptr = ptr;
		ptr = ptr->next;
	}

	if(ptr != NULL) {
		nbytes = ptr->nbytes;
		cye_total_bytes -= nbytes;
		cye_linked_list_len--;
#ifdef DEBUG
		printf("len=%d %s(%d) on 0x%x\n", cye_linked_list_len,
		is_dma ? "dmabuf_free" : "free", nbytes, address);
#endif
		if(prev_ptr != NULL) {
			prev_ptr->next = ptr->next;

		/* in the case with the first entry */
		} else {
			cye_mdebug_head = ptr->next;
		}
		free(ptr);
	} else {
		printf("ERROR cye_%s() : can't find address=0x%x\n",
		is_dma ? "dmabuf_free" : "free", address);
	}
#endif
	if(is_dma)
		dmabuf_free(address);
	else
		free(address);
}



void *
cye_dmabuf_malloc(size_t nbytes)
{
	return get_memory(nbytes, 1);
}

void *
cye_malloc(size_t nbytes)
{
	return get_memory(nbytes, 0);
}

void
cye_dmabuf_free(void *address)
{
	free_memory(address, 1);
}

void
cye_free(void *address)
{
	free_memory(address, 0);
}

#ifdef MDEBUG
void
cye_outstanding_nbytes(void)
{
	printf("chunks=%d cye_total_bytes=%d\n", cye_linked_list_len,
	cye_total_bytes);
}
#endif
