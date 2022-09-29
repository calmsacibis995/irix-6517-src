#include "BTree.h"

#include <assert.h>

#ifndef NDEBUG

template <class K, class V>
BTree<K, V>::BTree()
{
    //assert(sizeof K <= sizeof BTB::Key);
    //assert(sizeof V <= sizeof BTB::Value);
}

#endif /* !NDEBUG */
