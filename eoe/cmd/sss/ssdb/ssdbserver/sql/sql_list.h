/* Copyright (C) 1979-1996 TcX AB & Monty Program KB & Detron HB

   This software is distributed with NO WARRANTY OF ANY KIND.  No author or
   distributor accepts any responsibility for the consequences of using it, or
   for whether it serves any particular purpose or works at all, unless he or
   she says so in writing.  Refer to the Free Public License (the "License")
   for full details.

   Every copy of this file must include a copy of the License, normally in a
   plain ASCII text file named PUBLIC.	The License grants you the right to
   copy, modify and redistribute this file, but only under certain conditions
   described in the License.  Among other things, the License requires that
   the copyright notice and this notice be preserved on all copies. */

/* mysql standard open memoryallocator */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif


class Sql_alloc
{
public:
  static void *operator new(size_t size) {return (void*) sql_alloc(size); }
  static void operator delete(void *ptr __attribute__((unused)),
			      size_t size __attribute__((unused))) {} /*lint -e715 */
  inline Sql_alloc() {};
  inline ~Sql_alloc() {};
};

/*
** basic single linked list
** Used for item and item_buffs.
*/

class base_list :public Sql_alloc {
protected:
  class list_node :public Sql_alloc
  {
public:
    list_node *next;
    void *info;
    list_node(void *info_par,list_node *next_par) : next(next_par),info(info_par) {}
    friend class base_list;
    friend class base_list_iterator;
  };
  list_node *first,**last;

public:
  uint elements;

  inline void empty() { elements=0; first=0; last=&first;}
  inline base_list() { empty(); }
  inline base_list(const base_list &tmp) :Sql_alloc()
  {
    elements=tmp.elements;
    first=tmp.first;
    last=tmp.last;
  }
  inline bool push_back(void *info)
  {
    if (((*last)=new list_node(info,0)))
    {
      last= &(*last)->next;
      elements++;
      return 0;
    }
    return 1;
  }
  inline bool push_front(void *info)
  {
    list_node *node=new list_node(info,first);
    if (node)
    {
      if (!first)
	last= &node->next;
      first=node;
      elements++;
      return 0;
    }
    return 1;
  }
  void remove(list_node **prev)
  {
    list_node *node=(*prev)->next;
    delete *prev;
    *prev=node;
    if (!--elements)
    {
      last= &first;
      first=0;
    }
  }
  inline void *pop(void)
  {
    if (!first) return 0;
    list_node *tmp=first;
    first=first->next;
    if (!--elements)
      last= &first;
    return tmp->info;
  }
  inline void *head() { return first ? first->info : 0; }
  inline void **head_ref() { return first ? &first->info : 0; }
  friend class base_list_iterator;

protected:
  void after(void *info,list_node *node)
  {
    list_node *new_node=new list_node(info,node->next);
    node->next=new_node;
    elements++;
  }
};


class base_list_iterator
{
  base_list *list;
  base_list::list_node **el,**prev,*current;
public:
  base_list_iterator(base_list &list_par) :list(&list_par),el(&list_par.first),
    prev(0),current(0)
  {}
  inline void *next(void)
  {
    prev=el;
    if (!(current= *el))
      return 0;
    el= &current->next;
    return current->info;
  }
  inline void rewind(void)
  {
    el= &list->first;
  }
  void *replace(void *element)
  {						// Return old element
    void *tmp=current->info;
    current->info=element;
    return tmp;
  }
  void *replace(base_list &new_list)
  {
    void *ret_value=current->info;
    if (new_list.first)
    {
      *new_list.last=current->next;
      current->info=new_list.first->info;
      current->next=new_list.first->next;
      list->elements+=new_list.elements-1;
    }
    return ret_value;				// return old element
  }
  inline void remove(void)			// Remove current
  {
    list->remove(prev);
    el=prev;
    current=0;					// Safeguard
  }
  void after(void *element)			// Insert element after current
  {
    list->after(element,current);
    current=current->next;
    el= &current->next;
  }
  inline void **ref()				// Get reference pointer
  {
    return &current->info;
  }
};


template <class T> class List :public base_list
{
public:
  inline List() :base_list() {}
  inline List(const List<T> &tmp) :base_list(tmp) {}
  inline bool push_back(T *a) { return base_list::push_back(a); }
  inline bool push_front(T *a) { return base_list::push_front(a); }
  inline T* head() {return (T*) base_list::head(); }
  inline T* pop()  {return (T*) base_list::pop(); }
  void delete_elements(void)
  {
    list_node *element,*next;
    for (element=first; element ; element=next)
    {
      next=element->next;
      delete (T*) element->info;
    }
    empty();
  }
};


template <class T> class List_iterator :public base_list_iterator
{
public:
  List_iterator(List<T> &a) : base_list_iterator(a) {}
  inline T* operator++(int) { return (T*) base_list_iterator::next(); }
  inline void rewind(void)  { base_list_iterator::rewind(); }
  inline T *replace(T *a)   { return (T*) base_list_iterator::replace(a); }
  inline T *replace(List<T> &a) { return (T*) base_list_iterator::replace(a); }
  inline void remove(void)  { base_list_iterator::remove(); }
  inline void after(T *a)   { base_list_iterator::after(a); }
  inline T** ref()	    { return (T**) base_list_iterator::ref(); }
};


/*
** An simple intrusive list with automaticly removes element from list
** on delete (for THD element)
*/

struct ilink {
  struct ilink **prev,*next;
  inline ilink()
  {
    prev=0; next=0;
  }
  inline void unlink()
  {
    if (prev) *prev= next;
    if (next) next->prev=prev;
    prev=0 ; next=0;
  }
  virtual ~ilink() { unlink(); } /*lint -e1740 */
};

template <class T> class I_List_iterator;

class base_ilist {
  public:
  struct ilink *first;
  base_ilist() { first=0; }
  inline void append(ilink *a)
  {
    if (first)
      first->prev=&a->next;
    a->next=first; a->prev=&first; first=a;
  }
  inline struct ilink *get()
  {
    struct ilink *link=first;
    if (link)
      link->unlink();				// Unlink from list
    return link;
  }
  friend class base_list_iterator;
};


class base_ilist_iterator
{
  base_ilist *list;
  struct ilink *el,*current;
public:
  base_ilist_iterator(base_ilist &list_par) :list(&list_par),el(list_par.first),current(0) {}
  void *next(void)
  {
    current=el;
    if (!el) return 0;
    el=el->next;
    return current;
  }
};


template <class T>
class I_List :private base_ilist {
public:
  I_List() :base_ilist() {}
  void append(T* a) { base_ilist::append(a); }
  inline T* get()   { return (T*) base_ilist::get(); }
#ifndef _lint
  friend class I_List_iterator<T>;
#endif
};


template <class T> class I_List_iterator :public base_ilist_iterator
{
public:
  I_List_iterator(I_List<T> &a) : base_ilist_iterator(a) {}
  inline T* operator++(int) { return (T*) base_ilist_iterator::next(); }
};
