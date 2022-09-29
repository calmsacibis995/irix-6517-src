#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libmdebug/RCS/md_print.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <filehdr.h>
#include <fcntl.h>
#include <stdio.h>
#include <sym.h>
#include <symconst.h>
#include <syms.h>
#include <cmplrs/stsupport.h>
#include <elf_abi.h>
#include <elf_mips.h>
#include <libelf.h>
#include <sex.h>
#include <ldfcn.h>
#include <errno.h>

#include "icrash.h"
#include "eval.h"
#include "libmdebug.h"

extern int errno;
extern int debug;
extern int proc_cnt;
extern int mbtsize;
extern mtbarray_t mbtarray[];
extern mdebugtab_t *mdp;

/*
 * Name: md_print_types()
 * Func: Print out all types on the mdebugtab_t list.  This is mostly
 *       for debugging purposes.
 */
int
md_print_types(mdebugtab_t *md)
{
	FILE *tfp;
	md_type_t *mt, *mt2;
	md_type_info_t *mi;
	md_type_info_sub_t *mir;

	tfp = fopen("md_output", "w");
	for (mt = md->m_struct; mt; mt = mt->m_next) {
		mi = mt->m_type;
		if (MD_VALID_STR(mt->m_name)) {
			fprintf(tfp, "STRUCT/UNION (file:%d, isym:%d, size:%d): %s\n",
				mt->m_ifd, mt->m_isym, mi->m_size, mt->m_name);
		} else {
			fprintf(tfp, "STRUCT/UNION (file:%d, isym:%d, size:%d):\n",
				mt->m_ifd, mt->m_isym, mi->m_size);
		}
		if (mt->m_member) {
			for (mt2 = mt->m_member; mt2; mt2 = mt2->m_member) {
				mi = mt2->m_type;
				fprintf(tfp, "    MEMBER (size:%d, offset:%d",
					mi->m_size, mi->m_offset);
				if (mir = md_is_range(mt2)) {
					fprintf(tfp, ", rangeifd:%d, rangeisym:%d",
						mir->mi_range_ifd, mir->mi_range_isym);
				}
				if (MD_VALID_STR(mt2->m_name)) {
					fprintf(tfp, "): %s\n", mt2->m_name);
				} else {
					fprintf(tfp, "):\n");
				}
			}
		}
	}
	fprintf(tfp, "\n\n");

	for (mt = md->m_enum; mt; mt = mt->m_next) {
		mi = mt->m_type;
		if (MD_VALID_STR(mt->m_name)) {
			fprintf(tfp, "ENUM (file:%d, isym:%d, size:%d): %s\n",
				mt->m_ifd, mt->m_isym, mi->m_size, mt->m_name);
		} else {
			fprintf(tfp, "ENUM (file:%d, isym:%d, size:%d):\n",
				mt->m_ifd, mt->m_isym, mi->m_size);
		}
		if (mt->m_member) {
			for (mt2 = mt->m_member; mt2; mt2 = mt2->m_member) {
				mi = mt2->m_type;
				fprintf(tfp, "    MEMBER (size:%d, offset:%d",
					mi->m_size, mi->m_offset);
				if (mir = md_is_range(mt2)) {
					fprintf(tfp, ", rangeifd:%d, rangeisym:%d",
						mir->mi_range_ifd, mir->mi_range_isym);
				}
				if (MD_VALID_STR(mt2->m_name)) {
					fprintf(tfp, "): %s\n", mt2->m_name);
				} else {
					fprintf(tfp, "):\n");
				}
			}
		}
	}
	fprintf(tfp, "\n\n");

	for (mt = md->m_typedef; mt; mt = mt->m_next) {
		mi = mt->m_type;
		fprintf(tfp, "TYPEDEF (file:%d, isym:%d, size:%d",
			mt->m_ifd, mt->m_isym, mi->m_size);
		fprintf(tfp, ", size:%d, offset:%d",
			mi->m_size, mi->m_offset);
		if (mir = md_is_range(mt)) {
			fprintf(tfp, ", rangeifd:%d, rangeisym:%d",
				mir->mi_range_ifd, mir->mi_range_isym);
		}
		fprintf(tfp, "): %s\n", mt->m_name);
	}
	fprintf(tfp, "\n\n");

	for (mt = md->m_variable; mt; mt = mt->m_next) {
		mi = mt->m_type;
		fprintf(tfp,
			"VARIABLE: (file:%d, isym:%d, pc:0x%x): %s\n",
			mt->m_ifd, mt->m_isym, mi->m_pc, mt->m_name);
	}
	fprintf(tfp, "\n\n");

	for (mt = md->m_proc; mt; mt = mt->m_next) {
		mi = mt->m_type;
		fprintf(tfp,
			"PROC: (file:%d, isym:%d, size:%d, "
			"lowpc:0x%x, highpc:0x%x): %s\n",
			mt->m_ifd, mt->m_isym, mi->m_size,
			mi->m_func_lowpc, mi->m_func_highpc, mt->m_name);
	}
	fprintf(tfp, "\n\n");

	fclose(tfp);
}

int
md_print_bit_value(i_ptr_t buf, int bit_size, int offset, int flags, FILE *ofp)
{
	i_uint_t val;

	bcopy(buf, &val, sizeof(i_uint_t));
	val <<= offset % 8;
	val >>= (64 - bit_size);
	if (flags & C_HEX) {
		fprintf(ofp, "0x%llx", val);
	} else if (flags & C_OCTAL) {
		fprintf(ofp, "0%llo", val);
	} else {
		fprintf(ofp, "%lld", val);
	}
}

int
md_print_base_type(i_ptr_t buf, md_type_t *md, int flags, FILE *ofp)
{
	unsigned long val = 0;
	i_uint_t longval = 0;

	switch ((int)(md->m_type->m_size / 8)) {
		case 1:
			val = *(unsigned char *)buf;
			break;
		case 2:
			val = *(short *)buf;
			break;
		case 4:
			val = *(unsigned long *)buf;
			break;
		case 8:
			longval = *(i_uint_t *)buf;
			if (flags & C_HEX) {
				fprintf(ofp, "0x%llx", longval);
			} else if (flags & C_OCTAL) {
				fprintf(ofp, "0%llo", longval);
			} else {
				fprintf(ofp, "%lld", longval);
			}
			return (1);
	}
	if (flags & C_HEX) {
		fprintf(ofp, "0x%x", val);
	} else if (flags & C_OCTAL) {
		fprintf(ofp, "0%o", val);
	} else {
		fprintf(ofp, "%d", val);
	}
	return (1);
}

int
md_print_base_value(i_ptr_t buf, md_type_t *md, int flags, FILE *ofp)
{
	md_type_info_sub_t *mi;

	if (md_is_pointer(md)) {
		md_print_pointer(buf, md, flags, ofp);
	} else if (mi = md_is_bit_field(md)) {
		md_print_bit_value(buf, mi->mi_bit_width,
			md->m_type->m_offset, flags, ofp);
	} else {
		md_print_base_type(buf, md, flags, ofp);
	}
}

int
md_print_base(i_ptr_t buf, md_type_t *md, int level, int flags, FILE *ofp)
{
	int i;
	md_type_info_sub_t *mi, *mib;

	if (buf) {
		if (!(flags & SUPRESS_NAME)) {
			fprintf(ofp, "%s = ", md->m_name);
		}
		md_print_base_value(buf, md, flags, ofp);
		fprintf(ofp, "\n");
	} else {
		mi = md_is_bt(md);
		if (mib = md_is_bit_field(md)) {
			i = mib->mi_bit_width;
			if (MD_VALID_STR(md->m_name)) {
				fprintf(ofp, "%s %s:%d;\n", mi->mi_type_name, md->m_name, i);
			} else {
				fprintf(ofp, "%s:%d;\n", mi->mi_type_name, i);
			}
		} else {
			if (MD_VALID_STR(md->m_name)) {
				fprintf(ofp, "%s %s;\n", mi->mi_type_name, md->m_name);
			} else {
				fprintf(ofp, "%s;\n", mi->mi_type_name);
			}
		}
	}
}

/*
 * Name: md_print_value()
 * Func: Print out a value based on the md passed in.
 */
int
md_print_value(kaddr_t k, md_type_t *md, int level, int flags, FILE *ofp)
{
	int size = 0, arrflag = 0;
	i_ptr_t ptr;
	md_type_info_sub_t *mi;

	size = md->m_type->m_size;
	if ((md->m_type->m_st_type != stStruct) &&
		(md->m_type->m_st_type != stUnion)) {
			if (mi = md_is_bt(md)) {
				if (mbtarray[mi->mi_bt_type].size >= 0) {
					size = (mbtarray[mi->mi_bt_type].size / 8);
				}
			} else {
				return (0);
			}
	}

	ptr = (i_ptr_t)malloc(size);
	if (!get_block(k, size, ptr, "md_value")) {
		free(ptr);
		return (0);
	}
	flags &= ~SUPRESS_NAME;
	if (md->m_type->m_st_type == stMember) {
		flags |= SUPRESS_OFFSET;
	}
	md_print_type(ptr, md, level, flags, ofp);
	return (1);
}

int
md_print_function_type(i_ptr_t buf, md_type_t *md, int flags, FILE *ofp)
{
	md_type_info_sub_t *mibt;

	mibt = md_is_bt(md);
	if (buf) {
		fprintf(ofp, "%s ( %s)();\n", mibt->mi_type_name, md->m_name);
	} else {
		if (!(flags & SUPRESS_NAME)) {
			fprintf(ofp, "%s ( %s)();\n", mibt->mi_type_name, md->m_name);
		}
	}
}

void
md_print_mi_array_loop(i_ptr_t buf, md_type_t *md, md_type_info_sub_t *mi,
	int offset, int level, int flags, FILE *ofp)
{
	char *str;
	i_ptr_t ptr;
	kaddr_t addr;
	md_type_info_sub_t *mibt, *mi2;
	int i, high, low, width, subarray = 0, noffset, bt;

	high = mi->mi_array_high;
	low = mi->mi_array_low;
	width = mi->mi_array_width / 8;

	if (mi->mi_sub_next) {
		for (mi2 = mi->mi_sub_next; mi2; mi2 = mi2->mi_sub_next) {
			if (mi2->mi_flags & MI_ARRAY) {
				subarray = 1;
			}
		}
	}

	mibt = md_is_bt(md);
	bt = mibt->mi_bt_type;
	if ((bt == btChar) || (bt == btUChar)) {
		fprintf(ofp, "%s = ", md->m_name);
		str = (char *)malloc(high - low + 2);
		bcopy(buf, str, high - low + 1);
		if (str[0] != '\0') {
			fprintf(ofp, "\"%s\"\n", str);
		} else {
			fprintf(ofp, "(nil)\n");
		}
		free(str);
	} else {
		if (!(flags & SUPRESS_NAME)) {
			fprintf(ofp, "%s = {\n", md->m_name);
			level++;
		}
		flags |= SUPRESS_NAME;
		for (i = low; i <= high; i++) {
			noffset = (i * width) + offset;
			ptr = (i_ptr_t)((unsigned)buf + (unsigned int)noffset);
			if (subarray) {
				LEVEL_INDENT(level);
				if ((bt == btStruct) || (bt == btUnion) || (bt == btEnum)) {
					fprintf(ofp, "[%d] %s {\n", i, mibt->mi_type_name);
				} else {
					fprintf(ofp, "[%d] {\n", i, mibt->mi_type_name);
				}
				md_print_mi_array_loop(ptr, md, mi->mi_sub_next, noffset,
					(level + 1), flags, ofp);
				if (!(flags & SUPRESS_NAME)) {
					LEVEL_INDENT(level);
					fprintf(ofp, "}\n");
				}
			} else {
				LEVEL_INDENT(level);
				fprintf(ofp, "[%d] = ", i);
				md_print_base(ptr, md, (level + 1), flags, ofp);
			}
		}
		level--;
		LEVEL_INDENT(level);
		fprintf(ofp, "}\n");
	}
}

void
md_print_mi_array(md_type_t *md, md_type_info_sub_t *mi, FILE *ofp)
{
	md_type_info_sub_t *mi2;

	if (!mi) {
		mi2 = md->m_type->m_type_sub;
	} else {
		mi2 = mi;
	}

	for (; mi2; mi2 = mi2->mi_sub_next) {
		if (mi2->mi_flags & MI_ARRAY) {
			fprintf(ofp, "[%d]",
				(mi2->mi_array_high - mi2->mi_array_low + 1));
		}
	}
}

int
md_print_array_type(i_ptr_t buf, md_type_t *md,
	int level, int flags, FILE *ofp)
{
	md_type_info_sub_t *mibt, *mi;

	if (buf) {
		md_print_mi_array_loop(buf, md,
			md_is_array(md), 0, level, flags, ofp);
	} else {
		if (md_is_pointer(md)) {
			md_print_pointer_type(buf, md, flags, ofp);
		} else {
			mibt = md_is_bt(md);
			fprintf(ofp, "%s %s", mibt->mi_type_name, md->m_name);
			md_print_mi_array(md, (md_type_info_sub_t *)NULL, ofp);
			fprintf(ofp, ";\n");
		}
	}
}

int
md_print_enum_flags(i_ptr_t buf, md_type_t *md, int flags, FILE *ofp)
{
	md_type_t *mdm;
	unsigned int value;

	value = *(unsigned int *)buf;
	for (mdm = md->m_member; mdm; mdm = mdm->m_member) {
		if (value == mdm->m_type->m_offset) {
			if (MD_VALID_STR(mdm->m_name)) {
				fprintf(ofp, " (%s)", mdm->m_name);
				return (1);
			}
		}
	}
	return (0);
}

int
md_print_enum(i_ptr_t buf, md_type_t *md, md_type_t *md2,
	int flags, FILE *ofp)
{
	md_type_t *mdm;
	md_type_info_sub_t *mibt;

	if (buf) {
		if (!(flags & SUPRESS_NAME)) {
			if ((md2) && (MD_VALID_STR(md2->m_name))) {
				fprintf(ofp, "%s = ", md2->m_name);
				print_base(buf, sizeof(unsigned),
					(md->m_type->m_offset / 8), flags, ofp);
				md_print_enum_flags(buf, md, flags, ofp);
				fprintf(ofp, "\n");
			}
		}
	} else {
		fprintf(ofp, "enum %s { ", md->m_name);
		for (mdm = md->m_member; mdm; mdm = mdm->m_member) {
			fprintf(ofp, "%s = %d", mdm->m_name, mdm->m_type->m_offset);
			if (mdm->m_member != (md_type_t *)NULL) {
				fprintf(ofp, ", ");
			}
		}
		if ((md2) && (MD_VALID_STR(md2->m_name))) {
			fprintf(ofp, " } %s;\n", md2->m_name);
		} else {
			fprintf(ofp, " };\n");
		}
	}
}

int
md_print_pointer(i_ptr_t buf, md_type_t *md, int flags, FILE *ofp)
{
	kaddr_t addr;
	md_type_info_sub_t *mibt;

	if (addr = *(unsigned long *)buf) {
		fprintf(ofp, "0x%llx", addr);
		mibt = md_is_bt(md);
		if ((!strcmp("char", mibt->mi_type_name)) && (md_is_pointer(md))) {
			fprintf(ofp, " = \"");
			dump_string(addr, 0, ofp);
			fprintf(ofp, "\"");
		}
	} else {
		fprintf(ofp, "(nil)");
	}
}

int
md_print_pointer_type(i_ptr_t buf, md_type_t *md, int flags, FILE *ofp)
{
	int i, j;
	char type[128];
	md_type_info_sub_t *mibt, *mi;

	bzero(type, 128);
	mibt = md_is_bt(md);
	if (buf) {
		if (!(flags & SUPRESS_NAME)) {
			fprintf(ofp, "%s = ", md->m_name);
		}
		md_print_pointer(buf, md, flags, ofp);
		fprintf(ofp, "\n");
	} else if (md_is_function(md)) {
		j = md_is_pointer(md);
		sprintf(type, "%s (", mibt->mi_type_name);
		for (i = 0; i < j; i++) {
			strcat(type, "*");
		}
		fprintf(ofp, "%s %s)();\n", type, md->m_name);
	} else {
		j = md_is_pointer(md);
		sprintf(type, "%s", mibt->mi_type_name);
		for (i = 0; i < j; i++) {
			strcat(type, "*");
		}
		if (md_is_array(md)) {
			fprintf(ofp, "%s %s", type, md->m_name);
			md_print_mi_array(md, (md_type_info_sub_t *)NULL, ofp);
			fprintf(ofp, ";\n");
		} else {
			fprintf(ofp, "%s %s;\n", type, md->m_name);
		}
	}
}

int
md_print_typedef(i_ptr_t buf, md_type_t *md, int level, int flags, FILE *ofp)
{
	int type;

	if (buf) {
		if (md_is_pointer(md)) {
			LEVEL_INDENT(level);
			md_print_pointer_type(buf, md, flags, ofp);
			return;
		} else {
			type = md->m_type->m_st_type;
			if ((type == stStruct) || (type == stUnion)) {
				md_dump_struct(buf, md, (md_type_t *)NULL, level, flags, ofp);
			} else if (md_is_array(md)) {
				md_print_array_type(buf, md, level, flags, ofp);
				LEVEL_INDENT(level);
			} else if (type == stEnum) {
				md_print_enum(buf, md, (md_type_t *)NULL, flags, ofp);
				LEVEL_INDENT(level);
			} else {
				LEVEL_INDENT(level);
				md_print_base(buf, md, level, flags, ofp);
			}
		}
	} else {
		LEVEL_INDENT(level);
		md_print_base(buf, md, level, flags, ofp);
	}
}

int
md_print_type(i_ptr_t buf, md_type_t *md, int level, int flags, FILE *ofp)
{
	int type;
	i_ptr_t ptr;
	md_type_info_sub_t *mibt;

	if (buf) {
		if ((md->m_type->m_st_type == stMember) &&
			(!(flags & SUPRESS_OFFSET))) {
				ptr = (i_ptr_t)((unsigned)buf +
					(unsigned int)md->m_type->m_offset / 8);
		} else {
			ptr = buf;
		}
	} else {
		ptr = 0;
	}

	type = md->m_type->m_st_type;
	if (type == stTypedef) {
		md_print_typedef(ptr, md, level, flags, ofp);
	} else if ((type == stStruct) || (type == stUnion)) {
		md_dump_struct(ptr, md, (md_type_t *)NULL, level, flags, ofp);
	} else if (md_is_pointer(md)) {
		LEVEL_INDENT(level);
		md_print_pointer_type(ptr, md, flags, ofp);
	} else if (md_is_function(md)) {
		LEVEL_INDENT(level);
		md_print_function_type(ptr, md, flags, ofp);
	} else if (md_is_array(md)) {
		LEVEL_INDENT(level);
		md_print_array_type(ptr, md, level, flags, ofp);
	} else if (type == stEnum) {
		LEVEL_INDENT(level);
		md_print_enum(ptr, md, (md_type_t *)NULL, flags, ofp);
	} else if (mibt = md_is_bt(md)) {
		if ((mibt->mi_bt_type == btStruct) || (mibt->mi_bt_type == btUnion)) {
			md_dump_struct(ptr, mibt->mi_type_ptr, md, level, flags, ofp);
		} else if (mibt->mi_bt_type == btEnum) {
			LEVEL_INDENT(level);
			md_print_enum(ptr, mibt->mi_type_ptr, md, flags, ofp);
		} else {
			LEVEL_INDENT(level);
			md_print_base(ptr, md, level, flags, ofp);
		}
	} else {
		LEVEL_INDENT(level);
		md_print_base(ptr, md, level, flags, ofp);
	}
}

int
md_dump_struct(i_ptr_t buf, md_type_t *md, md_type_t *md2,
	int level, int flags, FILE *ofp)
{
	md_type_t *mdm;

	if (!(flags & NO_INDENT)) {
		LEVEL_INDENT(level);
	}

	if ((!level) || (flags & NO_INDENT)) {
		if (MD_VALID_STR(md->m_name)) {
			fprintf(ofp, "%s %s {\n",
				(md->m_type->m_st_type == stStruct ? "struct" : "union"),
				md->m_name);
		} else {
			fprintf(ofp, "%s {\n",
				(md->m_type->m_st_type == stStruct ? "struct" : "union"));
		}
	} else {
		if (buf) {
			if ((md2) && MD_VALID_STR(md2->m_name)) {
				fprintf(ofp, "%s = %s {\n", md2->m_name,
					(md->m_type->m_st_type == stStruct ? "struct" : "union"));
			} else if (MD_VALID_STR(md->m_name)) {
				fprintf(ofp, "%s = %s {\n", md->m_name,
					(md->m_type->m_st_type == stStruct ? "struct" : "union"));
			} else {
				fprintf(ofp, "%s {\n",
					(md->m_type->m_st_type == stStruct ? "struct" : "union"));
			}
		} else {
			if (MD_VALID_STR(md->m_name)) {
				fprintf(ofp, "%s %s {\n",
					(md->m_type->m_st_type == stStruct ? "struct" : "union"),
					md->m_name);
			} else {
				fprintf(ofp, "%s {\n",
					(md->m_type->m_st_type == stStruct ? "struct" : "union"));
			}
		}
	}

	if (flags & NO_INDENT) {
			flags &= ~NO_INDENT;
	}

	for (mdm = md->m_member; mdm; mdm = mdm->m_member) {
		md_print_type(buf, mdm, (level + 1), flags, ofp);
	}

	LEVEL_INDENT(level);
	if (buf) {
		fprintf(ofp, "}\n");
	} else {
		if ((!level) || (!md2) || (!MD_VALID_STR(md2->m_name))) {
			fprintf(ofp, "};\n");
		} else {
			fprintf(ofp, "} %s;\n", md2->m_name);
		}
	}
}
