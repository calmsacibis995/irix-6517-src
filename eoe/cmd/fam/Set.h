#ifndef Set_included
#define Set_included

#include "Boolean.h"
#include "BTree.h"

//  This implementation of a Set is horrible!  It should
//  be reimplemented as a separate type.

template <class T> class Set : public BTree<T, Boolean> {

public:

    typedef BTree<T, Boolean> inherited;

    Set()			       { }

    void insert(const T& e)	       { (void) inherited::insert(e, true); }
    Boolean contains(const T& e)       { return inherited::find(e); }
    // Inherit remove(), first(), next(), size(), sizeofnode() methods.

private:

    Set(const Set&);			// Do not copy
    operator = (const Set&);		//  or assign.

};

#endif /* !Set_included */
