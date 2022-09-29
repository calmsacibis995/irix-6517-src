#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libsym/RCS/dwarfprint.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"

/*
 * dw_print_base_value()
 */
void
dw_print_base_value(k_ptr_t ptr, dw_type_info_t *dti, int flags, FILE *ofp)
{
	dw_die_t *d;

	d = dti->t_actual_type;
	if (d->d_tag != DW_TAG_base_type) {
		return;
	}

	if (dti->t_encoding == DW_ATE_address) {
		dw_print_pointer(ptr, dti, flags, ofp);
	}
	else if (dti->t_bit_size) {
		print_bit_value(ptr, (int)dti->t_byte_size, 
				(int)dti->t_bit_size, (int)dti->t_bit_offset, flags, ofp);
	}
	else {
		print_base(ptr, (int)d->d_byte_size, (int)d->d_encoding, flags, ofp);
	}
}

/*
 * dw_print_base_type()
 */
void
dw_print_base_type(k_ptr_t ptr, dw_type_info_t *dti, int flags, FILE *ofp)
{
	if (ptr) {
		if (!(flags & SUPPRESS_NAME))  {
			fprintf (ofp, "%s = ", dti->t_name);
		}
		dw_print_base_value(ptr, dti, flags, ofp);
		if (!(flags & SUPPRESS_NL)) {
			fprintf(ofp, "\n");
		}
	}
	else {
		if (dti->t_bit_size) {
			if (dti->t_name) {
				fprintf (ofp, "%s %s :%lld;\n", 
					 dti->t_type_str, dti->t_name, dti->t_bit_size);
			}
			else {
				fprintf (ofp, "%s :%lld;\n", dti->t_type_str, dti->t_bit_size);
			}
		}
		else {
			if (dti->t_name) {
				fprintf (ofp, "%s %s;\n", dti->t_type_str, dti->t_name);
			}
			else {
				fprintf (ofp, "%s;\n", dti->t_type_str);
			}
		}
	}
}

/*
 * dw_print_subroutine_type()
 */
void
dw_print_subroutine_type(k_ptr_t ptr, dw_type_info_t *dti, int flags, FILE *ofp)
{
	int len;
	char *c, typename[128];

	if (ptr) {
		fprintf(ofp, "%s ( %s)();", dti->t_type_str, dti->t_name);
		if (!(flags & SUPPRESS_NL)) {
			fprintf(ofp, "\n");
		}
	}
	else {
		if ((flags & SUPPRESS_NAME) == 0) {
			fprintf(ofp, "%s ( %s)();\n", dti->t_type_str, dti->t_name);
		}
	}
}


/*
 * dw_print_array_type()
 */
int
dw_print_array_type(k_ptr_t ptr, dw_type_info_t *dti, 
		    int level, int flags, FILE *ofp)
{
	int i, size, n = 0, m = 0;
	k_ptr_t p;
	dw_die_t *d;

	if (!(d = dti->t_actual_type)) {
		return(-1);
	}
	if (d->d_tag != DW_TAG_array_type) {
		return(-1);
	}

	if (d->d_sibling_off) {
		dw_die_t *d1;
		d1 = (dw_die_t *)klib_alloc_block(K, sizeof(*d1), B_TEMP);

		if (dw_die_info(d->d_sibling_off, d1, 0)) {
			if (d1->d_tag == DW_TAG_subrange_type) {
				n = d1->d_upper_bound + 1;
			}
			if (d1->d_sibling_off) {
				/* XXX - multi-dimensioned array? */
			}
		}
		klib_free_block(K, (k_ptr_t)d1);
	}

	/* Walk to the end of the die info list. This should be the actual 
	 * array element type (e.g., DW_TAG_base_type, DW_TAG_structure_type, 
	 * DW_TAG_union_type, etc.)
	 */
	while (d->d_next) {
		d = d->d_next;
		if (d->d_tag != DW_TAG_typedef) {
			break;
		}
	}
	dti->t_actual_type = d;

	/* If there wasn't an upward bounds, determine the number of elements
	 * by dividing the total byte size by the size of the element type.
	 */
	if (!n) {
		if (d->d_tag == DW_TAG_pointer_type) {
			n = dti->t_byte_size / K_NBPW(K);
		}
		else {
			n = dti->t_byte_size / d->d_byte_size;
		}
	}

	if (ptr) {

		p = ptr;

		if ((d->d_byte_size == 1) && !strcmp(dti->t_type_str, "char")) {
			if (flags & SUPPRESS_NAME) {
				fprintf(ofp, "\"");
				flags &= ~SUPPRESS_NAME;
			}
			else {
				fprintf(ofp, "%s = \"", dti->t_name);
			}
			for (i = 0; i < n; i++) {
				fprintf(ofp, "%c", *(char *)p);
				p = (k_ptr_t)((unsigned)p + d->d_byte_size);
				if (*(char*)p == 0) {
					break;
				}
			}
			fprintf(ofp, "\"");
			if (!(flags & SUPPRESS_NL)) {
				fprintf(ofp, "\n");
			}
		}
		else {
			if (flags & SUPPRESS_NAME) {
				fprintf(ofp, "{\n");
				flags &= ~SUPPRESS_NAME;
			}
			else {
				fprintf(ofp, "%s = {\n", dti->t_name);
			}

			if (d->d_tag == DW_TAG_pointer_type) {
				size = K_NBPW(K);
			}
			else {
				size = d->d_byte_size;
			}
			for (i = 0; i < n; i++) {

				LEVEL_INDENT(level + 1);
				fprintf(ofp, "[%d] ", i);

				switch (d->d_tag) {
				case DW_TAG_pointer_type :
					dw_print_pointer(p, dti, flags, ofp);
					break;

				case DW_TAG_base_type:
					dw_print_base_value(p, dti, flags, ofp);
					fprintf(ofp, "\n");
					break;

				case DW_TAG_union_type:
				case DW_TAG_structure_type:
					dw_dump_struct(p, dti, level + 1, flags|NO_INDENT, ofp);
					break;

				default:
					dw_print_base_value(p, dti, flags, ofp);
					fprintf(ofp, "\n");
					break;
				}
				p = (k_ptr_t)((uint)p + size);
			}
			LEVEL_INDENT(level);
			fprintf(ofp, "}");
			if (!(flags & SUPPRESS_NL)) {
				fprintf(ofp, "\n");
			}
		}
	}
	else if (d->d_tag == DW_TAG_pointer_type) {
		dw_print_pointer_type(ptr, dti, flags, ofp);
	}
	else {
		if (n) {
			fprintf (ofp, "%s %s[%d];\n", dti->t_type_str, dti->t_name, n);
		}
		else {
			fprintf (ofp, "%s %s[];\n", dti->t_type_str, dti->t_name);
		}
	}
	return(0);
}

/*
 * dw_print_enumeration_type()
 */
void
dw_print_enumeration_type(k_ptr_t ptr, dw_type_info_t *dti, 
			  int flags, FILE *ofp)
{
	int n;
	k_uint_t val;
	Dwarf_Off child_off;
	dw_die_t *d, *d1;

	d = dti->t_actual_type;
	d1 = (dw_die_t *)klib_alloc_block(K, sizeof(dw_die_t), B_TEMP);

	if (ptr) {
		if ((flags & SUPPRESS_NAME) == 0) {
			fprintf(ofp, "%s = ", dti->t_name);
		}
		child_off = d->d_sibling_off;
		while (child_off) {
			dw_die_info(child_off, d1, 0);
			if (d->d_byte_size == 1) {
				if (*(char*)ptr == d1->d_const) {
					break;
				}
			}
			else if (d->d_byte_size == 2) {
				if (*(short*)ptr == d1->d_const) {
					break;
				}
			}
			else if (d->d_byte_size == 4) {
				if (*(uint*)ptr == d1->d_const) {
					break;
				}
			}
			else {
				if (*(unsigned long long*)ptr == d1->d_const) {
					break;
				}
			}
			child_off = d1->d_sibling_off;
		}

		/* If the value matches one of the constant values, print out the
		 * constant name and the value. Otherwise, just print out the
		 * value.
		 */
		if (child_off) {
			fprintf(ofp, "(%s=%lld)\n", d1->d_name, d1->d_const);
		}
		else {
			switch (d->d_byte_size) {
			case 1:
				val = *(char*)ptr;
				break;

			case 2:
				val = *(short*)ptr;
				break;

			case 4:
				val = *(int*)ptr;
				break;

			case 8:
				val = *(long long*)ptr;
				break;
			}
			fprintf(ofp, "(%lld=BADVAL)\n", val);
		}
	}
	else {
		fprintf (ofp, "%s {", dti->t_type_str);
		child_off = d->d_sibling_off;
		while (child_off) {
			dw_die_info(child_off, d1, 0);
			fprintf(ofp, "%s = %lld", d1->d_name, d1->d_const);
			if (child_off = d1->d_sibling_off) {
				fprintf(ofp, ", ");
			}
		}
		if (strcmp(dti->t_type_str, dti->t_name)) {
			fprintf(ofp, "} %s;", dti->t_name);
		}
		else {
			fprintf(ofp, "};");
		}
		if (!(flags & SUPPRESS_NL)) {
			fprintf(ofp, "\n");
		}
	}
	dw_free_die(d1);
}

/*
 * dw_print_pointer()
 */
void
dw_print_pointer(k_ptr_t ptr, dw_type_info_t *t, int flags, FILE *ofp)
{
	kaddr_t addr;
	dw_die_t *d;

	d = t->t_actual_type;

	if (PTRSZ64(K)) {
		addr = *(unsigned long long *)ptr;
	}
	else {
		addr = *(unsigned long *)ptr;
	}
	if (addr) {
		fprintf(ofp, "0x%llx", addr);
		if (d->d_next && (d->d_next->d_tag == DW_TAG_base_type) &
		    (d->d_next->d_byte_size == 1) &&
		    (!strcmp(t->t_type_str, "char*"))) {
			fprintf(ofp, " = \"");
			dump_string(addr, 0, ofp);
			fprintf(ofp, "\"");
		}
	}
	else {
		fprintf(ofp, "(nil)");
	}
	if (!(flags & SUPPRESS_NL)) {
		fprintf(ofp, "\n");
	}
}


/*
 * dw_print_pointer_type()
 */
void
dw_print_pointer_type(k_ptr_t ptr, dw_type_info_t *dti, int flags, FILE *ofp)
{
	if (ptr) {
		if ((flags & SUPPRESS_NAME) == 0) {
			fprintf (ofp, "%s = ", dti->t_name);
		}
		dw_print_pointer(ptr, dti, flags, ofp);
	}
	else {
		int len;
		char *c, typename[128];
		dw_die_t *d;

		d = dti->t_actual_type;
		while (d) {
			if (d->d_tag != DW_TAG_pointer_type) {
				break;
			}
			d = d->d_next;
		}
		if (d && d->d_tag == DW_TAG_subroutine_type) {
			c = strchr(dti->t_type_str, '*');
			len = (int)(c - dti->t_type_str);
			strncpy(typename, dti->t_type_str, len);
			typename[len] = 0;
			fprintf(ofp, "%s (%s %s)();\n", typename, c, dti->t_name);
		}
		else {
			fprintf (ofp, "%s %s;\n", dti->t_type_str, dti->t_name);
		}
	}
}

/*
 * dw_print_typedef()
 */
void
dw_print_typedef(k_ptr_t ptr, dw_type_info_t *dti, 
		 int level, int flags, FILE *ofp)
{
	dw_die_t *d;

	d = dti->t_actual_type;

	if (ptr) {
		if (d->d_tag == DW_TAG_pointer_type) {
			LEVEL_INDENT(level);
			dw_print_pointer_type(ptr, dti, flags, ofp);
			return;
		}
		else {
			switch (d->d_tag) {
			case DW_TAG_base_type:
				LEVEL_INDENT(level);
				dw_print_base_type(ptr, dti, flags, ofp);
				break;

			case DW_TAG_union_type:
			case DW_TAG_structure_type:
				dw_dump_struct(ptr, dti, level, flags, ofp);
				break;

			case DW_TAG_array_type:
				LEVEL_INDENT(level);
				dw_print_array_type(ptr, dti, level, flags, ofp);
				break;

			case DW_TAG_enumeration_type:
				LEVEL_INDENT(level);
				dw_print_enumeration_type(ptr, dti, flags, ofp);
				break;

			default:
				LEVEL_INDENT(level);
				dw_print_base_type(ptr, dti, flags, ofp);
				break;
			}
		}
	}
	else {
		LEVEL_INDENT(level);
		if (level == 0) {
			if (dti->t_tag == DW_TAG_typedef) {
				fprintf(ofp, "typedef ");
			}
			switch (d->d_tag) {

			case DW_TAG_union_type:
			case DW_TAG_structure_type:
				dw_dump_struct(ptr, dti, level, flags, ofp);
				break;

			default:
				dw_print_base_type(ptr, dti, flags, ofp);
			}
		}
		else {
			dw_print_base_type(ptr, dti, flags, ofp);
		}
	}
}

/*
 * dw_print_type()
 */
void
dw_print_type(k_ptr_t buf, dw_type_info_t *dti, int level, int flags, FILE *ofp)
{
	int i, f;
	k_ptr_t ptr;
	dw_die_t *d;
	dw_type_info_t *rdti;

	if (buf) {
		if (dti->t_tag == DW_TAG_member) {
			ptr = (k_ptr_t)((unsigned)buf + dti->t_loc);
		}
		else {
			ptr = buf;
		}
	}
	else {
		ptr = 0;
	}

	if (!ptr && (rdti = dti->t_link)) {
		if ((rdti->t_tag == DW_TAG_member) 
		    && (rdti->t_type->d_tag == DW_TAG_typedef)) {
			fprintf(ofp, "%s %s;", rdti->t_type_str, rdti->t_name);
			if (!(flags & SUPPRESS_NL)) {
				fprintf(ofp, "\n");
			}
			return;
		}
	}
	else {
		rdti = dti;
	}

	/* Turn off the SUPPRESS_NL flag if it's set. We have to leave it
	 * in the original flags parameter so that it will have effect
	 * when we reach the end of this routine (any multi-element types,
	 * like structs, that are displayed below this point will need to
	 * have newlines placed at the end of lines.
	 */
	f = flags & (~SUPPRESS_NL);

	d = dti->t_type;

	switch (d->d_tag) {

	case DW_TAG_typedef :
		dw_print_typedef(ptr, rdti, level, f, ofp);
		break;

	case DW_TAG_structure_type :
	case DW_TAG_union_type :
		dw_dump_struct(ptr, rdti, level, f, ofp);
		break;

	case DW_TAG_pointer_type :
		LEVEL_INDENT(level);
		dw_print_pointer_type(ptr, rdti, f, ofp);
		break;
	
	case DW_TAG_subroutine_type :
		LEVEL_INDENT(level);
		dw_print_subroutine_type(ptr, rdti, f, ofp);
		break;

	case DW_TAG_array_type :
		LEVEL_INDENT(level);
		dw_print_array_type(ptr, rdti, level, f, ofp);
		break;

	case DW_TAG_enumeration_type :
		LEVEL_INDENT(level);
		dw_print_enumeration_type(ptr, rdti, f, ofp);
		break;

	case DW_TAG_base_type :
		LEVEL_INDENT(level);
		dw_print_base_type(ptr, rdti, f, ofp);
		break;

	default :
		LEVEL_INDENT(level);
		fprintf (ofp, "%s %s;", rdti->t_type_str, dti->t_name);
		if (!(flags & SUPPRESS_NL)) {
			fprintf(ofp, "\n");
		}
	}
}

/*
 * dw_dump_struct()
 */
void
dw_dump_struct(k_ptr_t buf, dw_type_info_t *t0, int level, int flags, FILE *ofp) 
{
	int i;
	dw_type_info_t *t1;
	Dwarf_Off next_off;

	if ((t0->t_actual_type->d_tag != DW_TAG_structure_type) &&
	    (t0->t_actual_type->d_tag != DW_TAG_union_type)) {
		return;
	}

	if (!(flags & NO_INDENT)) {
		LEVEL_INDENT(level);
	}
	if ((level == 0) || (flags & NO_INDENT)) {
		fprintf(ofp, "%s {\n", t0->t_type_str);
	}
	else {
		if (buf) {
			fprintf(ofp, "%s = %s {\n", t0->t_name, t0->t_type_str);
		}
		else {
			fprintf(ofp, "%s {\n", t0->t_type_str);
		}
	}

	/* If the NO_INDENT is set, we need to turn it off at this 
	 * point -- just in case we come across a member of this struct
	 * that is also a struct.
	 */
	if (flags & NO_INDENT) {
		flags &= ~(NO_INDENT);
	}

	/* Get the offset to the first member's die. Then cycle through 
	 * the linked members, printing out the values.
	 */
	next_off = t0->t_actual_type->d_sibling_off;

	t1 = (dw_type_info_t *)klib_alloc_block(K, sizeof(dw_type_info_t), B_TEMP);
	while (next_off) {
		dw_type_info(next_off, t1, 0);
		dw_print_type(buf, t1, (level + 1), flags, ofp);
		next_off = t1->t_member_off;
	}
	dw_free_type_info(t1);

	LEVEL_INDENT(level);
	if (buf) {
		if (level == 0) {
			fprintf(ofp, "}");
		}
		else {
			fprintf(ofp, "}\n");
		}
	}
	else {
		if (level == 0) {
			if ((t0->t_tag == DW_TAG_member) || (t0->t_tag == DW_TAG_typedef)) {
				fprintf(ofp, "} %s;\n", t0->t_name);
			}
			else {
				fprintf(ofp, "};\n");
			}
		}
		else {
			fprintf(ofp, "} %s;\n", t0->t_name);
		}
	}
}

/*
 * dw_dowhatis()
 */
void
dw_dowhatis(command_t cmd)
{
	int found = 0;
	Dwarf_Off off;
	Dwarf_Half tag;
	dw_type_t *dtp;
	dw_type_info_t *t;

	t = klib_alloc_block(K, sizeof(dw_type_info_t), B_TEMP);

	if (cmd.nargs) {

		if (dtp = dw_lkup(stp, cmd.args[0], DW_TAG_variable)) {
			dw_type_info(dtp->dt_off, t, 0);
			if (t->t_type_str) {
				fprintf(cmd.ofp, "%s %s;\n", t->t_type_str, t->t_name);
			}
			else {
				fprintf(cmd.ofp, "<UNKNOWN TYPE> %s;\n", t->t_name);
			}
			found++;
		}
		if (dtp = dw_lkup(stp, cmd.args[0], 0)) {
			dw_type_info(dtp->dt_off, t, 0);
			if (found) {
				fprintf(cmd.ofp, "---------------------------------------\n");
			}
			switch (dtp->dt_tag) {
			case DW_TAG_structure_type:
			case DW_TAG_union_type:
				dw_dump_struct((k_ptr_t)NULL, t, 0, cmd.flags, cmd.ofp);
				break;

			case DW_TAG_enumeration_type:
				fprintf(cmd.ofp, "%s is an DW_TAG_enumeration_type\n",
					dtp->dt_name);
				break;

			case DW_TAG_typedef:
				fprintf(cmd.ofp, "typedef %s %s;\n", 
					t->t_type_str, t->t_name);
				break;

			default:
				break;
			}
		}
		if (!found) {
			fprintf(cmd.ofp, "%s not found in symbol table!\n", cmd.args[0]);
		}
		dw_free_type_info(t);
		return;
	}

	if (cmd.flags & C_ALL) {

		fprintf(cmd.ofp, "STRUCT DEFINITIONS:\n\n");
		dtp = (dw_type_t *)stp->st_struct;

		while (dtp) {

			dw_type_info(dtp->dt_off, t, 0);
			if (cmd.flags & C_FULL) {
				dw_dump_struct((k_ptr_t)NULL, t, 0, cmd.flags, cmd.ofp);
				fprintf(cmd.ofp, "\n");
			}
			else {
				fprintf(cmd.ofp, "%s\n", t->t_type_str);
			}
			dtp = (dw_type_t *)dtp->dt_next;
		}

		fprintf(cmd.ofp, "\n\n");
		fprintf(cmd.ofp, "UNION DEFINITIONS:\n\n");
		dtp = (dw_type_t *)stp->st_union;

		while (dtp) {

			dw_type_info(dtp->dt_off, t, 0);
			if (cmd.flags & C_FULL) {
				dw_dump_struct((k_ptr_t)NULL, t, 0, cmd.flags, cmd.ofp);
				fprintf(cmd.ofp, "\n");
			}
			else {
				fprintf(cmd.ofp, "%s\n", t->t_type_str);
			}
			dtp = (dw_type_t *)dtp->dt_next;
		}

		fprintf(cmd.ofp, "\n\n");
		fprintf(cmd.ofp, "TYPEDEF DEFINITIONS:\n\n");
		dtp = (dw_type_t *)stp->st_typedef;

		while (dtp) {
			dw_type_info(dtp->dt_off, t, 0);
			fprintf(cmd.ofp, "typedef %s %s;\n", t->t_type_str, t->t_name);
			dtp = (dw_type_t *)dtp->dt_next;
		}

		fprintf(cmd.ofp, "\n\n");
		fprintf(cmd.ofp, "VARIABLE DEFINITIONS:\n\n");
		dtp = (dw_type_t *)stp->st_var_ptr;

		while (dtp) {
			dw_type_info(dtp->dt_off, t, 0);
			if (t->t_type_str) {
				fprintf(cmd.ofp, "%s %s;\n", t->t_type_str, t->t_name);
			}
			else {
				fprintf(cmd.ofp, "<UNKNOWN TYPE> %s;\n", t->t_name);
			}
			dtp = (dw_type_t *)dtp->dt_next;
		}
		return;
	}
}

/*
 * dw_print_struct()
 */
int
dw_print_struct(char *name, kaddr_t addr, k_ptr_t ptr, int flags, FILE *ofp)
{
	int size;
	dw_type_info_t *t;
	dw_die_t *d;
	dw_type_t *dtp;

	if (DEBUG(DC_FUNCTRACE, 3)) {
		fprintf (ofp, "dw_print_struct: name=%s, addr=0x%llx, ptr=0x%x, "
			 "flags=%d, ofp=0x%x\n", name, addr, ptr, flags, ofp);
	}

	kl_reset_error();

	if (!(dtp = dw_lkup(stp, name, 0))) {
		KL_SET_ERROR_CVAL(KLE_BAD_STRUCT, name);
		return(1);
	}

	if (dtp) {
		t = klib_alloc_block(K, sizeof(dw_type_info_t), B_TEMP);
		dw_type_info(dtp->dt_off, t, 0);
		if (!(size = t->t_byte_size)) {
			KL_SET_ERROR_CVAL(KLE_BAD_STRUCT, name);
			dw_free_type_info(t);
			return(1);
		}
		switch (t->t_tag) {

		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			fprintf(ofp, "%s at 0x%llx:\n", t->t_type_str, addr);
			dw_print_type(ptr, t, 0, flags, ofp);
			break;

		default:
			KL_SET_ERROR_CVAL(KLE_BAD_STRUCT, name);
			dw_free_type_info(t);
			return(1);
		}
	}
	dw_free_type_info(t);
	return(0);
}

