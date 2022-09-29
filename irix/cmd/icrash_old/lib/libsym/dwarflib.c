#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libsym/RCS/dwarflib.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"

/* Local variables
 */
Dwarf_Error err;
Dwarf_Die module_die;
Dwarf_Line *linebuf;
Dwarf_Signed linecount;
Dwarf_Arange *aranges_get_info_for_loc;
Dwarf_Signed current_module_arange_count;
Dwarf_Arange current_module_arange_info;

int line_numbers_valid;

extern symtab_t *stp;

#define T_ZEROSZ_OK 1

/*
 * dw_die()
 */
Dwarf_Die
dw_die(Dwarf_Off die_off)
{
	int dres;
	Dwarf_Die die;
	Dwarf_Error derr;

	/* convert the offset to a die 
	 */
	dres = dwarf_offdie(DBG, die_off, &die, &derr);
	if (dres != DW_DLV_OK) {
		if (DEBUG(DC_DWARF, 1)) {
			fprintf(KL_ERRORFP, "get_die: dwarf_offdie: %d\n",dres);
		}
		return(0);
	}
	return(die);
}

/*
 * dw_off()
 */
Dwarf_Off
dw_off(Dwarf_Die die)
{
	int dres;
	Dwarf_Error derr;
	Dwarf_Off die_off;

	/* convert the die to an offset 
	 */
	dres = dwarf_dieoffset(die, &die_off, &derr);
	if (dres != DW_DLV_OK) {
		if (DEBUG(DC_DWARF, 1)) {
			fprintf(KL_ERRORFP, "dw_off: dwarf_dieoffset: %d\n",dres);
		}
		return(0);
	}
	return(die_off);
}

/*
 * dw_tag()
 */
Dwarf_Half
dw_tag(Dwarf_Die die)
{
	int dres;
	Dwarf_Error derr;
	Dwarf_Half tag;

	/* convert the die to a tag 
	 */
	dres = dwarf_tag(die, &tag, &derr);
	if (dres != DW_DLV_OK) {
		if (DEBUG(DC_DWARF, 1)) {
			fprintf(KL_ERRORFP, "get_tag: dwarf_tag: %d\n",dres);
		}
		return(0);
	}
	return(tag);
}

/*
 * dw_child()
 */
Dwarf_Die
dw_child(Dwarf_Die die)
{
	int dres;
	Dwarf_Die child_die;
	Dwarf_Error derr;

	/* convert the offset to a die 
	 */
	dres = dwarf_child(die, &child_die, &derr);
	if (dres != DW_DLV_OK) {
		if (DEBUG(DC_DWARF, 1)) {
			fprintf(KL_ERRORFP, "get_child_die: dwarf_child: %d\n",dres);
		}
		return(0);
	}
	return(child_die);
}

/*
 * dw_child_off()
 */
Dwarf_Off
dw_child_off(Dwarf_Die die)
{
	Dwarf_Die child_die;

	if (!(child_die = dw_child(die))) {
		return(0);
	}
	return(dw_off(child_die));
}

/*
 * dw_sibling()
 */
Dwarf_Die
dw_sibling(Dwarf_Die die)
{
	int dres;
	Dwarf_Die sibling_die;
	Dwarf_Error derr;

	dres = dwarf_siblingof(DBG, die, &sibling_die, &derr);
	if (dres != DW_DLV_OK ) {
		if (DEBUG(DC_DWARF, 1)) {
			fprintf(KL_ERRORFP, "get_sibling: dwarf_siblingof: %d\n",dres);
		}
		return(0);
	}
	return(sibling_die);
}

/*
 * dw_siblin_off()
 */
Dwarf_Off
dw_sibling_off(Dwarf_Die die)
{
	Dwarf_Die child_die;

	if (!(child_die = dw_sibling(die))) {
		return(0);
	}
	return(dw_off(child_die));
}

/*
 * dw_type_off()
 */
int
dw_type_off(Dwarf_Die die, Dwarf_Off off, Dwarf_Off *type_off)
{
	Dwarf_Die type_die;
	Dwarf_Error derr;
	Dwarf_Off cu_off, die_cu_off, die_off;
	int	dres;

	/* Get the Compilation Unit offset of this die 
	 */
	dres = dwarf_die_CU_offset(die, &die_cu_off, &derr);
	if (dres != DW_DLV_OK) {
		return(-1);
	}

	/* Get the die offset from the beginning of debug section 
	 */
	dres = dwarf_dieoffset(die,& die_off, &derr);
	if (dres != DW_DLV_OK) {
		return(-1);
	}

	/* This offset is needed most
	 */
	cu_off = die_off - die_cu_off;

	dres = dwarf_offdie(DBG, cu_off + off, &type_die, &derr);
	if (dres != DW_DLV_OK) {
		return(-1);
	}

	dres = dwarf_dieoffset(type_die, &die_off, &derr);
	if (dres != DW_DLV_OK) {
		return(-1);
	}

	*type_off = die_off;
	return(0);
}

/*
 * dw_loc_list()
 */
ulong
dw_loc_list(Dwarf_Attribute dattr)
{
	Dwarf_Locdesc *llbuf;
	Dwarf_Signed no_of_elements;
	Dwarf_Half no_of_ops;
	Dwarf_Error derr;
	int i;
	Dwarf_Locdesc *locd;
	int dres;

	dres = dwarf_loclist(dattr, &llbuf, &no_of_elements, &derr);
	if (dres != DW_DLV_OK)
		return((ulong)-1);

	locd = llbuf + 0;
	no_of_ops = llbuf->ld_cents;
	for (i = 0 ; i < no_of_ops; i ++) {
		Dwarf_Small op;
		Dwarf_Unsigned opd1;

		op = locd->ld_s[i].lr_atom;
		if (op > DW_OP_nop)
			break;
		opd1 = locd->ld_s[i].lr_number;
		switch( op ) {
		case DW_OP_const1s:
		case DW_OP_const2s:
		case DW_OP_const4s:
		case DW_OP_const8s:
		case DW_OP_consts:
		case DW_OP_const1u:
		case DW_OP_const2u:
		case DW_OP_const4u:
		case DW_OP_const8u:
		case DW_OP_constu:
			return((ulong)opd1);
		}
	}
	return((ulong)-1);
}

/*
 * dw_free_die()
 */
dw_die_t *
dw_free_die(dw_die_t *d)
{
	dw_die_t *dnext;

	if (d->d_name) {
		klib_free_block(K, (k_ptr_t)d->d_name);
	}
	dnext = d->d_next;
	klib_free_block(K, (k_ptr_t)d);
	return(dnext);
}

/*
 * dw_clean_die()
 */
void
dw_clean_die(dw_die_t *d)
{
	if (d->d_name) {
		klib_free_block(K, (k_ptr_t)d->d_name);
	}
	bzero(d, sizeof(dw_die_t));
}

/*
 * dw_free_die_chain()
 */
void
dw_free_die_chain(dw_die_t *d)
{
	dw_die_t *d1;

	d1 = d;
	while(d1) {
		d1 = dw_free_die(d1);
	}
}

/*
 * dw_free_type_info()
 */
void
dw_free_type_info(dw_type_info_t *t)
{
	if (t->t_type_str) {
		klib_free_block(K, (k_ptr_t)t->t_type_str);
	}
	dw_free_die_chain(t->t_dlist);

	/* If we previously saved the real type information (e.g., typedef),
	 * make a recursive call here to free the related memory blocks.
	 */
	if (t->t_link) {
		dw_free_type_info(t->t_link);
	}
	klib_free_block(K, (k_ptr_t)t);
}

/*
 * dw_clean_type_info()
 */
void
dw_clean_type_info(dw_type_info_t *t)
{
	if (t->t_type_str) {
		klib_free_block(K, (k_ptr_t)t->t_type_str);
	}
	dw_free_die_chain(t->t_dlist);
	bzero(t, sizeof(dw_type_info_t));
}

/*
 * dw_attrlist()
 */
Dwarf_Signed
dw_attrlist(Dwarf_Die die, Dwarf_Attribute **atlist)
{
	int dres;
	Dwarf_Signed atcnt;
	Dwarf_Error derr;

	/* Pull out attributes 
	 */
	dres = dwarf_attrlist(die, atlist, &atcnt, &derr);
	if ( dres == DW_DLV_NO_ENTRY ) {
		return(0);
	}
	else if ( dres == DW_DLV_ERROR ) {
		if (DEBUG(DC_DWARF, 1)) {
			fprintf(KL_ERRORFP, "dw_attrlist: dwarf_attrlist: %d\n", dres);
		}
		return(-1);
	}
	return(atcnt);
}

/*
 * dw_attr()
 */
Dwarf_Half
dw_attr(Dwarf_Attribute dattr)
{
	int dres;
	Dwarf_Error derr;
	Dwarf_Half attr;

	dres = dwarf_whatattr(dattr, &attr, &derr);
	if (dres != DW_DLV_OK) {
		if (DEBUG(DC_DWARF, 1)) {
			fprintf(KL_ERRORFP, "get_attr: dwarf_whatattr: %d\n", dres);
		}
		return(0);
	}
	return(attr);
}

/*
 * dw_attr_value()
 */
Dwarf_Unsigned
dw_attr_value(Dwarf_Attribute dattr)
{
	int dres;
	Dwarf_Error derr;
	Dwarf_Unsigned return_value;

	dres = dwarf_formudata(dattr, &return_value, &derr);
	if (dres != DW_DLV_OK) {
		return(0);
	}
	return(return_value);
}

/*
 * dw_attr_name() 
 */
char *
dw_attr_name(Dwarf_Attribute dattr, int flags)
{
	int dres;
	char *name, *attr_name;
	Dwarf_Error derr;

	dres = dwarf_formstring(dattr, &name, &derr);
	if (dres != DW_DLV_OK) {
		return((char *)NULL);
	}
	attr_name = (char *)KLIB_ALLOC_BLOCK(K, strlen(name) + 1, flags);
	strcpy(attr_name, name);
	dwarf_dealloc(DBG, name, DW_DLA_STRING);
	return(attr_name);
}

/*
 * dw_attr_type()
 */
Dwarf_Off
dw_attr_type(Dwarf_Die die, Dwarf_Attribute dattr)
{
	int dres;
	Dwarf_Error derr;
	Dwarf_Off off, type_off;

	dres = dwarf_formref(dattr, &off, &derr);
	if (dres != DW_DLV_OK) {
		return(0);
	}

	/* Get the offset to type die
	 */
	if (dw_type_off(die, off, &type_off) == -1) {
		return(0);
	}
	return(type_off);
}

/*
 * dw_die_info()
 */
dw_die_t *
dw_die_info(Dwarf_Off die_off, dw_die_t *d, int flags)
{
	int dres, i;
	Dwarf_Die die;
	Dwarf_Error derr;
	Dwarf_Half attr;
	Dwarf_Attribute *atlist, dattr;
	Dwarf_Signed atcnt;

	/* Get the die
	 */
	if (!(die = dw_die(die_off))) {
		return((dw_die_t *)NULL);
	}

	dw_clean_die(d);

	/* Initialize the dwarf_die_s struct
	 */
	d->d_off = die_off;
	d->d_tag = dw_tag(die);

	/* Get the attributes for this die. If there aren't any attributes,
	 * return dw_die_t pointer. If an error occurs, return zero.
	 */
	atcnt = dw_attrlist(die, &atlist);
	if (atcnt == 0) {
		return(d);
	}
	else if (atcnt == -1) {
		return(0);
	}

	/* Cycle through the attribute list
	 */
	for (i = 0; i < atcnt; i++) {

		/* Get the attribute 
		 */
		dattr = atlist[i];
		if (!(attr = dw_attr(dattr))) {
			continue;
		}

		switch(d->d_tag) {

		case DW_TAG_union_type:
			switch(attr) {

			case DW_AT_abstract_origin:
			case DW_AT_accessibility:
			case DW_AT_sibling:
			case DW_AT_start_scope:
			case DW_AT_visibility:
				break;

			case DW_AT_declaration:
				d->d_declaration = dw_attr_value(dattr);
				break;

			case DW_AT_byte_size:
				d->d_byte_size = dw_attr_value(dattr);
				break;

			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			default:
				break;
			}
			break;

		case DW_TAG_structure_type:
			switch(attr) {

			case DW_AT_byte_size:
				d->d_byte_size = dw_attr_value(dattr);
				break;

			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			case DW_AT_type:
				d->d_type_off = dw_attr_type(die, dattr);
				break;

			case DW_AT_abstract_origin:
			case DW_AT_accessibility:
			case DW_AT_declaration:
			case DW_AT_sibling:
			case DW_AT_start_scope:
			case DW_AT_visibility:
				break;

			default:
				break;
			}
			break;

		case DW_TAG_member:
			switch(attr) {

			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			case DW_AT_byte_size:
				d->d_byte_size = dw_attr_value(dattr);
				break;

			case DW_AT_bit_offset:
				d->d_bit_offset = dw_attr_value(dattr);
				break;

			case DW_AT_bit_size:
				d->d_bit_size = dw_attr_value(dattr);
				break;

			case DW_AT_data_member_location:
				d->d_loc = dw_loc_list(dattr);
				break;

			case DW_AT_type:
				d->d_type_off = dw_attr_type(die, dattr);
				break;

			case DW_AT_accessibility:
			case DW_AT_declaration:
			case DW_AT_sibling:
			case DW_AT_visibility:
				break;

			default:
				break;
			}
			break;

		case DW_TAG_base_type:
			switch(attr) {
			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			case DW_AT_byte_size:
				d->d_byte_size = dw_attr_value(dattr);
				break;

			case DW_AT_bit_offset:
				d->d_bit_offset = dw_attr_value(dattr);
				break;

			case DW_AT_bit_size:
				d->d_bit_size = dw_attr_value(dattr);
				break;
					
			case DW_AT_encoding:
				d->d_encoding = dw_attr_value(dattr);
				break;

			case DW_AT_sibling:
				d->d_sibling_off = dw_child_off(die);
				break;

			default:
				break;
			}
			break;


		case DW_TAG_array_type:
			switch(attr) {

			case DW_AT_byte_size:
				d->d_byte_size = dw_attr_value(dattr);
				break;

			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			case DW_AT_ordering:
				break;

			case DW_AT_stride_size:
				break;

			case DW_AT_type:
				d->d_type_off = dw_attr_type(die, dattr);
				break;

			case DW_AT_sibling:
				d->d_sibling_off = dw_child_off(die);
				break;

			case DW_AT_abstract_origin:
			case DW_AT_accessibility:
			case DW_AT_declaration:
			case DW_AT_start_scope:
			case DW_AT_visibility:
				break;

			default:
				break;
			}
			break;

		case DW_TAG_subrange_type:
			switch (attr) {

			case DW_AT_count:
				d->d_count = dw_attr_value(dattr);
				break;

			case DW_AT_lower_bound:
				d->d_lower_bound = dw_attr_value(dattr);
				break;

			case DW_AT_upper_bound:
				d->d_upper_bound = dw_attr_value(dattr);
				break;

			case DW_AT_byte_size:
				d->d_byte_size = dw_attr_value(dattr);
				break;

			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			case DW_AT_type:
				d->d_type_off = dw_attr_type(die, dattr);
				break;

			case DW_AT_sibling:
				d->d_sibling_off = dw_child_off(die);
				break;

			case DW_AT_abstract_origin:
			case DW_AT_accessibility:
			case DW_AT_declaration:
			case DW_AT_visibility:
				break;

			}
			break;

		case DW_TAG_pointer_type:
			switch(attr) {

			case DW_AT_address_class:
				d->d_address_class = dw_attr_value(dattr);
				break;

			case DW_AT_type:
				d->d_type_off = dw_attr_type(die, dattr);

			case DW_AT_sibling:
				d->d_sibling_off = dw_child_off(die);
				break;

			default:
				break;
			}
			break;

		case DW_TAG_subroutine_type:
			switch(attr) {

			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			case DW_AT_type:
				d->d_type_off = dw_attr_type(die, dattr);
				break;

			case DW_AT_address_class:
				d->d_address_class = dw_attr_value(dattr);
				break;

			case DW_AT_prototyped:
				break;

			case DW_AT_sibling:
				d->d_sibling_off = dw_child_off(die);
				break;

			case DW_AT_abstract_origin:
			case DW_AT_accessibility:
			case DW_AT_declaration:
			case DW_AT_start_scope:
			case DW_AT_visibility:
				break;

			default:
				break;
			}
			break;

		case DW_TAG_enumeration_type:
			switch(attr) {

			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			case DW_AT_byte_size:
				d->d_byte_size = dw_attr_value(dattr);
				break;

			case DW_AT_sibling:
				d->d_sibling_off = dw_child_off(die);
				break;

			case DW_AT_abstract_origin:
			case DW_AT_accessibility:
			case DW_AT_declaration:
			case DW_AT_start_scope:
			case DW_AT_visibility:
				break;

			default:
				break;
			}
			break;

		case DW_TAG_enumerator:
			switch(attr) {
			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			case DW_AT_const_value:
				d->d_const = dw_attr_value(dattr);
				break;

			case DW_AT_sibling:
				d->d_sibling_off = dw_sibling_off(die);

			default:
				break;
			}
			break;

		case DW_TAG_typedef:
			switch(attr) {

			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			case DW_AT_type:
				d->d_type_off = dw_attr_type(die, dattr);
				break;

			case DW_AT_byte_size:
				d->d_byte_size = dw_attr_value(dattr);
				break;

			case DW_AT_sibling:
				d->d_sibling_off = dw_child_off(die);
				break;

			case DW_AT_abstract_origin:
			case DW_AT_accessibility:
			case DW_AT_start_scope:
			case DW_AT_visibility:
				break;

			default:
				break;
			}
			break;

		case DW_TAG_volatile_type:
			switch(attr) {
			case DW_AT_type:
				d->d_type_off = dw_attr_type(die, dattr);
				break;

			case DW_AT_sibling:
				d->d_sibling_off = dw_child_off(die);
				break;

			default:
				break;
			}
			break;

		case DW_TAG_const_type:
			switch(attr) {
			case DW_AT_type:
				d->d_type_off = dw_attr_type(die, dattr);
				break;

			case DW_AT_sibling:
				d->d_sibling_off = dw_child_off(die);
				break;

			default:
				break;
			}
			break;

		case DW_TAG_variable:
			switch(attr) {

			case DW_AT_name:
				d->d_name = dw_attr_name(dattr, flags);
				break;

			case DW_AT_const_value:
				d->d_const = dw_attr_value(dattr);
				break;

			case DW_AT_declaration:
				d->d_declaration = dw_attr_value(dattr);
				break;

			case DW_AT_external:
				d->d_external = dw_attr_value(dattr);
				break;

			case DW_AT_location:
				d->d_loc = dw_loc_list(dattr);
				break;

			case DW_AT_type:
				d->d_type_off = dw_attr_type(die, dattr);
				break;

			case DW_AT_sibling:
				d->d_sibling_off = dw_child_off(die);
				break;

			case DW_AT_accessibility:
			case DW_AT_segment:
			case DW_AT_specification:
			case DW_AT_start_scope:
			case DW_AT_visibility:
				break;

			default:
				break;
			}
			break;

		default:
			break;
		}
		dwarf_dealloc(DBG, atlist[i], DW_DLA_ATTR);
	}
	if ((d->d_tag == DW_TAG_structure_type) || 
	    (d->d_tag == DW_TAG_union_type)) {
		d->d_sibling_off = dw_child_off(die);
	}
	else if ((d->d_tag == DW_TAG_member) || 
		 (d->d_tag == DW_TAG_enumerator)) {
		d->d_sibling_off = dw_sibling_off(die);
	}
	return(d);
}

/*
 * dw_type_str()
 */
char *
dw_type_str(dw_type_info_t *t, int flags)
{
	int i, ptr_type = 0;
	char *type_str;
	dw_die_t *dp;
	
	type_str = (char *)KLIB_ALLOC_BLOCK(K, 128, flags);

	/* Check to see if this is a struct or union type
	 */
	if (!(dp = t->t_dlist->d_next)) {
		switch (t->t_tag) {

		case DW_TAG_structure_type:
			strcat(type_str, "struct");
			if (t->t_name) {
				strcat (type_str, " ");
			}
			break;

		case DW_TAG_union_type:
			strcat(type_str, "union");
			if (t->t_name) {
				strcat (type_str, " ");
			}
			break;

		case DW_TAG_enumeration_type:
			strcat(type_str, "enum");
			if (t->t_name) {
				strcat (type_str, " ");
			}
			break;

		case DW_TAG_variable:
			klib_free_block(K, (k_ptr_t)type_str);
			return((char *)NULL);

		default:
			break;
		}
		if (t->t_name) {
			strcat (type_str, t->t_name);
		}
		return(type_str);
	}

	/* Walk the modifer/type blocks For now, just look to see if there 
	 * are any pointer_type modifers. Need to address volatile and const
	 * modifiers as well.
	 */
	while(dp) {
		switch (dp->d_tag) {

		case DW_TAG_pointer_type:
			ptr_type++;
			break;

		case DW_TAG_volatile_type:
		case DW_TAG_const_type:
			break;

		default:
			goto got_type;
		}
		dp = dp->d_next;
	}

got_type:

	/* If there is no type, then type is void.
	 */
	if (!dp) {
		if (ptr_type) {
			strcat(type_str, "void*");
			return(type_str);
		}
		else {
			klib_free_block(K, (k_ptr_t)type_str);
			return((char *)NULL);
		}
	}

	switch (dp->d_tag) {

	case DW_TAG_structure_type:
		strcat(type_str, "struct");
		if (dp->d_name) {
			strcat (type_str, " ");
		}
		break;

	case DW_TAG_union_type:
		strcat(type_str, "union");
		if (dp->d_name) {
			strcat (type_str, " ");
		}
		break;

	case DW_TAG_enumeration_type:
		strcat(type_str, "enum");
		if (dp->d_name) {
			strcat (type_str, " ");
		}
		break;

	case DW_TAG_array_type: {
		int ptr_cnt = 0;

		dp = dp->d_next;
		while (dp->d_tag == DW_TAG_pointer_type) {
			ptr_cnt++;
			dp = dp->d_next;
		}
		if (dp) {
			if (dp->d_tag == DW_TAG_structure_type) {
				strcat(type_str, "struct");
				if (dp->d_name) {
					strcat(type_str, " ");
				}
			}
			else if (dp->d_tag ==  DW_TAG_union_type) {
				strcat(type_str, "union");
				if (dp->d_name) {
					strcat(type_str, " ");
				}
			}

			if (dp->d_name) {
				strcat (type_str, dp->d_name);
			}
			else if (!dp->d_next) {
				strcat(type_str, "void");
			}
		}
		if (ptr_cnt) {
			if (!dp) {
				strcat(type_str, "void");
			}
			for (i = 0; i < ptr_cnt; i++) {
				strcat (type_str, "*");
			}
		}
		return(type_str);
	}

	default:
		break;
	}
	if (dp->d_name) {
		strcat (type_str, dp->d_name);
		for (i = 0; i < ptr_type; i++) {
			strcat (type_str, "*");
		}
	}
	else {
		if ((dp->d_tag != DW_TAG_structure_type) && 
		    (dp->d_tag != DW_TAG_union_type)) {
			strcat(type_str, "void");
		}
		for (i = 0; i < ptr_type; i++) {
			strcat (type_str, "*");
		}
	}
	return(type_str);
}

/*
 * dw_type_info()
 */
dw_type_info_t *
dw_type_info(Dwarf_Off off, dw_type_info_t *t, int flags) 
{
	int type_found = 0, real_type_found = 0;
	dw_die_t *d, *next, *last;
	Dwarf_Off next_off;

	/* Get the attributes for die at offset off
	 */
	d = KLIB_ALLOC_BLOCK(K, sizeof(dw_die_t), flags);
	if (!dw_die_info(off, d, flags)) {
		dw_free_die(d);
		return((dw_type_info_t *)NULL);
	}

	/* Clean out the type_info_s struct
	 */
	dw_clean_type_info(t);

	/* Get the information about the die at off
	 */
	t->t_tag = d->d_tag;
	t->t_off = d->d_off;
	t->t_name = d->d_name;
	t->t_dlist = d;
	t->t_byte_size = d->d_byte_size;

	switch (d->d_tag) {

	case DW_TAG_structure_type :
	case DW_TAG_union_type :
		t->t_member_off = d->d_sibling_off;
		t->t_type_str = dw_type_str(t, flags);
		t->t_type = t->t_actual_type = d;
		return(t);

	case DW_TAG_member :
		t->t_loc = d->d_loc;
		t->t_bit_offset = d->d_bit_offset;
		t->t_bit_size = d->d_bit_size;
		t->t_member_off = d->d_sibling_off;
		break;

	case DW_TAG_variable :
		t->t_bit_offset = d->d_bit_offset;
		t->t_bit_size = d->d_bit_size;
		break;

	default:
		t->t_bit_offset = d->d_bit_offset;
		t->t_bit_size = d->d_bit_size;
		t->t_type = d;
		if (d->d_tag != DW_TAG_typedef) {
			t->t_actual_type = d;
		}
		break;
	}

	/* Now walk through the type dies, capturing information on 
	 * each of the type modifer entrys.
	 */
	last = t->t_dlist;
	next_off = d->d_type_off;
	while (next_off) {

		d = (dw_die_t *)KLIB_ALLOC_BLOCK(K, sizeof(dw_die_t), flags);
		
		/* Get the die for type_off
		 */
		if (!dw_die_info(next_off, d, flags)) {
			dw_free_die(d);
			return((dw_type_info_t *)NULL);
		}

		switch(d->d_tag) {

		case DW_TAG_typedef:
		case DW_TAG_subroutine_type:
			if (!t->t_type) {
				t->t_type = d;
			}
			break;

		case DW_TAG_pointer_type:
			if (!t->t_type) {
				t->t_type = d;
			}
			if (!t->t_actual_type) {
				t->t_actual_type = d;
			}
			if (!t->t_byte_size) {
				t->t_byte_size = K_NBPW(K);
			}
			break;

		case DW_TAG_array_type:
		case DW_TAG_enumeration_type:
		case DW_TAG_union_type:
		case DW_TAG_structure_type:
			if (!t->t_type) {
				t->t_type = d;
			}
			if (!t->t_actual_type) {
				t->t_actual_type = d;
			}
			if (!t->t_byte_size) {
				t->t_byte_size = d->d_byte_size;
			}
			break;

		case DW_TAG_base_type:
			if (!t->t_type) {
				t->t_type = d;
			}
			if (!t->t_actual_type) {
				t->t_actual_type = d;
			}
			if (!t->t_byte_size) {
				t->t_byte_size = d->d_byte_size;
			}
			if (!t->t_bit_offset) {
				t->t_bit_offset = d->d_bit_offset;
			}
			if (!t->t_bit_size) {
				t->t_bit_size = d->d_bit_size;
			}
			break;

		case DW_TAG_const_type:
		case DW_TAG_volatile_type:
			break;

		default:
			break;
		}

		last->d_next = d;
		if (next_off = d->d_type_off) {
			last = d;
		}
	}
	t->t_type_str = dw_type_str(t, flags);
	return(t);
}

/*
 * dw_make_node()
 */
dw_type_t *
dw_make_type(char *name)
{
	dw_type_t *dtp;

	dtp = (dw_type_t *)malloc(sizeof(*dtp));
	bzero(dtp, sizeof(*dtp));
	if (name) {
		dtp->dt_name = strdup(name);
	}
	return(dtp);
}

/*
 * dw_type() -- convert a dw_die_t to a dw_type_t
 */
dw_type_t *
dw_type(dw_type_info_t *dti, int flag) 
{
	dw_type_t *dtp;

	/* Make sure that there is type information available from this die.
	 */
	if ((dti->t_tag == DW_TAG_structure_type) ||
	    (dti->t_tag == DW_TAG_union_type)) {
		if (!(flag & T_ZEROSZ_OK) && !dti->t_byte_size) {
			return((dw_type_t *)NULL);
		}
	}
	else if (dti->t_tag == DW_TAG_typedef) {
		if (!dti->t_type) {
			return((dw_type_t *)NULL);
		}
	}
	dtp = dw_make_type(dti->t_name);
	dtp->dt_tag = dti->t_tag;
	dtp->dt_off = dti->t_off;
	if (dti->t_type) {
		dtp->dt_type_off = dti->t_type->d_off;
	}
	dtp->dt_size = dti->t_byte_size;
	dtp->dt_offset = dti->t_loc;
	return(dtp);
}

/*
 * dw_populate_type()
 *
 *  Walk the member die's and fill in type info for the various 
 *  fields. This is necessary for performances reasons. It is much 
 *  quicker to walk the various fields in a structure than
 *  to do a lookup every time. This operation is mainly for use
 *  by such routines as STRUCT, FIELD(), etc.
 */
int
dw_populate_type(dw_type_t *dtp)
{
	dw_type_t *mtp = (dw_type_t *)NULL;
	dw_type_info_t *dti;
	Dwarf_Off next_off;

	/* Make sure that the type is either struct, union, or enumeration
	 * (this operation should not be performed on a typedef to a union
	 * or struct).
	 */
	if ((dtp->dt_tag != DW_TAG_structure_type) &&
	    (dtp->dt_tag != DW_TAG_union_type) && 
	    (dtp->dt_tag != DW_TAG_enumeration_type)) {
		return(1);
	}

	dti = (dw_type_info_t *)klib_alloc_block(K, sizeof(*dti), B_TEMP);

	/* get the type information 
	 */
	if (!dw_type_info(dtp->dt_off, dti, 0)) {
		dw_free_type_info(dti);
		return(1);
	}

	/* Now walk the member dies and link into the type struct
	 */
	if (dtp->dt_tag == DW_TAG_enumeration_type) {
		next_off = dti->t_sibling_off;
	}
	else {
		next_off = dti->t_member_off;
	}
	while(next_off) {

		/* get the die information for the next member
		 */
		if (!dw_type_info(next_off, dti, 0)) {
			dw_free_type_info(dti);
			return(1);
		}
		if (!dtp->dt_member) {
			dtp->dt_member = (symdef_t*)dw_type(dti, T_ZEROSZ_OK);
			mtp = (dw_type_t *)dtp->dt_member;
		}
		else {
			mtp->dt_member = (symdef_t*)dw_type(dti, T_ZEROSZ_OK);
			mtp = (dw_type_t *)mtp->dt_member;
		}
		if (dtp->dt_tag == DW_TAG_enumeration_type) {
			next_off = dti->t_sibling_off;
		}
		else {
			next_off = dti->t_member_off;
		}
	}
	dw_free_type_info(dti);
	return(0);
}

/*
 * dw_add_type()
 */
int 
dw_add_type(symtab_t *stp, dw_type_t *dtp)
{
	int ret;

	/* If there isn't any root node, make dtp the root node
	 */
	if (!stp->st_type_ptr) {
		stp->st_type_ptr = SYMDEF(dtp);
		dw_link_type(stp, dtp);
		stp->st_type_cnt++;
		return(0);
	}
	ret = insert_btnode((btnode_t **)&stp->st_type_ptr, (btnode_t *)dtp, 0);
	if (ret != -1) {
		dw_link_type(stp, dtp);
		stp->st_type_cnt++;
		return(0);
	}
	return(1);
}

/*
 * dw_add_variable()
 */
int 
dw_add_variable(symtab_t *stp, dw_type_t *dtp)
{
	int ret;

	/* If there isn't any root node, make dtp the root node
	 */
	if (!stp->st_var_ptr) {
		stp->st_var_ptr = SYMDEF(dtp);
		dw_link_type(stp, dtp);
		stp->st_var_cnt++;
		return(0);
	}
	ret = insert_btnode((btnode_t **)&stp->st_var_ptr, (btnode_t *)dtp, 0);
	if (ret != -1) {
		dw_link_type(stp, dtp);
		stp->st_var_cnt++;
		return(0);
	}
	return(1);
}

/*
 * dw_link_type()
 */
void
dw_link_type(symtab_t *stp, dw_type_t *dtp)
{
	switch (dtp->dt_tag) {
	case DW_TAG_structure_type:
		if (stp->st_struct) {
			dtp->dt_next = SYMDEF(stp->st_struct);
		}
		stp->st_struct = SYMDEF(dtp);
		stp->st_nstruct++;
		break;

	case DW_TAG_union_type:
		if (stp->st_union) {
			dtp->dt_next = stp->st_union;
		}
		stp->st_union = SYMDEF(dtp);
		stp->st_nunion++;
		break;

	case DW_TAG_enumeration_type:
		if (stp->st_enumeration) {
			dtp->dt_next = stp->st_enumeration;
		}
		stp->st_enumeration = SYMDEF(dtp);
		stp->st_nenumeration++;
		break;

	case DW_TAG_typedef:
		if (stp->st_typedef) {
			dtp->dt_next = stp->st_typedef;
		}
		stp->st_typedef = SYMDEF(dtp);
		stp->st_ntypedef++;
		break;

	case DW_TAG_base_type:
		if (stp->st_base) {
			dtp->dt_next = stp->st_base;
		}
		stp->st_base = SYMDEF(dtp);
		stp->st_nbase++;
		break;
	}
}

/*
 * dw_lkup()
 */
dw_type_t *
dw_lkup(symtab_t *stp, char *name, int type)
{
	int max_depth;
	dw_type_t *dtp;
	btnode_t *root;

	if (type == DW_TAG_variable) {
		root = (btnode_t *)stp->st_var_ptr;
	}
	else {
		root = (btnode_t *)stp->st_type_ptr;
	}
	dtp = (dw_type_t *)find_btnode(root, name, &max_depth);
	if (DEBUG(DC_DWARF, 1)) {
		fprintf(KL_ERRORFP, 
			"dw_lkup: name=%s, max_depth=%d\n", name, max_depth);
	}

	/* Change the current dbg index to the one in dtp
	 */
	if (dtp) {
		curnmlist = dtp->dt_nmlist;
	}
	return(dtp);
}

/*
 * dw_lkup_link()
 *
 *   Walk the linked lists in symtab to see if a particular name is in the
 *   list. This way of looking up a type is MUCH slower than by doing
 *   a search on the type tree (via dw_lkup()).
 */
dw_type_t *
dw_lkup_link(symtab_t *stp, char *name, int type)
{
	dw_type_t *dtp = (dw_type_t *)NULL;

#ifdef NOTYET
	switch (type) {

	case DW_TAG_structure_type:
		dtp = DW_TYPE(stp->st_struct);
		break;

	case DW_TAG_union_type:
		dtp = DW_TYPE(stp->st_union);
		break;

	case DW_TAG_typedef:
		dtp = DW_TYPE(stp->st_typedef);
		break;

	case DW_TAG_base_type:
		dtp = DW_TYPE(stp->st_base);
		break;

	case DW_TAG_variable:
		dtp = DW_TYPE(stp->st_variable);
		break;

	default:
		return((dw_type_t *)NULL);
	}
#endif
	return(dtp);
}

/*
 * dw_find_member()
 */
dw_type_t *
dw_find_member(dw_type_t *dtp, char *name)
{
	int ret;
	dw_type_t *member;
	dw_type_info_t *dti_typedef;
	type_t *t;

	/* Just to be safe...
	 */
	if (!dtp) {
		return((dw_type_t *)NULL);
	}

	/* Make sure that dtp references either a structure or union
	 * type.
	 */
	if ((dtp->dt_tag != DW_TAG_structure_type) && 
		(dtp->dt_tag != DW_TAG_union_type)) {

		if(dtp->dt_tag == DW_TAG_typedef) {
			/* If dtp references a typedef, determine the actual
			 * type and get the member offset from there.
			 */
			dti_typedef = (dw_type_info_t *)
				KLIB_ALLOC_BLOCK(K, sizeof(dw_type_info_t),C_B_TEMP);
	    
			if(dw_type_info(dtp->dt_off, dti_typedef, C_B_TEMP) == 
							   (dw_type_info_t *)NULL) {
				dw_free_type_info(dti_typedef);
				return (dw_type_t *)NULL;
			}
	    
			if((dti_typedef->t_actual_type == (dw_die_t *)NULL) ||
				(dti_typedef->t_actual_type->d_tag != DW_TAG_structure_type &&
				dti_typedef->t_actual_type->d_tag != DW_TAG_union_type) ||
				(t=dw_lkup_member(stp,dti_typedef,name,C_B_TEMP)) == 
				(type_t *)NULL) {

				dw_free_type_info(dti_typedef);
				return (dw_type_t *)NULL;
			}
	    
			member = dw_type((dw_type_info_t *)t->t_ptr,C_B_TEMP);
			
			dw_free_type_info((dw_type_info_t *)t->t_ptr);
			dw_free_type_info(dti_typedef);
			K_BLOCK_FREE(K)(NULL,t);
			return member;
		}
		return((dw_type_t *)NULL);
	}

	/* Check to see if the type has been completed. If it hasn't, then
	 * do that fist.
	 */
	if (dtp->dt_state == SYM_INIT) {
		if (dw_complete_type(dtp)) {
			return((dw_type_t *)NULL);
		}
	}

	/* Get the first member pointer
	 */
	if (!(member = (dw_type_t*)dtp->dt_member)) {
		return((dw_type_t *)NULL);
	}
	while (member) {
		if (member->dt_name && !strcmp(name, member->dt_name)) {
			return(member);
		}
		member = (dw_type_t *)member->dt_member;
	}
	return((dw_type_t *)NULL);
}

/*
 * dw_lkup_member()
 */
type_t *
dw_lkup_member(symtab_t *stp, dw_type_info_t *t, char *member, int flags)
{
	type_t *tp;
	dw_die_t *d;
	dw_type_info_t *dti;
	Dwarf_Off next_off;

	/* We should have a pointer to a struct or union type. If not, 
	 * return NULL.
	 */
	d = t->t_actual_type;
	if ((d->d_tag != DW_TAG_structure_type) && 
	    (d->d_tag != DW_TAG_union_type)) {
		return((type_t *)NULL);
	}

	/* Get the offset to the first member's die. Then cycle through 
	 * the linked members, looking for member. Note that in certain 
	 * circumstances it is possible for there to be a member that 
	 * does not have a name (e.g., see pte_t definition). In such
	 * cases, just skip over this member and go to the next.
	 */
	dti = (dw_type_info_t *)KLIB_ALLOC_BLOCK(K, sizeof(*dti), flags);
	next_off = d->d_sibling_off;
	while (next_off) {
		dw_type_info(next_off, dti, flags);
		if (dti->t_name && strcmp(dti->t_name, member) == 0) {
			tp = (type_t *)KLIB_ALLOC_BLOCK(K, sizeof(*tp), flags);
			tp->flag = DWARF_TYPE_FLAG;
			tp->t_ptr = (void *)dti;
			return(tp);
		}
		next_off = dti->t_member_off;
	}

	/* If member wasn't a member of struct/union ...
	 */
	dw_free_type_info(dti);
	return((type_t *)NULL);
}

/*
 * dw_var_to_type() -- Converts a variable die to a dw_type_info_t struct
 *
 *   of the appropriate type. All DW_TAG_pointer dies are converted to
 *   type_t structs with POINTER_FLAG set. The original dw_type_info_t
 *   struct and all associated structs are freed upon success (they
 *   are not freed on failure).
 */
type_t *
dw_var_to_type(dw_type_info_t *t, int flags)
{
	dw_die_t *dp;
	dw_type_info_t *dti;
	type_t *tp, *head = (type_t *)NULL, *last = (type_t *)NULL;

	/* Make sure this is a DW_TAG_variable
	 */
	if (t->t_tag != DW_TAG_variable) {
		return((type_t *)NULL);
	}

	dp = t->t_actual_type;
	while (dp && dp->d_tag == DW_TAG_pointer_type) {
		tp = (type_t *)KLIB_ALLOC_BLOCK(K, sizeof(type_t), flags);
		tp->flag = POINTER_FLAG;
		if (last) {
			last->t_next = tp;
			last = tp;
		}
		else {
			head = last = tp;
		}
		dp = dp->d_next;
	}

	/* If We step past all the pointer dies and don't point at anything,
	 * this must be a void pointer. Just return.
	 */
	if (!dp) {
		dw_free_type_info(t);
		return(head);
	}
	dti = (dw_type_info_t *)KLIB_ALLOC_BLOCK(K, sizeof(*dti), flags);
	switch (dp->d_tag) {

	case DW_TAG_structure_type :

		/* Check to see if this die points to the struct 
		 * members. If not, we need to search for the actual
		 * struct definition.
		 */
		if (!dp->d_sibling_off) {
			dw_type_t *dtp;

			if (!(dtp = dw_lkup(stp, dp->d_name, DW_TAG_structure_type))) {
				return((type_t *)NULL);
			}
			if (!dw_type_info(dtp->dt_off, dti, flags)) {
				return((type_t *)NULL);
			}
		}
		else {
			if (!dw_type_info(dp->d_off, dti, flags)) {
				return((type_t *)NULL);
			}
		}
		break;

	case DW_TAG_union_type :

		/* Check to see if this die points to the union 
		 * members. If not, we need to search for the actual
		 * union definition.
		 */
		if (!dp->d_sibling_off) {
			dw_type_t *dtp;

			if (!(dtp = dw_lkup(stp, dp->d_name, DW_TAG_union_type))) {
				return((type_t *)NULL);
			}
			if (!dw_type_info(dtp->dt_off, dti, flags)) {
				return((type_t *)NULL);
			}
		}
		else {
			if (!dw_type_info(dp->d_off, dti, flags)) {
				return((type_t *)NULL);
			}
		}
		break;

	default :
		if (!dw_type_info(dp->d_off, dti, flags)) {
			return((type_t *)NULL);
		}
		break;
	}

	/* Save the pointer to the info about the actual member (for
	 * use by dowhatis() )
	 */
	dti->t_link = t;

	tp = (type_t *)KLIB_ALLOC_BLOCK(K, sizeof(type_t), flags);
	tp->flag = DWARF_TYPE_FLAG;
	tp->t_ptr = (void *)dti;
	if (last) {
		last->t_next = tp;
	}
	else {
		head = tp;
	}
	return(head);
}

/*
 * dw_get_type()
 */
dw_type_t *
dw_get_type(Dwarf_Off die_off) 
{
	int type;
	Dwarf_Signed dres;
	Dwarf_Die die;
	Dwarf_Half tag;
	Dwarf_Unsigned size = 0;
	Dwarf_Error derr;
	char *die_name;
	dw_type_t *dtp;

	if (!(die = dw_die(die_off))) {
		return((dw_type_t *)NULL);
	}

	tag = dw_tag(die);
	switch (tag) {
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		dres = dwarf_bytesize(die, &size, &derr);
		if ((dres == DW_DLV_NO_ENTRY) || (dres == DW_DLV_ERROR) || !size) {
			return((dw_type_t *)NULL);
		}
		type = SYM_STRUCT_UNION;
		break;

	case DW_TAG_enumeration_type:
		type = SYM_ENUMERATION;
		break;

	case DW_TAG_variable:
		type = SYM_VARIABLE;
		break;

	case DW_TAG_typedef:
		type = SYM_TYPEDEF;
		break;

	default:
		return((dw_type_t *)NULL);
	}
	dres = dwarf_diename(die, &die_name, &derr);
	if ((dres == DW_DLV_NO_ENTRY) || (dres == DW_DLV_ERROR)) {
		return((dw_type_t *)NULL);
	}
	dtp = (dw_type_t*)malloc(sizeof(*dtp));
	bzero(dtp, sizeof(*dtp));
	dtp->dt_type = type;
	dtp->dt_state = SYM_INIT;
	dtp->dt_name = die_name;
	dtp->dt_tag = tag;
	dtp->dt_off = die_off;
	dtp->dt_size = size;
	return(dtp);
}

/* 
 * dw_complete_type() -- Fill in the remaining fields of the dw_type_t struct
 */
int
dw_complete_type(dw_type_t *dtp)
{
	dw_type_info_t *dti;

	dti = (dw_type_info_t *)klib_alloc_block(K, sizeof(*dti), B_TEMP);

	/* get all the type information
	 */
	if (!dw_type_info(dtp->dt_off, dti, 0)) {
		dw_free_type_info(dti);
		return(1);;
	}

	/* Fill in the rest of the fields in dw_type_t struct
	 */
	if (dti->t_type) {
		dtp->dt_type_off = dti->t_type->d_off;
	}
	dtp->dt_offset = dti->t_loc;

	if ((dti->t_tag == DW_TAG_typedef) && (!dti->t_type)) {
		dw_free_type_info(dti);
		return(1);
	}
	else if ((dtp->dt_tag == DW_TAG_structure_type) ||
		 (dtp->dt_tag == DW_TAG_union_type)) {

		/* Attach a linked list of member records.
		 */
		if (dw_populate_type(dtp)) {
			dw_free_type_info(dti);
			return(1);
		}
	}
	else if (dtp->dt_tag == DW_TAG_enumeration_type) {
		/* XXX - not sure what needs to be done here!
		 */
	}
	dtp->dt_state = SYM_COMPLETE;
	dw_free_type_info(dti);
	return(0);
}

/*
 * dw_init_types() -- Grab type info from the dwarf debug information.
 */
int
dw_init_types()
{
	int dres, i;
	dw_type_t *dtp;
	Dwarf_Error derr;
	Dwarf_Off die_off;
	Dwarf_Type *dtypebuf;
	Dwarf_Signed dcount;
	type_t *tp;

	/* Get the list of the user defined types. Any error here
	 * is fatal, since it means we won't be able to properly 
	 * initialize symbol table information.
	 */
	dres = dwarf_get_types(DBG, &dtypebuf, &dcount, &derr);
	if ((dres == DW_DLV_ERROR) || (dres == DW_DLV_NO_ENTRY)) {
		return(1);
	}

	for( i = 0 ; i < dcount ; i++ ) {

		/* get the die offset for this type
		 */
		dres = dwarf_type_die_offset(dtypebuf[i], &die_off, &derr);
		if (dres != DW_DLV_OK) {
			if (DEBUG(DC_DWARF, 1)) {
				fprintf(KL_ERRORFP, "d_type_nm_offs: %d\n", dres);
			}
			continue;
		}

		if (dtp = dw_get_type(die_off)) {

			/* Set the dbg flag to the current dbg
			 */
			dtp->dt_nmlist = curnmlist;

			/* Add the type to the list. If there is already an entry for
			 * this type, free the allocated memory and continue.
			 */
			if (dw_add_type(stp, dtp)) {
				dwarf_dealloc(DBG, dtp->dt_name, DW_DLA_STRING);
				free(dtp);
			}
		}
		dwarf_dealloc(DBG, dtypebuf[i], DW_DLA_TYPENAME);
	}

#ifdef DWARF_TEST
	dtp = (dw_type_t *)stp->st_struct;
	while(dtp) {
		fprintf(stderr, "left=0x%08x, right=0x%08x, ", 
			dtp->dt_left, dtp->dt_right);
		fprintf(stderr, "name=%s, state=%d, off=%d\n", 
			dtp->dt_name, dtp->dt_state, dtp->dt_off);
		dtp = dtp->dt_next;
	}
#endif

	dwarf_dealloc(DBG, dtypebuf, DW_DLA_LIST);
	return(0);
}

/*
 * dw_init_globals() -- Grab global info from the dwarf debug information.
 */
int
dw_init_globals()
{
	int dres, i;
	dw_type_t *dtp;
	Dwarf_Error derr;
	Dwarf_Off die_off;
	Dwarf_Global *globbuf;
	Dwarf_Signed gcount;

	/* get the list of globals
	 */
	dres = dwarf_get_globals(DBG, &globbuf, &gcount, &derr);
	if (dres == DW_DLV_ERROR) {
		return(1);
	}
	else if (dres == DW_DLV_NO_ENTRY) {
		return(1);
	}

	for(i = 0 ; i < gcount; i++) {

		/* get the die offset for this global
		 */
		dres = dwarf_global_die_offset(globbuf[i], &die_off, &derr);
		if (dres != DW_DLV_OK) {
			if (DEBUG(DC_DWARF, 2)) {
				fprintf(KL_ERRORFP, "dwarf_global_die_offset: %d\n",dres);
			}
			continue;
		}

		if (dtp = dw_get_type(die_off)) {

			/* Set the dbg flag to the current dbg
			 */
			dtp->dt_nmlist = curnmlist;

			/* Add the variable to the list. If there is already an entry for
			 * this variable name, free the allocated memory and continue.
			 */
			if (dw_add_variable(stp, dtp)) {
				dwarf_dealloc(DBG, dtp->dt_name, DW_DLA_STRING);
				free(dtp);
			}
		}
		dwarf_dealloc(DBG, globbuf[i], DW_DLA_GLOBAL);
	}

#ifdef DWARF_TEST
	dtp = (dw_type_t *)stp->st_variable;
	while(dtp) {
		fprintf(stderr, "left=0x%08x, right=0x%08x, ", 
			dtp->dt_left, dtp->dt_right);
		fprintf(stderr, "name=%s, state=%d, off=%d\n", 
			dtp->dt_name, dtp->dt_state, dtp->dt_off);
		dtp = dtp->dt_next;
	}
#endif
	dwarf_dealloc(DBG, globbuf, DW_DLA_LIST);
	return(0);
}

/*
 * dw_init_vars() -- Grab static var info from the dwarf debug information.
 */
void
dw_init_vars()
{
	int dres, i;
	dw_type_t *dtp;
	Dwarf_Error derr;
	Dwarf_Off die_off;
	Dwarf_Var *varbuf;
	Dwarf_Signed vcount;

	/* get the list of vars
	 */
	dres = dwarf_get_vars(DBG, &varbuf, &vcount, &derr);
	if (dres == DW_DLV_ERROR) {
		return;
	}
	else if (dres == DW_DLV_NO_ENTRY) {
		return;
	}

	for( i = 0 ; i < vcount ; i++ ) {

		/* get the die offset for this global
		 */
		dres = dwarf_var_die_offset(varbuf[i], &die_off, &derr);
		if (dres != DW_DLV_OK) {
			if (DEBUG(DC_DWARF, 2)) {
				fprintf(KL_ERRORFP, "dwarf_var_die_offset: %d\n",dres);
			}
			continue;
		}

		if (dtp = dw_get_type(die_off)) {

			/* Set the dbg flag to the current dbg
			 */
			dtp->dt_nmlist = curnmlist;

			/* Add the variable to the list. If there is already an entry for
			 * this variable name, free the allocated memory and continue.
			 */
			if (dw_add_variable(stp, dtp)) {
				dwarf_dealloc(DBG, dtp->dt_name, DW_DLA_STRING);
				free(dtp);
			}
		}
		dwarf_dealloc(DBG, varbuf[i], DW_DLA_VAR);
	}
	dwarf_dealloc(DBG, varbuf, DW_DLA_LIST);
	return;
}

/*
 * dw_struct()
 *
 */
int
dw_struct(char *sname)
{
	dw_type_t *dtp;

	if (sname) {
		if (!(dtp = dw_lkup(stp, sname, 0))) {
			return(0);
		}
		return(dtp->dt_size);
	}
	return(0);
}

/*
 * dw_field()
 */
int
dw_field(char *sname, char *member)
{
	dw_type_t *dtp;

	if (sname) {
		if (!(dtp = dw_lkup(stp, sname, 0))) {
			return(-1);
		}
		if (member) {
			if (!(dtp = dw_find_member(dtp, member))) {
				return(-1);
			}
		}
		return(dtp->dt_offset);
	}
	return(-1);
}

/*
 * dw_is_field()
 */
int
dw_is_field(char *sname, char *member)
{
	dw_type_t *dtp;

	if (dw_field(sname, member) == -1) {
		return(0);
	}
	else {
		return(1);
	}
}

/*
 * dw_field_sz()
 */
int
dw_field_sz(char *sname, char *member)
{
	dw_type_t *dtp;

	if (sname) {
		if (!(dtp = dw_lkup(stp, sname, 0))) {
			return(0);
		}
		if (member) {
			if (!(dtp = dw_find_member(dtp, member))) {
				return(0);
			}
		}
		return(dtp->dt_size);
	}
	return(0);
}

/*
 * dw_get_field()
 */
dw_type_t *
dw_get_field(char *sname, char *member)
{
	dw_type_t *dtp;

	if (sname) {
		if (!(dtp = dw_lkup(stp, sname, 0))) {
			return((dw_type_t *)NULL);
		}
		if (member) {
			if (!(dtp = dw_find_member(dtp, member))) {
				return((dw_type_t *)NULL);
			}
		}
		return(dtp);
	}
	return((dw_type_t *)NULL);
}

/*
 * dw_base_value() 
 *
 *    Return the value of a struct member properly aligned in v.
 *    If the field is not a base type (e.g., it's a struct, array, 
 *    etc), return -1 for failure.
 */
int
dw_base_value(k_ptr_t p, dw_type_t *dtp, k_uint_t *v)
{
	int res;
	k_uint_t val;
	dw_die_t *d;
	dw_type_info_t *dti;

	dti = (dw_type_info_t *)klib_alloc_block(K, sizeof(*dti), B_TEMP);

	if (!dw_type_info(dtp->dt_off, dti, 0)) {
		dw_free_type_info(dti);
		return(-1);
	}

	d = dti->t_actual_type;

	if (d->d_tag != DW_TAG_base_type) {
		dw_free_type_info(dti);
		return(-1);
	}

	if (d->d_encoding == DW_ATE_address) {
		val = kl_kaddr_val(K, p);
	}
	else if (dti->t_bit_size) {
		val = kl_get_bit_value(p, (int)dti->t_byte_size, 
				       (int)dti->t_bit_size, (int)dti->t_bit_offset);
	}
	else {
		switch (d->d_byte_size) {

		case 1:
			val = *(unsigned char*)p;
			break;

		case 2:
			val = *(short*)p;
			break;

		case 4:
			val = *(unsigned long*)p;
			break;

		case 8:
			val = *(k_uint_t*)p;
			break;

		default:
			break;
		}
	}
	*v = val;
	dw_free_type_info(dti);
	return(0);
}

/*
 * dw_free_current_module_dwarf_memory() -- 
 * 
 *   free the dwarf memory objects associated with the current module.
 *   Called when switching modules.
 */
void
dw_free_current_module_dwarf_memory(void)
{
	if (linebuf != NULL && linecount != 0) {
		int line_to_free;

		/* free previous line number information 
		 */
		for (line_to_free = 0; line_to_free < linecount; line_to_free++) {
			dwarf_dealloc (DBG, linebuf[line_to_free], DW_DLA_LINE);
		}

		dwarf_dealloc(DBG, linebuf, DW_DLA_LIST);
		linecount = 0;
	}
}

/*
 * dw_set_current_module()
 *
 *   Get cu info for loc. If loc is FROM current module, return NULL,
 *   if an error occurred; return -1; otherwise setup new current
 *   module and return new_cu_die.
 */
int
dw_set_current_module(kaddr_t loc)
{
	Dwarf_Arange arange;
	Dwarf_Error error;
	Dwarf_Unsigned cu_offset;
	Dwarf_Die new_cu_die;
	int ares;

	/* Check to see if loc is in current module
	 */
	if (current_module_arange_info != NULL) {
		Dwarf_Addr start;
		Dwarf_Unsigned length;
		Dwarf_Off cu_die_offset;
		int ares;

		ares = dwarf_get_arange_info(current_module_arange_info,
					     &start, &length, &cu_die_offset, &error);

		if (ares != DW_DLV_OK) {
			return(-1);
		}
		if (loc >= start && loc < start + length) {
			/* We don't have to do anything. We're still in the same 
			 * module as before.
			 */
			return(NULL);
		}
	}
	line_numbers_valid = 0;

	/* first, get a handle on the aranges and find out how many of them
	 * there are 
	 */
	if (aranges_get_info_for_loc == NULL) {
		int gares = dwarf_get_aranges(DBG, &aranges_get_info_for_loc,
					      &current_module_arange_count, &error);
		if (gares != DW_DLV_OK) {
			/* no aranges present, can't do much 
			 */
			return(-1);
		}
	}

	/* now try to find an arange that matches the PC given 
	 */
	ares = dwarf_get_arange(aranges_get_info_for_loc,
				current_module_arange_count, loc, &arange, &error);

	if (ares != DW_DLV_OK) {
		/* no match, can't get line number information for this PC 
		 */
		return(-1);
	}

	ares = dwarf_get_cu_die_offset(arange, &cu_offset, &error);
	if (ares == DW_DLV_ERROR) {
		/* can't get the compilation unit offset, something is amiss 
		 */
		if (DEBUG(DC_SYM, 1)) {
			fprintf(KL_ERRORFP, "dw_set_current_module: "
				"dwarf_get_cu_die_offset: could not get compilation "
				"unit offset! ares=%d, error=%d\n", ares, error);
		}
		return(-1);
	}
	if (ares ==  DW_DLV_NO_ENTRY) {
		/* compilation unit does not exist, no debug info here 
		 */
		return(-1);
	}

	/* get the compilation unit die 
	 */
	ares = dwarf_offdie(DBG, cu_offset, &new_cu_die, &error);
	if (ares != DW_DLV_OK) {
		new_cu_die = 0;
		/* can't read the compilation unit header, something amiss 
		 */
		if (DEBUG(DC_SYM, 1)) {
			fprintf(KL_ERRORFP, "dw_set_current_module: dwarf_offdie: "
				"could not get compilation unit offset! ares=%d, error=%d\n", 
				ares, error);
		}
		return(-1);
	}

	/* setup the new current module (free old current module resources).
	 */
	current_module_arange_info = arange;
	dwarf_dealloc(DBG, module_die, DW_DLA_DIE);
	dw_free_current_module_dwarf_memory();
	module_die = new_cu_die;
	return(1);
}

/*
 * dw_get_line()
 */
Dwarf_Line
dw_get_line(kaddr_t loc)
{
	Dwarf_Line line;
	Dwarf_Unsigned lineno, what_line;
	int pcres, lres, lires;
	kaddr_t pc;

	if (dw_set_current_module(loc) == -1) {
		return(NULL);
	}

	/* If loc was not from the current module, setup line number info
	 * for new module.
	 */
	if (line_numbers_valid == 0) {
		what_line = 0;
		lres = dwarf_srclines(module_die, &linebuf, &linecount, &err);
		if (lres != DW_DLV_OK) {
			linecount = 0;
			linebuf = 0;
			return(NULL);
		}
		line_numbers_valid = 1;
	}

	/* Get the line.
	 */
	what_line = 0;
	do {
		if (what_line >= linecount) {
			break;
		}
		line = linebuf[what_line++];
		pcres = dwarf_lineaddr(line, &pc, &err);
		if (pcres == DW_DLV_ERROR) {
			if (DEBUG(DC_SYM, 1)) {
				fprintf(KL_ERRORFP, "dw_get_line: dwarf_lineaddr: line=0x%xd, "
					"pcres=%d\n", line, pcres);
			}
			return(NULL);
		} 
		else if (pcres == DW_DLV_NO_ENTRY) {
			pc = 0;
		}
		if (pc > loc) {
			/* XXX - There has been a problem with certain
			 * boundray addresses (where loc == start of module).
			 * In these cases, line number information for the start 
			 * address of a function is not included in the linebuf[].
			 * If (what_line - 2) < 0, then just return linebuf[0].
			 * The line number might not be correct (it should be close),
			 * AND, at least it will prevent a BUSS ERROR. This is a
			 * real remote possibility but...
			 */
			if (what_line == 1) {
				line = linebuf[0];
				return(line);
			}
			line = linebuf[what_line - 2];
		}
	} while (loc > pc);
	return(line);
}

/*
 * dw_get_srcfile()
 */
char *
dw_get_srcfile(kaddr_t pc)
{
	Dwarf_Line line;
	int fres, newlen;
	char *filename, *src_file, *c;
	
	kl_reset_error();

	/* Get the line (and set the current module)
	 */
	if ((line = dw_get_line(pc)) == NULL) {
		KL_SET_ERROR_NVAL(KLE_BAD_FUNCADDR, pc, 2);
		return(NULL);
	}

	if (dw_set_current_module(pc) == -1) {
		KL_SET_ERROR_NVAL(KLE_BAD_FUNCADDR, pc, 2);
		return(NULL);
	}

	/* Get the filename for the current module
	 */
	fres = dwarf_linesrc(line, &filename, &err);
	if (fres != DW_DLV_OK) {
		KL_SET_ERROR_NVAL(KLE_BAD_FUNCADDR, pc, 2);
		return(NULL);
	}
	src_file = (char *)klib_alloc_block(K, strlen(filename) + 1, B_TEMP);
	strcpy(src_file, filename);
	dwarf_dealloc(DBG, filename, DW_DLA_STRING);

	/* If pathname starts at root ('/'), shorten it to include only
	 * those elements above the kern directory. The new truncated
	 * pathname string is moved to the beginning of the block.
	 */
	if ((src_file[0] == '/') && (c = strstr(src_file, "kern/"))) {
		c += 5;
		src_file[0] = '.';
		src_file[1] = '.';
		src_file[2] = '/';
		newlen = strlen(c);
		bcopy(c, src_file + 3, newlen);
		src_file[3 + newlen] = 0;
	}
	return(src_file);
}

/*
 * dw_get_lineno()
 */
int
dw_get_lineno(kaddr_t pc)
{
	Dwarf_Line line;
	Dwarf_Unsigned lineno;
	int lires;

	/* Get the line.
	 */
	if ((line = dw_get_line(pc)) == NULL) {
		return(NULL);
	}

	/* Now get the line number.
	 */
	lires = dwarf_lineno(line, &lineno, &err);
	if (lires != DW_DLV_OK) {
		lineno = 0;
	}
	return(lineno);
}

/*
 * dw_typestr()
 */
char *
dw_typestr(type_t *tp, int flags)
{
	int i, len = 0, ptrcnt = 0; 
	dw_type_info_t *dti;
	char *tsp, *c;
	type_t *t;

	t = tp;
	while (t->flag == POINTER_FLAG) {
		ptrcnt++;
		t = t->t_next;
	}
	dti = (dw_type_info_t *)t->t_ptr;

	/* We want the actaul type as defined in the kernel source -- not
	 * the real type (e.g., the typedef not the type).
	 */
	if (dti->t_link) {
		dti = dti->t_link;
	}

	/* If this is a type for a struct/union member, we have to make
	 * sure and decrement the ptrcnt so that the type string will
	 * have the correct number of '*' characters in it.
	 */
	if (dti->t_type->d_tag == DW_TAG_pointer_type) {
		ptrcnt--;
	}
	len = strlen(dti->t_type_str) + ptrcnt + 3;
	tsp = (char *)KLIB_ALLOC_BLOCK(K, len, flags);
	c = tsp;
	*c++ = '(';
	strcat(c, dti->t_type_str);
	c += strlen(dti->t_type_str);
	for (i = 0; i < ptrcnt; i++) {
		*c++ = '*';
	}
	*c++ = ')';
	*c = 0;
	return(tsp);
}
