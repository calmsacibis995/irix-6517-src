/*
 * @(#)toc.h	1.1 88/06/08 4.0NFSSRC; from 1.4 87/09/21 D/NFS
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include "xdr_custom.h"
#define TOCMAGIC 0x674D2309
#define TOCVERSION 0

typedef char *stringp;
bool_t xdr_stringp();


typedef struct {
	u_int string_list_len;
	stringp *string_list_val;
} string_list;
bool_t xdr_string_list();


enum file_type {
	UNKNOWN = 0,
	TOC = 1,
	IMAGE = 2,
	TAR = 3,
	CPIO = 4,
};
typedef enum file_type file_type;
bool_t xdr_file_type();


enum media_type {
	TAPE = 0,
	DISK = 1,
	STREAM = 2,
};
typedef enum media_type media_type;
bool_t xdr_media_type();


struct tapeaddress {
	int volno;
	char *label;
	int file_number;
	int size;
	int volsize;
	char *dname;
};
typedef struct tapeaddress tapeaddress;
bool_t xdr_tapeaddress();


struct diskaddress {
	int volno;
	char *label;
	int offset;
	int size;
	int volsize;
	char *dname;
};
typedef struct diskaddress diskaddress;
bool_t xdr_diskaddress();


struct Address {
	media_type dtype;
	union {
		tapeaddress tape;
		diskaddress disk;
		struct nomedia junk;
	} Address_u;
};
typedef struct Address Address;
bool_t xdr_Address();


struct contents {
	char junk;
};
typedef struct contents contents;
bool_t xdr_contents();


struct standalone {
	string_list arch;
	u_int size;
};
typedef struct standalone standalone;
bool_t xdr_standalone();


struct executable {
	string_list arch;
	u_int size;
};
typedef struct executable executable;
bool_t xdr_executable();


struct amorphous {
	u_int size;
};
typedef struct amorphous amorphous;
bool_t xdr_amorphous();


struct Install_tool {
	string_list belongs_to;
	char *loadpoint;
	bool_t moveable;
	u_int size;
	u_int workspace;
};
typedef struct Install_tool Install_tool;
bool_t xdr_Install_tool();


struct Installable {
	string_list arch_for;
	string_list soft_depends;
	bool_t required;
	bool_t desirable;
	bool_t common;
	char *loadpoint;
	bool_t moveable;
	u_int size;
	char *pre_install;
	char *install;
};
typedef struct Installable Installable;
bool_t xdr_Installable();


enum file_kind {
	UNDEFINED = 0,
	CONTENTS = 1,
	AMORPHOUS = 2,
	STANDALONE = 3,
	EXECUTABLE = 4,
	INSTALLABLE = 5,
	INSTALL_TOOL = 6,
};
typedef enum file_kind file_kind;
bool_t xdr_file_kind();


struct Information {
	file_kind kind;
	union {
		contents Toc;
		amorphous File;
		standalone Standalone;
		executable Exec;
		Installable Install;
		Install_tool Tool;
		struct futureslop junk;
	} Information_u;
};
typedef struct Information Information;
bool_t xdr_Information();


struct distribution_info {
	char *Title;
	char *Source;
	char *Version;
	char *Date;
	int nentries;
	int dstate;
};
typedef struct distribution_info distribution_info;
bool_t xdr_distribution_info();


struct volume_info {
	Address vaddr;
};
typedef struct volume_info volume_info;
bool_t xdr_volume_info();


struct toc_entry {
	char *Name;
	char *LongName;
	char *Command;
	Address Where;
	file_type Type;
	Information Info;
};
typedef struct toc_entry toc_entry;
bool_t xdr_toc_entry();

