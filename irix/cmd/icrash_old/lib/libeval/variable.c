#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libeval/RCS/variable.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <libelf.h>
#include <dwarf.h>
#include <libdwarf.h>
#include <syms.h>
#include <sym.h>

#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"
#include "variable.h"

/*
 * make_variable()
 */
variable_t *
make_variable(char *name, char *exp, node_t *np, int flags)
{
	variable_t *vp;
	char *typestr;
	k_uint_t value;

	vp = (variable_t *)alloc_block(sizeof(*vp), B_PERM);
	vp->v_name = (char *)alloc_block((strlen(name) + 1), B_PERM);
	strcpy(vp->v_name, name);
	vp->v_exp = (char *)alloc_block((strlen(exp) + 1), B_PERM);
	strcpy(vp->v_exp, exp);
	vp->v_flags = flags;
	if (np) {
		typestr = node_to_typestr(np, 0);
		if (np->flags & POINTER_FLAG) {
			vp->v_typestr = (char *)alloc_block(strlen(typestr) + 19, B_PERM);
			sprintf(vp->v_typestr, "%s0x%llx", typestr, np->value);
			free_block((k_ptr_t)typestr);
		}
		else if (np->flags & DWARF_TYPE_FLAG) {
			int size, shft;
			type_t *tp;
			dw_type_info_t *t;
			dw_die_t *d;

			/* Get the type information
			 */
			t = (dw_type_info_t *)np->type->t_ptr;
			if (t->t_type->d_tag == DW_TAG_typedef) {
				d = t->t_actual_type;
			}
			else {
				d = t->t_type;
			}
			size = t->t_byte_size;
			if (d->d_tag == DW_TAG_base_type) {
				vp->v_typestr = 
					(char *)alloc_block(strlen(typestr) + 19, B_PERM);
				if (np->flags & ADDRESS_FLAG) {
					kl_get_block(K, np->address, size, &value, "value");
				}
				else {
					switch (size) {
						case 1:
							*(unsigned char *)&value = np->value;
							break;

						case 2:
							*(unsigned short *)&value = np->value;
							break;

						case 4:
							*(unsigned int *)&value = np->value;
							break;

						case 8:
							*(unsigned long long *)&value = np->value;
							break;
					}
				}
				if (shft = (8 - size)) {
					value = value >> (shft * 8);
				}
				sprintf(vp->v_typestr, "%s%lld", typestr, value);
				free_block((k_ptr_t)typestr);
			}
			else {
				/* We have a more complex type (e.g., struct, union, etc.).
				 * We need to be able to assign a variable to a complex 
				 * type. For now though lets just convert it to a pointer
				 * to the type (using the address of the type as the pointer
				 * value).
				 */
				if (np->flags & ADDRESS_FLAG) {
					char *c;

					if (c = strchr(typestr, ')')) {
						*c++ = '*';
						*c++ = ')';
						*c = 0;
						value = np->address;
						if (!(flags & V_PERM_NODE)) {
							free_nodes(np);
							np = (node_t*)NULL;
						}
						vp->v_node = np;
						vp->v_typestr = 
							(char *)alloc_block(strlen(typestr) + 19, B_PERM);
						vp->v_flags |= V_REC_STRUCT;
						sprintf(vp->v_typestr, "%s0x%llx", typestr, value);
						return(vp);
					}
				}

				/* We have to return an error if there is no address
				 */
				free_block((k_ptr_t)typestr);
				if (!(flags & V_PERM_NODE)) {
					free_nodes(np);
					np = (node_t*)NULL;
				}
				free_variable(vp);
				return((variable_t*)NULL);
			}
		}
		else {
			value = np->value;
			vp->v_typestr = (char *)alloc_block(strlen(typestr) + 19, B_PERM);
			sprintf(vp->v_typestr, "%s%lld", typestr, value);
		}

		if (!(flags & V_PERM_NODE)) {
			free_nodes(np);
			np = (node_t*)NULL;
		}
		vp->v_node = np;
	}
	return(vp);
}

/*
 * clean_variable()
 */
void
clean_variable(variable_t *vp)
{
	if (vp->v_name) {
		free_block((k_ptr_t)vp->v_name);
		vp->v_name = 0;
	}
	if (vp->v_exp) {
		free_block((k_ptr_t)vp->v_exp);
		vp->v_exp = 0;
	}
	if (vp->v_typestr) {
		free_block((k_ptr_t)vp->v_typestr);
		vp->v_typestr = 0;
	}
	if (vp->v_node) {
		free_nodes(vp->v_node);
		vp->v_node = 0;
	}
}

/*
 * free_variable()
 */
void
free_variable(variable_t *vp)
{
	if (vp->v_name) {
		free_block((k_ptr_t)vp->v_name);
	}
	if (vp->v_exp) {
		free_block((k_ptr_t)vp->v_exp);
	}
	if (vp->v_typestr) {
		free_block((k_ptr_t)vp->v_typestr);
	}
	if (vp->v_node) {
		free_nodes(vp->v_node);
	}
	free_block((k_ptr_t)vp);
}

/*
 * init_variables()
 */
void
init_variables(vtab_t *vtab)
{
	/* Setup default variables */
}

/*
 * set_variable()
 */
int
set_variable(vtab_t *vtab, char *name, char *exp, node_t *np, int flags)
{
    int e, ret;
	variable_t *vp;

	/* Check to see if there is already a variable for name.
	 * If there is, free it and make a new one.
	 */
	if (vp = find_variable(vtab, name, 0)) {
		if (unset_variable(vtab, vp)) {
			/* You can't unset V_PERM varialbes. 
			 * XXX - There needs to be a way to reset the values of
			 * V_PERM variables. The key is to replace the value 
			 * without changing the type (and testing to make sure
			 * the type is not changed).
			 */
			return(1);
		}
	}
	vp = make_variable(name, exp, np, flags);

    /* If we don't have a root node, make this node the root. Otherwis
	 * add it to the tree.
     */
    if (!vtab->vt_root) {
		vtab->vt_root = vp;
        vtab->vt_count++;
        return(0);
    }

    ret = insert_btnode((btnode_t **)&vtab->vt_root, (btnode_t *)vp, 0);
    if (ret != -1) {
        vtab->vt_count++;
        return(0);
    }

	/* Free memory blocks? 
	 */
    return(1);
}

/*
 * unset_variable()
 */
int
unset_variable(vtab_t *vtab, variable_t *vp)
{
	int ret;

	if (vp->v_flags & V_PERM) {
		/*ERROR*/
		return(1);
	}
	
    ret = delete_btnode((btnode_t **)&vtab->vt_root, (btnode_t *)vp, 
			free_variable, 0);
    if (ret != -1) {
        vtab->vt_count--;
        return(0);
    }
	return(1);
}

/*
 * find_variable()
 */
variable_t *
find_variable(vtab_t *vtab, char *name, int type)
{
	int max_depth;
	variable_t *vp;

    /* Search for name in the variable tree
     */
    if (DEBUG(DC_EVAL, 3)) {
        vp = (variable_t *)find_btnode((btnode_t *)vtab->vt_root,
                        name, &max_depth);
        fprintf(KL_ERRORFP, "find_variable: varlist: name=%s (%s), "
			"max_count=%d\n", name, (vp ? "FOUND" : "NOT FOUND"), max_depth);
    }
	else {
		vp = (variable_t *)find_btnode((btnode_t *)vtab->vt_root,
					name, (int *)NULL);
	}

	/* If we are looking for a specific type of variable (V_TYPEDEF,
	 * V_STRING, or V_COMMAND) check to make sure the variable we 
	 * found matches the type flag.
	 */
	if (vp && type && !(vp->v_flags & type)) {
		return((variable_t*)NULL);
	}
	return(vp);
}
