/* union to take care of 32/64 ptr business */
typedef union {
  Elf32_Sym* ptr32;
  Elf64_Sym* ptr64;
} Elf_Sym_Ptr;

/* Symbol Table */
struct symbol_table {
  Elf_Sym_Ptr symtab_u;     /* ptr to symbol table entries */
  uint symtab_entry_no;     /* number of symbol table entries */
  uint symtab_c_index;      /* index into symtab that pts to first "C" symbol */
  char* strtab;             /* string table */
};

/* Globals */
extern struct symbol_table _core_symbol_table;   /* contains core's symbol table */
extern char* _module_gp_ptr;                     /* ptr to the argv[1] entry which will
						    contain the gp address of the module */

/* function prototypes */

/* loadelfsymtab.c */
int load_symtab32(CHAR *filename, struct symbol_table *st);
int load_symtab64(CHAR *filename, struct symbol_table *st);
int loadelf32_symtab(char *filename, ULONG fd, Elf32_Ehdr *elfhdr, struct symbol_table *st);
int loadelf64_symtab(char *filename, ULONG fd, Elf64_Ehdr *elfhdr, struct symbol_table *st);
int resolve_symbol32(struct symbol_table* st, char* symbol_name, Elf32_Addr* addr);
int resolve_symbol64(struct symbol_table* st, char* symbol_name, Elf64_Addr* addr);
void dump_symbol_table32(struct symbol_table *st);
void dump_symbol_table64(struct symbol_table *st);

/* loadelfdynamic.c */
LONG ide_loadelf32dynamic(char *filename, ULONG fd, ULONG *pc, Elf32_Ehdr *elfhdr, Elf32_Phdr *pgmhdr);
LONG ide_loadelf64dynamic(char *filename, ULONG fd, ULONG *pc, Elf64_Ehdr *elfhdr, Elf64_Phdr *pgmhdr);


