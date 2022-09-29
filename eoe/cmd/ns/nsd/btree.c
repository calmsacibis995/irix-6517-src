#include <stdio.h>
#include <stdlib.h>
#include <ns_api.h>
#include <ns_daemon.h>

nsd_node_t *__nsd_files = 0;

static int
bsplit(nsd_node_t *np, unsigned hbit)
{
	int i;
	unsigned nbit;
	nsd_leaf_t *zdp=0, *odp=0, *lp;

	/*
	** Rehash data.
	*/
	lp = np->n_data;

	nbit = 1 << hbit;
	for (i = 0; i < BSIZE; i++) {
		if (lp->l_key[i] & nbit) {
			if (! odp) {
				odp = (nsd_leaf_t *)nsd_calloc(
				    1, sizeof(nsd_leaf_t));
				if (! odp) {
					if (zdp) {
						free(zdp);
					}
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "bsplit: failed calloc\n");
					return NSD_ERROR;
				}
			}
			odp->l_key[odp->l_index] =
			    lp->l_key[i];
			odp->l_value[odp->l_index++] =
			    lp->l_value[i];
		} else {
			if (! zdp) {
				zdp = (nsd_leaf_t *)nsd_calloc(
				    1, sizeof(nsd_leaf_t));
				if (! zdp) {
					if (odp) {
						free(odp);
					}
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "bsplit: failed calloc\n");
					return NSD_ERROR;
				}
			}
			zdp->l_key[zdp->l_index] =
			    lp->l_key[i];
			zdp->l_value[zdp->l_index++] =
			    lp->l_value[i];
		}
	}

	if (odp) {
		np->n_one = (nsd_node_t *)nsd_calloc(1,
		    sizeof(nsd_node_t));
		if (! np->n_one) {
			free(odp);
			if (zdp) {
				free(zdp);
			}
			nsd_logprintf(NSD_LOG_RESOURCE, "bsplit: failed calloc\n");
			return NSD_ERROR;
		}
		np->n_one->n_data = odp;
	}

	if (zdp) {
		np->n_zero = (nsd_node_t *)nsd_calloc(1,
		    sizeof(nsd_node_t));
		if (! np->n_zero) {
			free(zdp);
			if (odp) {
				free(np->n_one);
				np->n_one = (nsd_node_t *)0;
				free(odp);
			}
			nsd_logprintf(NSD_LOG_RESOURCE, "bsplit: failed calloc\n");
			return NSD_ERROR;
		}
		np->n_zero->n_data = zdp;
	}

	free(np->n_data);
	np->n_data = (nsd_leaf_t *)0;

	return NSD_OK;
}

/*
** This will insert a new key/value pair into the tree, splitting the
** node if needed.
*/
int
nsd_binsert(uint32_t key, nsd_file_t *file)
{
	nsd_node_t *np;
	unsigned hbit, maxbits = 32;

	if (! __nsd_files) {
		__nsd_files = (nsd_node_t *)nsd_calloc(1, sizeof(nsd_node_t));
		if (! __nsd_files) {
			nsd_logprintf(NSD_LOG_RESOURCE, "binsert: failed calloc\n");
			return NSD_ERROR;
		}
	}

	np = __nsd_files;
	hbit = 0;
	while (np && (hbit < maxbits)) {
		if (np->n_data) {
			if (np->n_data->l_index < BSIZE) {
				break;
			}
			if (bsplit(np, hbit) != NSD_OK) {
				nsd_logprintf(NSD_LOG_MIN, "binsert: failed bsplit\n");
				return NSD_ERROR;
			}
		}
		if (! (np->n_one || np->n_zero)) {
			np->n_data = (nsd_leaf_t *)nsd_calloc(1,
			    sizeof(nsd_leaf_t));
			if (! np->n_data) {
				nsd_logprintf(NSD_LOG_RESOURCE, "binsert: failed calloc\n");
				return NSD_ERROR;
			}
			break;
		}

		if (key & (1 << hbit++)) {
			if (! np->n_one) {
				np->n_one = nsd_calloc(1, sizeof(nsd_node_t));
				if (! np->n_one) {
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "binsert: failed calloc\n");
					return NSD_ERROR;
				}
			}
			np = np->n_one;
		} else {
			if (! np->n_zero) {
				np->n_zero = nsd_calloc(1, sizeof(nsd_node_t));
				if (! np->n_zero) {
					nsd_logprintf(NSD_LOG_RESOURCE,
					    "binsert: failed calloc\n");
					return NSD_ERROR;
				}
			}
			np = np->n_zero;
		}
	}
	
	/*
	** Insert the new data at end of list.
	*/
	if (np->n_data && np->n_data->l_index < BSIZE) {
		np->n_data->l_key[np->n_data->l_index] = key;
		np->n_data->l_value[np->n_data->l_index++] = file;
	} else {
		nsd_logprintf(NSD_LOG_MIN, "binsert: no data or already full\n");
		return NSD_ERROR;
	}

	return NSD_OK;
}

/*
** This will find the given key, removing the pointer, and recombining
** pages if possible.
*/
void *
nsd_bdelete(uint32_t key)
{
	nsd_node_t *np, *tmp=0;
	nsd_leaf_t *lp;
	void *vp;
	unsigned i, j, hbit=0, maxbits=32;

	if (! __nsd_files) {
		nsd_logprintf(NSD_LOG_MIN, "bdelete: empty\n");
		return (void *)0;
	}

	np = __nsd_files;
	while (np && ! np->n_data && (hbit < maxbits)) {
		tmp = np;
		if (key & (1 << hbit++)) {
			np = np->n_one;
		} else {
			np = np->n_zero;
		}
	}

	if (! np || ! np->n_data) {
		nsd_logprintf(NSD_LOG_MIN, "bdelete: not found: %u\n", key);
		return (void *)0;
	}
	lp = np->n_data;

	for (i = 0; i < lp->l_index; i++) {
		if (lp->l_key[i] == key) {
			vp = lp->l_value[i];

			if (lp->l_index == 1) {
				/*
				** If node is now empty we remove this node.
				*/
				free(lp);
				np->n_data = (nsd_leaf_t *)0;

				if (tmp) {
					if (tmp->n_zero == np) {
						tmp->n_zero = (nsd_node_t *)0;
					} else {
						tmp->n_one = (nsd_node_t *)0;
					}
					free(np);
				}
			} else {
				/*
				** Move data down to fill gap.
				*/
				for (j = i++; i < lp->l_index; j = i++) {
					lp->l_key[j] = lp->l_key[i];
					lp->l_value[j] = lp->l_value[i];
				}
				--lp->l_index;
			}

			return vp;
		}
	}

	nsd_logprintf(NSD_LOG_MIN, "bdelete: not found: %u\n", key);
	return (void *)0;
}

/*
** This will just walk the index tree returning the value for the key
** if found.
*/
void *
nsd_bget(uint32_t key) {
	nsd_node_t *np;
	nsd_leaf_t *lp;
	unsigned i, hbit=0, maxbits=32;

	if (! __nsd_files) {
		nsd_logprintf(NSD_LOG_MIN, "bget: empty\n");
		return (void *)0;
	}

	np = __nsd_files;
	while (np && ! np->n_data && (hbit < maxbits)) {
		if (key & (1 << hbit++)) {
			np = np->n_one;
		} else {
			np = np->n_zero;
		}
	}

	if (! np || ! np->n_data) {
		nsd_logprintf(NSD_LOG_MIN, "bget: not found: %u\n", key);
		return (void *)0;
	}

	lp = np->n_data;
	for (i = 0; i < lp->l_index; i++) {
		if (lp->l_key[i] == key) {
			return lp->l_value[i];
		}
	}

	nsd_logprintf(NSD_LOG_MIN, "bget: not found 2: %u\n", key);
	return (void *)0;
}

/*
** This will recursively free all the data and indexes for the tree.
*/
void
nsd_bclear(nsd_node_t *top, void (*bfree)(void *))
{
	unsigned i;

	if (! top) {
		top = __nsd_files;
	}

	if (top->n_data) {
		if (bfree) {
			for (i = 0; i < top->n_data->l_index; i++) {
				bfree(top->n_data->l_value[i]);
			}
		}
		free(top->n_data);
	}

	if (top->n_zero) {
		nsd_bclear(top->n_zero, bfree);
	}

	if (top->n_one) {
		nsd_bclear(top->n_one, bfree);
	}

	if (top == __nsd_files) {
		__nsd_files = (nsd_node_t *)0;
	}
	free(top);
}

void
nsd_blist(nsd_node_t *top, FILE *fp)
{
	int i;
	nsd_file_t *np;

	if (! top) {
		top = __nsd_files;
	}

	if (top->n_data) {
		for (i = 0; i < top->n_data->l_index; i++) {
			np = (nsd_file_t *)top->n_data->l_value[i];
			fprintf(fp, "%llu: %s from %llu refs %d\n", np->f_fh[0],
			    np->f_name, np->f_fh[1], np->f_nlink);
			if (top->n_data->l_key[i] != (uint32_t)np->f_fh[0]) {
				fprintf(fp, "**key-handle mismatch: %u != %u\n",
				    top->n_data->l_key[i],
				    (uint32_t)np->f_fh[0]);
			}
		}
	}
	if (top->n_zero) {
		nsd_blist(top->n_zero, fp);
	}
	if (top->n_one) {
		nsd_blist(top->n_one, fp);
	}
}
