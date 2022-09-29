#ifndef _XFS_TRANS_PRIV_H
#define	_XFS_TRANS_PRIV_H

#ident "$Revision: 1.12 $"

struct xfs_log_item;
struct xfs_log_item_desc;
struct xfs_mount;
struct xfs_trans;

/*
 * From xfs_trans_item.c
 */
struct xfs_log_item_desc	*xfs_trans_add_item(struct xfs_trans *,
					    struct xfs_log_item *);
void				xfs_trans_free_item(struct xfs_trans *,
					    struct xfs_log_item_desc *);
struct xfs_log_item_desc	*xfs_trans_find_item(struct xfs_trans *,
					     struct xfs_log_item *);
struct xfs_log_item_desc	*xfs_trans_first_item(struct xfs_trans *);
struct xfs_log_item_desc	*xfs_trans_next_item(struct xfs_trans *,
					     struct xfs_log_item_desc *);
void				xfs_trans_free_items(struct xfs_trans *, int);
void				xfs_trans_unlock_items(struct xfs_trans *);


/*
 * From xfs_trans_ail.c
 */
#if defined(INTERRUPT_LATENCY_TESTING)
#define	xfs_trans_update_ail(a,b,c,d)	_xfs_trans_update_ail(a,b,c)
#define	xfs_trans_delete_ail(a,b,c)	_xfs_trans_delete_ail(a,b)
void			_xfs_trans_update_ail(struct xfs_mount *,
				     struct xfs_log_item *, xfs_lsn_t);
void			_xfs_trans_delete_ail(struct xfs_mount *,
				     struct xfs_log_item *);
#else	/* INTERRUPT_LATENCY_TESTING */
#define	xfs_trans_update_ail(a,b,c,d)	_xfs_trans_update_ail(a,b,c,d)
#define	xfs_trans_delete_ail(a,b,c)	_xfs_trans_delete_ail(a,b,c)
void			xfs_trans_update_ail(struct xfs_mount *,
				     struct xfs_log_item *, xfs_lsn_t, int);
void			xfs_trans_delete_ail(struct xfs_mount *,
				     struct xfs_log_item *, int);
#endif	/* INTERRUPT_LATENCY_TESTING */
struct xfs_log_item	*xfs_trans_first_ail(struct xfs_mount *, int *);
struct xfs_log_item	*xfs_trans_next_ail(struct xfs_mount *,
				     struct xfs_log_item *, int *, int *);


#endif	/* _XFS_TRANS_PRIV_H */
