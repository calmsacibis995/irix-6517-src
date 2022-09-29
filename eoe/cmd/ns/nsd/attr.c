#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ns_api.h>
#include <ns_daemon.h>

nsd_attr_t *__nsd_attrs = 0;

/*
** This routine walks the stack freeing every record.
*/
void
nsd_attr_clear(nsd_attr_t *root) {
	nsd_attr_t *na;

	while (root && (--root->a_nlink <= 0)) {
		na = root;
		root = root->a_next;

		if (na->a_value) {
			free(na->a_value);
		}

		if (na->a_data && na->a_free) {
			(*na->a_free)(na->a_data);
		}

		free(na);
	}
}

int
nsd_attr_data_set(nsd_attr_t *na, void *data, int len, void (*dfree)(void *))
{
	if (na) {
		if (na->a_data && na->a_free) {
			(*na->a_free)(na->a_data);
		}

		na->a_data = data;
		na->a_size = len;
		na->a_free = dfree;

		return NSD_OK;
	}

	return NSD_ERROR;
}

/*
** This just pushes an attribute onto the given attribute stack.
*/
nsd_attr_t *
nsd_attr_append(nsd_attr_t **root, char *key, char *value, int append)
{
	nsd_attr_t *na, *end;
	char *nval;
	int len;

	if (! key) {
		return (nsd_attr_t *)0;
	}
	if (! root) {
		root = &__nsd_attrs;
	}
	
	end = *root;

	/*
	** Look for a duplicate.
	*/
	for (na = *root; na && na->a_key; na = na->a_next) {
		end = na;
		if (strcasecmp(key, na->a_key) == 0) {
			/*
			** Replace value with new one, carefully.
			*/
			len = strlen(value);
			nval = (char *)nsd_malloc(len + 1);
			if (! nval) {
				return (nsd_attr_t *)0;
			} else {
				memcpy(nval, value, len + 1);
				free(na->a_value);
				na->a_value = nval;
				na->a_size = len;
				return (na);
			}
		}
	}

	/*
	** Allocate space for new attribute.
	*/
	len = strlen(key);
	na = (nsd_attr_t *)nsd_calloc(1, sizeof(nsd_attr_t) + len + 1);
	if (! na) {
		return (nsd_attr_t *)0;
	}

	/*
	** Duplicate the key.  Appended onto structure.
	*/
	na->a_key = (char *)(na + 1);
	memcpy(na->a_key, key, len);

	/*
	** Duplicate the value.
	*/
	if (value) {
		len = strlen(value);
		na->a_value = nsd_malloc(len + 1);
		if (! na->a_value) {
			free(na), free(na->a_key);
			return (nsd_attr_t *)0;
		}
		memcpy(na->a_value, value, len + 1);
		na->a_size = len;
	}

	na->a_nlink++;
	if (append) {
		/* 
		** put attribute at the end of this files attributes
		*/
		if (! *root) {
			*root = na;
		}
		if (end) {
			na->a_next = end->a_next;
			end->a_next = na;
		}
	} else {
		/*
		** Push attribute onto list.
		*/
		if (*root) {
			na->a_next = *root;
		}
		*root = na;
	}

	return na;
}

nsd_attr_t *
nsd_attr_store(nsd_attr_t **root, char *key, char *value) {
	return nsd_attr_append(root, key, value, FALSE);
}

/*
** This is a link from the end of one list to the beginning of another.
*/
nsd_attr_t *
nsd_attr_continue(nsd_attr_t **root, nsd_attr_t *cont)
{
	nsd_attr_t *na, **end;

	if (! root || ! cont) {
		return (nsd_attr_t *)0;
	}

	/*
	** Find end of current list.
	*/
	for (end = root; *end && (*end)->a_key; end = &(*end)->a_next);
	if (*end) {
		nsd_attr_clear((*end)->a_next);
		(*end)->a_next = cont;
		cont->a_nlink++;

		return *end;
	} else {
		na = (nsd_attr_t *)nsd_calloc(1, sizeof(nsd_attr_t));
		if (! na) {
			return (nsd_attr_t *)0;
		}

		na->a_next = cont;
		cont->a_nlink++;

		*end = na;
		na->a_nlink++;

		return na;
	}
}

/*
** This just walks the stack looking for the given attribute.
*/
nsd_attr_t *
nsd_attr_fetch(nsd_attr_t *root, char *key)
{
	nsd_attr_t *na;

	if (! key) {
		return (nsd_attr_t *)0;
	}
	if (! root) {
		root = __nsd_attrs;
	}
	for (na = root; na && (!na->a_key || strcasecmp(na->a_key, key));
	    na = na->a_next);
	return na;
}

long
nsd_attr_fetch_long(nsd_attr_t *list, char *key, int base, long def)
{
	nsd_attr_t *na;
	char *p;
	long result;

	/*
	** strtol does not want a NULL passed to it
	*/
	na = nsd_attr_fetch(list, key);
	if (na && na->a_value) {
		result = strtol(na->a_value, &p, base);
		if (! p) {
			result = def;
		}
	} else {
		result = def;
	}

	return result;
}

/*
** A boolean is a string like "true" or "false", or a number like 0 or 1,
** or an empty string which just means true.  If the attribute is not
** found we return the default.
*/
int
nsd_attr_fetch_bool(nsd_attr_t *list, char *key, int def)
{
	nsd_attr_t *na;
	char *p;
	int result;

	na = nsd_attr_fetch(list, key);
	if (! na) {
		return def;
	}

	if (! na->a_value) {
		return TRUE;
	}

	result = strtol(na->a_value, &p, 10);
	if (p == na->a_value) {
		if (na->a_value[0] == 't' || na->a_value[0] == 'T') {
			return TRUE;
		} else {
			return FALSE;
		}
	}

	return result;
}

char *
nsd_attr_fetch_string(nsd_attr_t *list, char *key, char *def)
{
	nsd_attr_t *na;

	na = nsd_attr_fetch(list, key);
	if (na) {
		return na->a_value;
	} else {
		return def;
	}
}

/*
** This removes an attribute record from the given stack and frees up
** the space.
*/
int
nsd_attr_delete(nsd_attr_t **root, char *key) {
	nsd_attr_t *cna, *lna;

	if (! root) {
		return NSD_ERROR;
	}

	for (cna = *root, lna = (nsd_attr_t *)0; cna && cna->a_key;
	    lna = cna, cna = cna->a_next) {
		if (strcasecmp(key, cna->a_key) == 0) {
			if (lna) {
				lna->a_next = cna->a_next;
			} else {
				*root = cna->a_next;
			}
			cna->a_next = (nsd_attr_t *)0;
			nsd_attr_clear(cna);

			return NSD_OK;
		}
	}

	return NSD_ERROR;
}

/*
** This routine creates a copy of an attribute list.
*/
nsd_attr_t *
nsd_attr_copy(nsd_attr_t *list)
{
	nsd_attr_t *result, *na;
	int len;

	for (result = (nsd_attr_t *)0; list && list->a_key;
	    list = list->a_next) {
		len = strlen(list->a_key);
		na = (nsd_attr_t *)nsd_calloc(1, sizeof(nsd_attr_t) + len + 1);
		if (! na) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "nsd_attr_copy: failed malloc\n");
			nsd_attr_clear(result);
			return (nsd_attr_t *)0;
		}

		na->a_key = (char *)(na + 1);;
		memcpy(na->a_key, list->a_key, len);

		if (list->a_value) {
			len = strlen(list->a_value);
			na->a_value = (char *)nsd_malloc(len + 1);
			if (! na->a_value) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "nsd_attr_copy: failed malloc\n");
				free(na);
				nsd_attr_clear(result);
				return (nsd_attr_t *)0;
			}
			memcpy(na->a_value, list->a_value, len + 1);
			na->a_size = len;
		}

		na->a_nlink++;
		na->a_next = result;
		result = na;
	}

	return result;
}

/*
** This routine will verify that the first attribute is strickly a
** subset of the second list.  No continuations are followed on the
** first list.
*/
int
nsd_attr_subset(nsd_attr_t *sublist, nsd_attr_t *list)
{
	nsd_attr_t *ap;

	/*
	** For every attribute in sublist there must be a matching
	** attribute in list.
	*/
	for (; sublist && sublist->a_key; sublist = sublist->a_next) {
		if (!(ap = nsd_attr_fetch(list, sublist->a_key)) ||
		    (strcasecmp(ap->a_value, sublist->a_value) != 0)) {
			return NSD_ERROR;
		}
	}

	return NSD_OK;
}

/*
** This function will parse a string of the format [<key> = <value>]+
** and create a list attached to the passed in callout.
*/
int
nsd_attr_parse(nsd_attr_t **list, char *line, int lineno, char *filename)
{
	char *p, *q;
	nsd_attr_t *attr;
	int len;

	if (! list) {
		list = &__nsd_attrs;
	}
	if (! filename) {
		filename = "request";
	}

	for (p = line; p && *p; ) {
		/*
		** Skip whitespace.
		*/
		for (; *p && (isspace(*p) || (*p == '(') || (*p == ',')); p++);
		if (! *p || (*p == ')')) {
			break;
		}

		/*
		** key continues until = or whitespace.
		*/
		len = strcspn(p, " \t\n=,)");

		attr = (nsd_attr_t *)nsd_calloc(1, sizeof(nsd_attr_t) + len+1);
		if (! attr) {
			nsd_logprintf(NSD_LOG_RESOURCE, "nsd_attr_parse: malloc failed\n");
			return NSD_ERROR;
		}

		attr->a_key = (char *)(attr + 1);;
		memcpy(attr->a_key, p, len);

		for (q = p + len; *q && (isspace(*q) || *q == '='); q++);
		if (! *q || *q == ',') {
			len = 0;
			p = q;
		} else if (*q == '"') {
			/*
			** quoted string - value continues until matching
			** quote.
			*/
			p = strchr(++q, '"');
			if (!p) {
				if (lineno) {
					nsd_logprintf(NSD_LOG_OPER,
		"nsd_attr_parse: error in %s at %d+%d: unterminated string\n",
					    filename, lineno, p-line);
				} else {
					nsd_logprintf(NSD_LOG_OPER,
			    "nsd_attr_parse: unterminated string in: %s\n",
					    line);
				}
				nsd_attr_clear(attr);
				return NSD_ERROR;
			}
			len = p++ - q;
		} else {
			/*
			** value continues until a space or the end.
			*/
			len = strcspn(q, " \t\n,)");
			p = q + len;
		}

		if (len > 0) {
			attr->a_value = (char *)nsd_malloc(len + 1);
			if (! attr->a_value) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "nsd_attr_parse: malloc failed\n");
				nsd_attr_clear(attr);
				return NSD_ERROR;
			}
			memcpy(attr->a_value, q, len);
			attr->a_value[len] = (char)0;
			attr->a_size = len;
		}

		attr->a_nlink++;
		attr->a_next = *list;
		*list = attr;
	}

	return NSD_OK;
}
