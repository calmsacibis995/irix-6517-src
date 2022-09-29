/* klib.c
 */
void free_klib(klib_t *);
int klib_check_dump_size(klib_t *);
int klib_utsname_init(klib_t *);
void klib_set_machine_type(klib_t *);
void klib_set_os_revision(klib_t *);
void klib_check_kernel_magic(klib_t *);
void klib_set_struct_sizes(klib_t *);
kaddr_t kl_sym_addr(klib_t *, char *);
kaddr_t kl_sym_pointer(klib_t *, char *);
int kl_struct_len(klib_t *, char *);
int kl_member_size(klib_t *, char *, char *);
int kl_member_offset(klib_t *, char *, char *);

/* klib_kthread.c
 */
int is_mapped_kern_ro(klib_t *, kaddr_t);
int is_mapped_kern_rw(klib_t *, kaddr_t);
int map_node_memory(klib_t *);
int addr_to_nasid(klib_t *, kaddr_t);
int addr_to_slot(klib_t *, kaddr_t);
int valid_physmem(klib_t *, kaddr_t);

/* klib_mem.c
 */
int map_node_memory(klib_t *);
int is_mapped_kern_ro(klib_t *, kaddr_t);
int is_mapped_kern_rw(klib_t *, kaddr_t);

/* klib_proc.c
 */
k_ptr_t get_lastproc(klib_t *, int, kaddr_t *, int *, int);
kaddr_t proc_slot_addr(klib_t *, kaddr_t, int, int *);
int proc_slot(klib_t *, k_ptr_t);

/* klib_util.c
 */
int in_block(char *, k_ptr_t);
k_ptr_t klib_alloc_block(klib_t *, int, int);
void klib_free_block(klib_t *, k_ptr_t);

