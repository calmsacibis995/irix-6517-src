#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/libhwconfig/src/RCS/kl_libhwconfig.c,v 1.2 1999/05/08 04:07:05 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <klib/klib.h>

/*
 * dup_hwcomponent()
 */
hw_component_t *
dup_hwcomponent(hwconfig_t *hcp, hw_component_t *old)
{
	int flag;
	hw_component_t *new;
	string_table_t *st;

	flag = hcp->h_flags;
	st = hcp->h_st;
	new = (hw_component_t *)dup_block((caddr_t *)old, flag);

	if (old->c_location) {
		new->c_location = get_string(st, old->c_location, flag);
	}
	if (old->c_name) {
		new->c_name = get_string(st, old->c_name, flag);
	}
	if (old->c_serial_number) {
		new->c_serial_number = 
			get_string(st, old->c_serial_number, flag);
	}
	if (old->c_part_number) {
		new->c_part_number = get_string(st, old->c_part_number, flag);
	}
	if (old->c_revision) {
		new->c_revision = get_string(st, old->c_revision, flag);
	}
	if (old->c_data) {
		new->c_data = dup_block(old->c_data, flag);
	}

	/* clear out some of the old links that no longer are valid
	 */
	new->c_next = new->c_prev = new;
	new->c_cmplist = new->c_parent = (hw_component_t*)NULL;

	return(new);
}

/*
 * next_hwcmp()
 */
hw_component_t *
next_hwcmp(hw_component_t *hcp)
{
	return((hw_component_t *)next_htnode((htnode_t *)hcp));
}

/*
 * prev_hwcmp()
 */
hw_component_t *
prev_hwcmp(hw_component_t *hcp)
{
	return((hw_component_t *)prev_htnode((htnode_t *)hcp));
}

/*
 * hwcmp_insert_next()
 *
 * This function will add the next hardware component record to
 * a hwconfig htree. Note that in order for this function to work
 * properly, it is necessary that the level for each of the 
 * hw_component_s structs be filled in. 
 */
void
hwcmp_insert_next(hw_component_t *current, hw_component_t *next)
{
	ht_insert_next_htnode((htnode_t *)current, (htnode_t *)next);
}

/*
 * hwcmp_add_child()
 */
void
hwcmp_add_child(hw_component_t *cmp, hw_component_t *child)
{
	ht_insert_child((htnode_t *)cmp, (htnode_t *)child, HT_BEFORE);
}

/*
 * hwcmp_add_peer()
 */
void
hwcmp_add_peer(hw_component_t *cmp, hw_component_t *peer)
{
	ht_insert_pier((htnode_t *)cmp, (htnode_t *)peer, HT_AFTER);
}

/*
 * replace_hwcomponent()
 *
 * Replace an existing hwcomponent record with a new hardware
 * component record. Note that the sub-components of the existing
 * hwcomponenent are NOT switched over to the new hwcomponent. That
 * is because there is no garuantee that the sub-components are the
 * same. Also, this approach better allows the tracking of hardware 
 * component movement. It IS, however, necessary to adjust the parent 
 * pointer and possibly the component list pointer.
 */
void
replace_hwcomponent(hw_component_t *old, hw_component_t *new)
{
	hw_component_t *cp;

	if (old->c_next == old) {
		new->c_next = new->c_prev = new;
	}
	else {
		new->c_next = old->c_next;
		new->c_next->c_prev = new;
		new->c_prev = old->c_prev;
		new->c_prev->c_next = new;
	}

	/* Copy the parent pointer. If there was a parent and the old 
	 * hwcomponent was the first item on the parent's sub-component 
	 * list then we have to substitute the new pointer for the old 
	 * pointer.
	 */
	if (new->c_parent = old->c_parent) {
		if (new->c_parent->c_cmplist == old) {
			new->c_parent->c_cmplist = new;
		}
	}
	new->c_deinstall_time = 0; 
}

/*
 * insert_hwcomponent()
 */
void
insert_hwcomponent(hw_component_t *hwc1, hw_component_t *hwc2, int flag)
{
	if (flag == INSERT_AFTER) {
		hwc2->c_next = hwc1->c_next;
		hwc2->c_next->c_prev = hwc2;
		hwc1->c_next = hwc2;
		hwc2->c_prev = hwc1;
		hwc2->c_parent = hwc1->c_parent;
	}
	else {
		hwc2->c_prev = hwc1->c_prev;
		hwc2->c_prev->c_next = hwc2;
		hwc2->c_next = hwc1;
		hwc1->c_prev = hwc2;
		if (hwc2->c_parent = hwc1->c_parent) {
			if (hwc2->c_parent->c_cmplist == hwc1) {
				hwc2->c_parent->c_cmplist = hwc2;
			}
		}
	}
}

/*
 * unlink_hwcomponent()
 */
void
unlink_hwcomponent(hw_component_t *hwc)
{
	if ((hwc->c_parent) && (hwc->c_parent->c_cmplist == hwc)) {
		if (hwc->c_next != hwc) {
			hwc->c_parent->c_cmplist = hwc->c_next;
		}
		else {
			hwc->c_parent->c_cmplist = (hw_component_t *)NULL;
		}
	}
	hwc->c_next->c_prev = hwc->c_prev;
	hwc->c_prev->c_next = hwc->c_next;
	hwc->c_next = hwc->c_prev = hwc;
}

/*
 * free_next_hwcmp()
 *
 * Recursively walk through the hwcomponent tree and free all blocks
 * of memory allocated for a hw_component_s structure, plus free any
 * private data structures associated.
 */
void
free_next_hwcmp(hw_component_t *hwcp)
{
	hw_component_t *cur, *next;

	cur = hwcp;
	do {
		if (cur->c_cmplist) {
			free_next_hwcmp(cur->c_cmplist);
		}
		next = cur->c_next;

		/* Check and see if the strings in this hw_component_s struct
		 * had been allocated from a string table. If they have NOT, then
		 * they need to be freed up like all the rest of the blocks.
		 */
		if (!(cur->c_flags & STRINGTAB_FLG)) {
			if (cur->c_location) {
				kl_free_block(cur->c_location);
			}
			if (cur->c_name) {
				kl_free_block(cur->c_name);
			}
			if (cur->c_serial_number) {
				kl_free_block(cur->c_serial_number);
			}
			if (cur->c_part_number) {
				kl_free_block(cur->c_part_number);
			}
			if (cur->c_revision) {
				kl_free_block(cur->c_revision);
			}
		}
		if (cur->c_data) {
			kl_free_block(cur->c_data);
		}
		kl_free_block(cur);
		if (!(cur = next)) {
			return;
		}
	} while(cur != hwcp);
}

/*
 * free_hwcomponents()
 */
void
free_hwcomponents(hw_component_t *hwcp)
{
	free_next_hwcmp(hwcp);
}

/*
 * hw_find_location()
 */
hw_component_t *
hw_find_location(hw_component_t *list, hw_component_t *hcp)
{
	hw_component_t *cp;

	cp = list;
	do {
		if (!compare_hwcomponents(cp, hcp)) {
			/* The components are the same
			 */
			return(cp);
		}
		if (string_match(cp->c_location, hcp->c_location) &&
				string_match(cp->c_name, hcp->c_name)) {
			return(cp);
		}
		cp = cp->c_next;
	} while (cp != list);

	return((hw_component_t *)NULL);
}

/*
 * hw_find_insert_point()
 */
hw_component_t *
hw_find_insert_point(hw_component_t *list, hw_component_t *hcp, int *flag)
{
	hw_component_t *cp;

	if (flag) {
		*flag = 0;
	}

	cp = list;
	do {
		if (hcp->c_location) {
			if (!cp->c_location) {
				cp = cp->c_next;
				continue;
			}
			if (string_compare(cp->c_location, hcp->c_location) > 0) {
				if (cp != list) {
					return(cp);
				}
				else {
					return(list);
				}
			}
		}
		else if (hcp->c_name) {
			if (!cp->c_name) {
				cp = cp->c_next;
				continue;
			}
			if (string_compare(cp->c_name, hcp->c_name) > 0) {
				if (cp != list) {
					return(cp);
				}
				else {
					return(list);
				}
			}
		}
		cp = cp->c_next;
	} while (cp != list);

	/* If we didn't find anyting, return the list pointer (the item
	 * will be inserted at the end of the list).
	 */
	if (flag) {
		*flag = INSERT_AFTER;
	}
	return(list->c_prev);
}

/*
 * compare_hwcomponents()
 */
int
compare_hwcomponents(hw_component_t *hwcp1, hw_component_t *hwcp2)
{
	/* We have to assuem that level is the same for each hw_component
	 */

	if (!string_match(hwcp1->c_location, hwcp2->c_location)) {
		return(1);
	}

	if (hwcp1->c_category != hwcp2->c_category) {
		return(1);
	}

	if (hwcp1->c_type != hwcp2->c_type) {
		return(1);
	}

	if (hwcp1->c_inv_class != hwcp2->c_inv_class) {
		return(1);
	}

	/* If we get to here, we prety much have the same type of
	 * hw_component located in the same location. Now all we have to
	 * do is compare serial_number, revision, etc. Note that if
	 * a particular hw_component_s struct does not contain any of
	 * this data, we don't consider it a failure (as long as
	 * it is the same for both components).
	 */
	if (!string_match(hwcp1->c_serial_number, hwcp2->c_serial_number)) {
		return(1);
	}
	if (!string_match(hwcp1->c_revision, hwcp2->c_revision)) {
		return(1);
	}
	if (!string_match(hwcp1->c_part_number, hwcp2->c_part_number)) {
		return(1);
	}
	if (!string_match(hwcp1->c_name, hwcp2->c_name)) {
		return(1);
	}

	/* If we get to here, then it either is the same component, or there
	 * is not enough information in the record (e.g., serial number) to
	 * tell if it is the same component or not. In such cases, we will
	 * consider that it is the same.
	 */
	return(0);
}

/*
 * kl_alloc_hwconfig()
 */
hwconfig_t *
kl_alloc_hwconfig(int flags)
{
	hwconfig_t *hcp;

	hcp = (hwconfig_t *)kl_alloc_block(sizeof(hwconfig_t), ALLOCFLG(flags));
	if (flags & STRINGTAB_FLG) {
		hcp->h_st = init_string_table(flags);
	}
	hcp->h_flags = flags;
	return(hcp);
}

/*
 * kl_update_hwcomponent()
 *
 * Recursively walk through the hwcomponent tree and update all the
 * hw_component_s structs (set the node level).
 */
void
kl_update_hwcomponent(hw_component_t *hwcp)
{
	hw_component_t *cur, *next;

	cur = hwcp;
	do {
		if (cur->c_parent) {
			cur->c_level = cur->c_parent->c_level + 1;
		}
		if (cur->c_cmplist) {
			kl_update_hwcomponent(cur->c_cmplist);
		}
		cur = cur->c_next;
	} while(cur != hwcp);
}

/*
 * kl_update_hwconfig()
 */
int
kl_update_hwconfig(hwconfig_t *hcp)
{
	hw_component_t *hwcp;

	if (hcp && (hwcp = hcp->h_hwcmp_root)) {

		/* Set the level for each node
		 */
		kl_update_hwcomponent(hwcp);

		/* Make sure that the sys_id for each hw_component matches the
		 * one in the hwconfig_s struct. Also, update the count of
		 * hw_component_s structs (after skipping over the root).
		 */
		hcp->h_hwcmp_cnt = 0;
		while (hwcp = next_hwcmp(hwcp)) {
			hwcp->c_sys_id = hcp->h_sys_id;
			hcp->h_hwcmp_cnt++;
		}
		return(0);
	}
	return(1);
}

/*
 * kl_free_hwconfig()
 */
void
kl_free_hwconfig(hwconfig_t *hcp)
{
	if (hcp) {
		if (hcp->h_hwcmp_root) {
			free_next_hwcmp(hcp->h_hwcmp_root);
		}
		if (hcp->h_st) {
			/* Free the string table
			 */
			free_string_table(hcp->h_st);
		}
		kl_free_block(hcp);
	}
}
