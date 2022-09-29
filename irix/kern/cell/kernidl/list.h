#if !defined(_list_h_)
#define _list_h_

#include <stddef.h>
#include <assert.h>

#include <stdio.h>

// Shamelessly stolen from <ifl/iflList.h> ... someday list primitives will
// be a part of C++ ...

class listitem_t {
public:
    listitem_t *next, *prev;

    listitem_t(listitem_t *_next=NULL, listitem_t *_prev=NULL)
    { next = _next; prev = _prev; }

    virtual ~listitem_t()
    {
	if (next != prev)
		printf("this 0x%x, next 0x%x, prev 0x%x\n", this, next, prev);
	assert(next == prev);
	assert(next == NULL || next == this);
    }

    int isLinked() const { return next != NULL; }

    void unlink()
    {
	assert(isLinked());
	listitem_t *after = next, *before = prev;
	before->next = after;
	after->prev = before;
	next = prev = NULL;
    }
};

class list_t {
private:
    listitem_t root;			// head (and tail) of list
    friend class listiter_t;

public:
    list_t() : root(&root, &root) {}

    void append(listitem_t *item)
    {
	listitem_t *itemtail;
	listitem_t *after = &root, *before = root.prev;
	int	chainlen = 0;
	for (itemtail = item; itemtail->next != NULL; itemtail = item->next)
		chainlen++;
	assert(chainlen == 0 || chainlen == 1);
	if (item != itemtail) {
		assert(item->next == itemtail);
		assert(item->prev == NULL);
		assert(itemtail->prev == item);
		assert(itemtail->next == NULL);
	}
	item->prev = before;
	itemtail->next = after;
	after->prev = itemtail;
	before->next = item;
    }

    int is_empty() const { return root.next == &root; }
    listitem_t *head() const  { return is_empty() ? NULL : root.next; }
    listitem_t *tail() const  { return is_empty() ? NULL : root.prev; }
};

class listiter_t {
private:
    list_t *list;
    listitem_t *next_iter, *prev_iter;

    void set_iter(listitem_t *item)
    { next_iter = item->next; prev_iter = item->prev; }

public:
    listiter_t(list_t &_list) { reset(_list); }
    listiter_t(list_t *_list) { reset(_list); }

    void reset(list_t &_list) { list = &_list; reset(); }
    void reset(list_t *_list) { list =  _list; reset(); }
    void reset() { set_iter(&list->root); }

    listitem_t *next()
    {
	listitem_t *item = next_iter;
	set_iter(item);
	return item == &list->root ? NULL : item;
    }
    listitem_t *prev()
    {
	listitem_t *item = prev_iter;
	set_iter(item);
	return item == &list->root ? NULL : item;
    }

    // Return TRUE is last returned list item was the first (last) item
    // in the list.
    int is_first() { return prev_iter == &list->root; }
    int is_last()  { return next_iter == &list->root; }
};

#endif /* _list_h_ */
