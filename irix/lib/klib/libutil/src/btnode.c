#ident "$Header: "

#include <sys/types.h>
#include <stdio.h>
#include <klib/btnode.h>

/* Local function prototypes 
 */
int max(int, int);
void set_btnode_height(btnode_t *);
void swap_btnode(btnode_t **, btnode_t **);
void single_left(btnode_t **);
void single_right(btnode_t **);
void double_left(btnode_t **);
void double_right(btnode_t **);
void balance_tree(btnode_t **);

/*
 * max()
 */
static int
max(int x, int y)
{
    return((x > y) ? x : y);
}

/*
 * alloc_btnode()
 */
btnode_t *
alloc_btnode(char *key)
{
    btnode_t *np;

    if (np = (btnode_t *)malloc(sizeof(*np))) {
		bzero(np, sizeof(*np));
		np->bt_key = strdup(key);
	}
    return (np);
}

/*
 * free_btnode()
 */
void
free_btnode(btnode_t *np)
{
	if (np) {
		free(np->bt_key);
		free(np);
	}
}

/*
 * btnode_height()
 */
int
btnode_height(btnode_t *root)
{
    if (!root) {
        return(-1);
    }
    return(root->bt_height);
}

/*
 * set_btnode_height()
 */
static void
set_btnode_height(btnode_t *np)
{
    if (np) {
        np->bt_height = 
			1 + max(btnode_height(np->bt_left), btnode_height(np->bt_right));
    }
}

/*
 * insert_btnode()
 */
int
insert_btnode(btnode_t **root, btnode_t *np, int flags)
{
    int ret, status = 0;

    if (*root == (btnode_t *)NULL) {
        *root = np;
    }
    else {
        ret = strcmp(np->bt_key, (*root)->bt_key);
        if (ret < 0) {
            status = insert_btnode(&((*root)->bt_left), np, flags);
            set_btnode_height(*root);
            balance_tree(root);
        }
        else if ((ret == 0) && !(flags & DUPLICATES_OK)) {
            status = -1;
        }
        else {
            status = insert_btnode(&((*root)->bt_right), np, flags);
            set_btnode_height(*root);
            balance_tree(root);
        }
    }
    return(status);
}

/*
 * swap_btnodes()
 *
 *   This function finds the next largest key in tree starting at
 *   root and swaps the node containing the key with node pointed
 *   to by npp. The routine is called recursively so that all nodes
 *   visited are height adjusted and balanced. This function is
 *   used in conjunction with delete_betnode() to remove a node
 *   from the tree while maintaining a reasonable balance in the
 *   tree.
 */
static void
swap_btnode(btnode_t **root, btnode_t **npp)
{
    btnode_t *tmp;

    if ((*root)->bt_right == (btnode_t *)NULL) {

        /* We're at the end of the line, so go ahead and do the
         * swap.
         */
        tmp = *npp;
        *npp = *root;
        *root = (*root)->bt_left;
        (*npp)->bt_left = tmp->bt_left;
        (*npp)->bt_right = tmp->bt_right;
        (*npp)->bt_height = tmp->bt_height;
        if (*root) {
            set_btnode_height(*root);
            balance_tree(root);
        }
    }
    else {
        swap_btnode(&(*root)->bt_right, npp);
        set_btnode_height(*root);
        balance_tree(root);
    }
}

/*
 * delete_btnode()
 *
 *    This function finds a node in a tree and then removes it, making
 *    sure to keep the tree in a reasonably balanced state. A pointer
 *    to the just freed node is then passed to the free function (if
 *    one is passed in as a parameter).
 */
int
delete_btnode(btnode_t **root, btnode_t *np, void(*free)(), int flags)
{
    int ret;
    btnode_t *hold;

    if (!np) {
        return(0);
    }

    if (*root == np) {

        /* We found it
         */
        if (!(*root)->bt_left && !(*root)->bt_right) {

            /* This node has no children, just remvoe it
             */
            hold = *root;
            *root = (btnode_t *)NULL;
        }
        else if ((*root)->bt_left && !(*root)->bt_right) {

            /* There is only a left child. Remove np and
             * link the child to root.
             */
            hold = *root;
            *root = (*root)->bt_left;
        }
        else if ((*root)->bt_right && !(*root)->bt_left) {

            /* There is only a right child. Remove np and
             * link the child to root.
             */
            hold = *root;
            *root = (*root)->bt_right;
        }
        else {

            /* We have both a left and right child. This is
             * more complicated. We have to find the node with
             * the next smallest key and exchange this node with
             * that. It will then be the other node which will
             * be deleted from the tree.
             */
            hold = *root;
            swap_btnode(&((*root)->bt_left), root);
        }
        if (*root) {
            set_btnode_height(*root);
            balance_tree(root);
        }
		if (free) {
			(*free)(hold);
		}
        return(0);
    }

    /* Continue walking the tree searching for the key that
     * needs to be deleted.
     */
    ret = strcmp(np->bt_key, (*root)->bt_key);
    if (ret < 0) {
        delete_btnode(&((*root)->bt_left), np, free, flags);
        set_btnode_height(*root);
        balance_tree(root);
    }
    else {
        delete_btnode(&((*root)->bt_right), np, free, flags);
        set_btnode_height(*root);
        balance_tree(root);
    }
    return(0);
}

/*
 * single_left()
 */
static void
single_left(btnode_t **root)
{
    btnode_t *old_root, *old_right, *middle;

    /* Get the relevant pointer values
     */
    old_root = *root;
    old_right = old_root->bt_right;
    middle = old_right->bt_left;

    /* Rearrange the nodes
     */
    old_right->bt_left = old_root;
    old_root->bt_right = middle;
    *root = old_right;

    /* Adjust the node heights
     */
    set_btnode_height(old_root);
    set_btnode_height(old_right);
}

/*
 * single_right()
 */
static void
single_right(btnode_t **root)
{
    btnode_t *old_root, *old_left, *middle;

    /* Get the relevant pointer values
     */
    old_root = *root;
    old_left = old_root->bt_left;
    middle = old_left->bt_right;

    /* Rearrange the nodes
     */
    old_left->bt_right = old_root;
    old_root->bt_left = middle;
    *root = old_left;

    /* Adjust the node heights
     */
    set_btnode_height(old_root);
    set_btnode_height(old_left);
}

/*
 * double_left()
 */
static void
double_left(btnode_t **root)
{
    single_right(&((*root)->bt_right));
    single_left(root);
}

/*
 * double_right()
 */
static void
double_right(btnode_t **root)
{
    single_left(&((*root)->bt_left));
    single_right(root);
}

/*
 * adjust_parent_pointers()
 */
static void
adjust_parent_pointers(btnode_t **root)
{
	if (!(*root)) {
		return;
	}
	if ((*root)->bt_left) {
		(*root)->bt_left->bt_parent = *root;
		adjust_parent_pointers(&((*root)->bt_left));
	}
	if ((*root)->bt_right) {
		(*root)->bt_right->bt_parent = *root;
		adjust_parent_pointers(&((*root)->bt_right));
	}

	/* Check and see if this btnode is the root of the tree. This 
	 * would be the case if the parent height of each child is less 
	 * than the hight of the current node. If this IS the root node, 
	 * then zero out the parent pointer (if there is one).
	 */
	 if ((*root)->bt_parent) {
		if ((*root)->bt_parent->bt_height <= (*root)->bt_height) {
			(*root)->bt_parent = (btnode_t *)NULL;
		}
	 }
}

/*
 * balance_tree()
 */
static void
balance_tree(btnode_t **root)
{
    int left_height, right_height;
    btnode_t *subtree;

    left_height = btnode_height((*root)->bt_left);
    right_height = btnode_height((*root)->bt_right);

    if (left_height > (right_height + 1)) {

        /* Too deep to the left
         */
        subtree = (*root)->bt_left;

        left_height = btnode_height(subtree->bt_left);
        right_height = btnode_height(subtree->bt_right);

        if (left_height >= right_height) {

            /* Too deep to the outside
             */
            single_right(root);
        }
        else {

            /* Too deep to the inside
             */
            double_right(root);
        }
    }
    else if ((left_height + 1) < right_height) {

        /* Too deep to the right
         */
        subtree = (*root)->bt_right;

        left_height = btnode_height(subtree->bt_left);
        right_height = btnode_height(subtree->bt_right);

        if (right_height >= left_height) {

            /* Too deep to the outside
             */
            single_left(root);
        }
        else {

            /* Too deep to the inside
             */
            double_left(root);
        }
    }

	/* Now make sure that all of the bt_parent pointers are set correctly
	 */
	adjust_parent_pointers(root);
}

/*
 * find_btnode()
 */
btnode_t *
find_btnode(btnode_t *np, char *key, int *max_depth)
{
    int ret;
    btnode_t *found = NULL;

    if (!np) {
        return((btnode_t *)NULL);
    }

    /* Bump the counter (if a pointer for one was passed in)
     * to calculate total nodes searched before finding/not
     * finding a key.
     */
    if (max_depth) {
        (*max_depth)++;
    }

    /* See if this node is the one we are looking for
     */
    ret = strcmp(key, np->bt_key);
    if (ret == 0) {
        return(np);
    }
    else if (ret < 0) {
        if (found = find_btnode(np->bt_left, key, max_depth)) {
            return(found);
        }
    }
    else if (ret > 0) {
        if (found = find_btnode(np->bt_right, key, max_depth)) {
            return(found);
        }
    }
    return((btnode_t *)NULL);
}

/*
 * first_btnode()
 */
btnode_t *
first_btnode(btnode_t *np)
{
	if (!np) {
		return((btnode_t *)NULL);
	}

	/* Walk down the left side 'til the end...
	 */
	while (np->bt_left) {
		np = np->bt_left;
	}
	return(np);
}

/*
 * next_btnode()
 */
btnode_t *
next_btnode(btnode_t *np)
{
	int ret;
	btnode_t *parent;

	if (np) {
		if (np->bt_right) {
			return(first_btnode(np->bt_right));
		}
		else {
			parent = np->bt_parent;
			while (parent) {
				ret = strcmp(np->bt_key, parent->bt_key);
				if (ret < 0) {
					return(parent);
				}
				parent = parent->bt_parent;
			}
		}
	}
	return((btnode_t *)NULL);
}

/*
 * prev_btnode()
 */
btnode_t *
prev_btnode(btnode_t *np)
{
	int ret;
	btnode_t *next, *parent;

	if (np) {
		if (next = np->bt_left) {
			while (next->bt_right) {
				next = next->bt_right;
			}
			return(next);
		}
		parent = np->bt_parent;
		while (parent) {
			ret = strcmp(parent->bt_key, np->bt_key);
			if (ret < 0) {
				return(parent);
			}
			parent = parent->bt_parent;
		}
	}
	return((btnode_t *)NULL);
}
