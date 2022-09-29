/*
 * Copyright (c) 1991, Sun Microsystems Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/nislib.h,v 1.2 1996/07/06 17:44:54 nn Exp $"

/*
 * This file contains the interfaces that are visible in the SunOS 5.x
 * implementation of NIS Plus. When using C++ the defined __cplusplus and
 * __STDC__ should both be true.
 */

#ifndef	_RPCSVC_NISLIB_H
#define	_RPCSVC_NISLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC__
extern void nis_freeresult(nis_result *);
extern nis_result * nis_lookup(nis_name, u_long);
extern nis_result * nis_list(nis_name, u_long,
	int (*)(nis_name, nis_object *, void *), void *);
extern nis_result * nis_add(nis_name, nis_object *);
extern nis_result * nis_remove(nis_name, nis_object *);
extern nis_result * nis_modify(nis_name, nis_object *);

extern nis_result * nis_add_entry(nis_name, nis_object *, u_long);
extern nis_result * nis_remove_entry(nis_name, nis_object *, u_long);
extern nis_result * nis_modify_entry(nis_name, nis_object *, u_long);
extern nis_result * nis_first_entry(nis_name);
extern nis_result * nis_next_entry(nis_name, netobj *);

extern nis_error nis_mkdir(nis_name, nis_server *);
extern nis_error nis_rmdir(nis_name, nis_server *);
extern name_pos nis_dir_cmp(nis_name, nis_name);

extern nis_name * nis_getnames(nis_name);
extern void nis_freenames(nis_name *);
extern nis_name nis_domain_of(nis_name);
extern nis_name nis_leaf_of(nis_name);
extern nis_name nis_leaf_of_r(const nis_name, char *, size_t);
extern nis_name nis_name_of(nis_name);
extern nis_name nis_local_group(void);
extern nis_name nis_local_directory(void);
extern nis_name nis_local_principal(void);
extern nis_name nis_local_host(void);

extern void nis_destroy_object(nis_object *);
extern nis_object * nis_clone_object(nis_object *, nis_object *);
extern void nis_print_object(nis_object *o);

extern char * nis_sperrno(nis_error);
extern void nis_perror(nis_error, char *);
extern char * nis_sperror(nis_error, char *);
extern void nis_lerror(nis_error, char *);

extern void nis_print_group_entry(nis_name);
extern bool_t nis_ismember(nis_name, nis_name);
extern nis_error nis_creategroup(nis_name, u_long);
extern nis_error nis_destroygroup(nis_name);
extern nis_error nis_addmember(nis_name, nis_name);
extern nis_error nis_removemember(nis_name, nis_name);
extern nis_error nis_verifygroup(nis_name);

extern void nis_freeservlist(nis_server **);
extern nis_server ** nis_getservlist(nis_name);
extern nis_error nis_stats(nis_server *, nis_tag *, int, nis_tag **);
extern nis_error nis_servstate(nis_server *, nis_tag *, int, nis_tag **);
extern void nis_freetags(nis_tag *, int);

extern nis_result * nis_checkpoint(nis_name);
extern void nis_ping(nis_name, u_long, nis_object *);

/*
 * XXX: PLEASE NOTE THAT THE FOLLOWING FUNCTIONS ARE INTERNAL
 * TO NIS+ AND SHOULD NOT BE USED BY ANY APPLICATION PROGRAM.
 * THEIR SEMANTICS AND/OR SIGNATURE CAN CHANGE WITHOUT NOTICE.
 * SO, PLEASE DO NOT USE THEM.  YOU ARE WARNED!!!!
 */

extern char ** __break_name(nis_name, int *);
extern int __name_distance(char **, char **);
extern nis_result * nis_make_error(nis_error, u_long, u_long, u_long, u_long);
extern nis_attr * __cvt2attr(int *, char **);
extern void nis_free_request(ib_request *);
extern nis_error nis_get_request(nis_name, nis_object *, netobj*, ib_request*);
extern nis_object * nis_read_obj(char *);
extern int nis_write_obj(char *, nis_object *);
extern int nis_in_table(nis_name, NIS_HASH_TABLE *, int *);
extern int nis_insert_item(NIS_HASH_ITEM *, NIS_HASH_TABLE *);
extern NIS_HASH_ITEM * nis_find_item(nis_name, NIS_HASH_TABLE *);
extern NIS_HASH_ITEM * nis_remove_item(nis_name, NIS_HASH_TABLE *);
extern void nis_insert_name(nis_name, NIS_HASH_TABLE *);
extern void nis_remove_name(nis_name, NIS_HASH_TABLE *);
extern CLIENT * nis_make_rpchandle(nis_server *, int, u_long, u_long, u_long,
								int, int);
extern void * nis_get_static_storage(struct nis_sdata *, u_long, u_long);
extern char * nis_data(char *);
extern char * nis_old_data(char *);
extern void nis_print_rights(u_long);
extern void nis_print_directory(directory_obj *);
extern void nis_print_group(group_obj *);
extern void nis_print_table(table_obj *);
extern void nis_print_link(link_obj *);
extern void nis_print_entry(entry_obj *);
extern nis_object * nis_get_object(char *, char *, char *, u_long, u_long,
								    zotypes);
extern nis_server * __nis_init_callback(CLIENT *,
    int (*)(nis_name, nis_object *, void *), void *);
extern int nis_getdtblsize(void);
extern int __nis_run_callback(netobj *, u_long, struct timeval *, CLIENT *);

extern log_result *nis_dumplog(nis_server *, nis_name, u_long);
extern log_result *nis_dump(nis_server *, nis_name,
    int (*)(nis_name, nis_object *, void *));

extern bool_t __do_ismember(nis_name, nis_name,
    nis_result *(*)(nis_name, u_long));
extern nis_name __nis_map_group(nis_name);
extern nis_name __nis_map_group_r(nis_name, char *, size_t);

extern nis_error __nis_CacheBind(char *, directory_obj *);
extern nis_error __nis_CacheSearch(char *, directory_obj *);
extern bool_t __nis_CacheRemoveEntry(directory_obj *);
extern void __nis_CacheRestart(void);
extern void __nis_CachePrint(void);
extern void __nis_CacheDumpStatistics(void);
extern bool_t writeColdStartFile(directory_obj *);

extern CLIENT * __get_ti_clnt(char *, CLIENT *, char **, pid_t *, dev_t *);
extern int __strcmp_case_insens(char *, char *);
extern int __strncmp_case_insens(char *, char *);

extern fd_result * nis_finddirectory(directory_obj *, nis_name);
extern fd_result * __nis_finddirectory_r(directory_obj *, nis_name,
	fd_result *);
extern int __start_clock(int);
extern u_long __stop_clock(int);

#else

/* Non-prototype definitions (old fashioned C) */

extern void nis_freeresult();
extern nis_result * nis_lookup();
extern nis_result * nis_list();
extern nis_result * nis_add();
extern nis_result * nis_remove();
extern nis_result * nis_modify();

extern nis_result * nis_add_entry();
extern nis_result * nis_remove_entry();
extern nis_result * nis_modify_entry();
extern nis_result * nis_first_entry();
extern nis_result * nis_next_entry();

extern nis_error nis_mkdir();
extern nis_error nis_rmdir();
extern name_pos nis_dir_cmp();

extern nis_name *nis_getnames();
extern void nis_freenames();
extern nis_name nis_domain_of();
extern nis_name nis_leaf_of();
extern nis_name nis_leaf_of_r();
extern nis_name nis_name_of();
extern nis_name nis_local_group();
extern nis_name nis_local_directory();
extern nis_name nis_local_principal();
extern nis_name nis_local_host();

extern void nis_destroy_object();
extern nis_object * nis_clone_object();
extern void nis_print_object();

extern char * nis_sperrno();
extern void nis_perror();
extern char * nis_sperror();
extern void nis_lerror();

extern void nis_print_group_entry();
extern bool_t nis_ismember();
extern nis_error nis_creategroup();
extern nis_error nis_destroygroup();
extern nis_error nis_addmember();
extern nis_error nis_removemember();
extern nis_error nis_verifygroup();

extern void nis_freeservlist();
extern nis_server ** nis_getservlist();
extern nis_error nis_stats();
extern nis_error nis_servstate();
extern void nis_freetags();

extern nis_result * nis_checkpoint();
extern void nis_ping();

/*
 * XXX: PLEASE NOTE THAT THE FOLLOWING FUNCTIONS ARE INTERNAL
 * TO NIS+ AND SHOULD NOT BE USED BY ANY APPLICATION PROGRAM.
 * THEIR SEMANTICS AND/OR SIGNATURE CAN CHANGE WITHOUT NOTICE.
 * SO, PLEASE DO NOT USE THEM.  YOU ARE WARNED!!!!
 */
extern char ** __break_name();
extern int __name_distance();
extern nis_result * nis_make_error();
extern nis_attr * __cvt2attr();
extern void nis_free_request();
extern nis_error nis_get_request();
extern nis_object * nis_read_obj();
extern int nis_write_obj();
extern int nis_in_table();
extern int nis_insert_item();
extern NIS_HASH_ITEM * nis_find_item();
extern NIS_HASH_ITEM * nis_remove_item();
extern void nis_insert_name();
extern void nis_remove_name();
extern CLIENT * nis_make_rpchandle();
extern void * nis_get_static_storage();
extern char * nis_data();
extern char * nis_old_data();

extern void nis_print_rights();
extern void nis_print_directory();
extern void nis_print_group();
extern void nis_print_table();
extern void nis_print_link();
extern void nis_print_entry();
extern nis_object * nis_get_object();

extern nis_server * __nis_init_callback();
extern int nis_getdtblsize();
extern int __nis_run_callback();

extern log_result * nis_dump();
extern log_result * nis_dumplog();

extern bool_t __do_ismember();
extern nis_name __nis_map_group();
extern nis_name __nis_map_group_r();


extern nis_error __nis_CacheBind();
extern directory_obj * __nis_CacheSearch();
extern bool_t __nis_CacheRemoveEntry();
extern void __nis_CacheRestart();
extern void __nis_CachePrint();
extern void __nis_CacheDumpStatistics();
extern bool_t writeColdStartFile();

extern CLIENT * __get_ti_clnt();
extern int __strcmp_case_insens();
extern int __strncmp_case_insens();

extern fd_result * nis_finddirectory();
extern fd_result * __nis_finddirectory_r();
extern int __start_clock();
extern u_long __stop_clock();

#endif

#define	NUL '\0'

#ifdef	__cplusplus
}
#endif

#endif	/* _RPCSVC_NISLIB_H */
