#ifndef UTREEXO_NODEPOOL_H
#define UTREEXO_NODEPOOL_H

#include "../include/accumulator.h"
#include "check.h"
#include <cassert>
#include <iostream>
#include <type_traits>

namespace utreexo {

/**
 * The NodePool is a allocator for accumulator nodes.
 * It allocates a fixed number of nodes during creation and manages them.
 * A caller can take a node and then later give it back. A node can only be taken once.
 * The motivation behind this is that (at least on macOS) a lot of small malloc/free calls
 * slow down the accumulator quite a bit.
 * (The pool also allocates a reference counter for every node to avoid a heap allocation for each NodePtr.) 
 * TODO: Restrict type T to Node types from the accumulator.
 */
template <class T>
class Accumulator::NodePool
{
private:
    int m_capacity;
    // The size of T in bytes.
    int m_type_size;
    // The number of taken nodes. m_capacity - m_num_taken is the number of nodes still left.
    int m_num_taken;
    int m_initialized;
    int m_next;
    int m_total_refs_taken;

    T* m_elements;
    int* m_list;
    int* m_ref_counts;

public:
    /**
     * Create a pool with a fixed number of nodes in it (capacity).
     * The default constructor of type T will be called once for every node
     * The total number of bytes allocates is: (sizeof(T) + 4 + 4) * capacity.
     */
    NodePool(int capacity);

    /**
     * Destroy the pool. This frees the memory allocate during creation and
     * destructs all nodes.
     */
    ~NodePool();

    /** Return the reference counter for a node from the pool. */
    int* RefCount(T* node);

    /**
     * Return a node from the pool that is not yet taken.
     * Return nullptr if all nodes are taken.
     */
    T* Take();

    /**
     * Give a node back to the pool.
     * After this no references to the node should be kept and the node
     * is free to be taken again.
     */
    void GiveBack(T* node);

    int Capacity() const { return m_capacity; }
    int Size() const { return m_num_taken; }
};

template <class T>
Accumulator::NodePool<T>::NodePool(int capacity)
{
    m_capacity = capacity;
    m_num_taken = 0;
    m_initialized = 0;
    m_elements = new T[m_capacity];
    m_list = new int[m_capacity];
    m_ref_counts = new int[m_capacity];
    m_next = 0;
    m_type_size = sizeof(T);
    m_total_refs_taken = 0;
}

template <class T>
Accumulator::NodePool<T>::~NodePool()
{
    CHECK_SAFE(m_num_taken == 0);
    m_capacity = 0;

    // Free allocated memory.

    // Free the elements first because the destructors still use the ref counters.
    delete[] m_elements;
    delete[] m_list;
    delete[] m_ref_counts;
}

template <class T>
int* Accumulator::NodePool<T>::RefCount(T* node)
{
    if (node == nullptr) return nullptr;

    CHECK_SAFE(node >= m_elements && node < m_elements + m_capacity);

    ++m_total_refs_taken;
    int node_index = ((uint64_t)node - (uint64_t)m_elements) / m_type_size;
    return m_ref_counts + node_index;
}

template <class T>
T* Accumulator::NodePool<T>::Take()
{
    CHECK_SAFE(m_num_taken <= m_capacity);

    if (m_num_taken == m_capacity) {
        return NULL;
    }

    if (m_initialized < m_capacity) {
        m_list[m_initialized] = m_initialized + 1;
        ++m_initialized;
    }

    ++m_num_taken;
    T* element = &m_elements[m_next];
    m_next = m_list[m_next];


    if (m_num_taken == m_capacity) {
        // All nodes have been taken.
        m_next = -1;
    }

    return element;
}


template <class T>
void Accumulator::NodePool<T>::GiveBack(T* node)
{
    if (node == nullptr || m_capacity == 0) return;

    int node_index = ((uint64_t)node - (uint64_t)m_elements) / m_type_size;
    assert(node_index < m_initialized);
    CHECK_SAFE(node_index < m_capacity);

    --m_num_taken;

    node->NodePoolDestroy();

    if (m_next < 0) {
        m_next = node_index;
        return;
    }

    m_list[node_index] = m_next;
    m_next = node_index;
}

/** 
 * Smart pointer for NodePool objects.
 * Uses reference counting to determine when a node should be given back to the pool.
 * TODO: Handle circular references.
 */
template <class T>
class Accumulator::NodePtr
{
private:
    template <class U>
    friend class NodePtr;

    NodePool<T>* m_pool;
    T* m_int_ptr;
    int* m_ref_count;

    NodePtr(NodePool<T>* pool, T* node, int* ref_count) : m_pool(pool), m_int_ptr(node), m_ref_count(ref_count) { AddRef(); }

    void AddRef();
    void RemoveRef();
    void Assign(const NodePtr node);

public:
    /** A default NodePtr points to nullptr. */
    NodePtr() : NodePtr<T>(nullptr) {}

    /** Create a NodePtr that points to a new node from the pool. */
    NodePtr(NodePool<T>* pool);

    // Copy and Move constructors are important for reference counting.

    template <class U>
    NodePtr(const NodePtr<U>& node) : NodePtr<T>((NodePool<T>*)node.m_pool, (T*)node.m_int_ptr, node.m_ref_count)
    {
    }
    NodePtr(const NodePtr& node) : NodePtr<T>(node.m_pool, node.m_int_ptr, node.m_ref_count) {}
    template <class U>
    NodePtr(const NodePtr<U>&& node) : NodePtr<T>((NodePool<T>*)node.m_pool, (T*)node.m_int_ptr, node.m_ref_count)
    {
    }
    NodePtr(const NodePtr&& node) : NodePtr<T>(node.m_pool, node.m_int_ptr, node.m_ref_count) {}

    ~NodePtr();

    /** Return the internal T* pointer. */
    T* get() const;

    /** Override the -> operator to make NodePtr immitate real pointers. */
    T* operator->() const;

    /** Override the * operator to make NodePtr immitate real pointers. */
    T& operator*();

    // Overriding the = operator is important for reference counting.

    template <class U>
    NodePtr<T>& operator=(const NodePtr<U>& other);
    NodePtr& operator=(const NodePtr& other);
    template <class U>
    NodePtr<T>& operator=(const NodePtr<U>&& other);
    NodePtr& operator=(const NodePtr&& other);

    /** Compare if two NodePtr point to the same node. */
    bool operator==(const T& other) const;

    /** Return wether or not the NodePtr points to null. */
    operator bool() const;

    /** Return the number of references to the node */
    int RefCount();
};

template <class T>
Accumulator::NodePtr<T>::NodePtr(Accumulator::NodePool<T>* pool)
{
    if (pool == nullptr) {
        m_pool = nullptr;
        m_int_ptr = nullptr;
        m_ref_count = nullptr;
        return;
    }

    m_pool = pool;
    m_int_ptr = m_pool->Take();
    m_ref_count = m_pool->RefCount(m_int_ptr);
    *m_ref_count = 1;
}

template <class T>
Accumulator::NodePtr<T>::~NodePtr()
{
    if (!m_ref_count) {
        return;
    }

    --(*m_ref_count);
    if (*m_ref_count == 0) {
        m_pool->GiveBack(m_int_ptr);
    }

    CHECK_SAFE(*m_ref_count >= 0);
}

template <class T>
T* Accumulator::NodePtr<T>::get() const
{
    return m_int_ptr;
}

template <class T>
T& Accumulator::NodePtr<T>::operator*()
{
    return *m_int_ptr;
}

template <class T>
T* Accumulator::NodePtr<T>::operator->() const
{
    return m_int_ptr;
}

template <class T>
void Accumulator::NodePtr<T>::AddRef()
{
    if (m_ref_count) {
        ++(*m_ref_count);
    }
}

template <class T>
void Accumulator::NodePtr<T>::RemoveRef()
{
    if (m_ref_count) {
        --(*m_ref_count);
    }
}

template <class T>
void Accumulator::NodePtr<T>::Assign(const NodePtr other)
{
    if (m_ref_count == other.m_ref_count) return;

    RemoveRef();
    if (m_ref_count && *m_ref_count == 0) {
        CHECK_SAFE(m_pool != nullptr);
        m_pool->GiveBack(m_int_ptr);
    }

    CHECK_SAFE(!m_ref_count || *m_ref_count >= 0);

    m_int_ptr = other.m_int_ptr;
    m_ref_count = other.m_ref_count;
    m_pool = (Accumulator::NodePool<T>*)other.m_pool;
    AddRef();
}

template <class T>
Accumulator::NodePtr<T>& Accumulator::NodePtr<T>::operator=(const NodePtr& other)
{
    Assign(other);
    return *this;
}

template <class T>
Accumulator::NodePtr<T>& Accumulator::NodePtr<T>::operator=(const NodePtr&& other)
{
    Assign(other);
    return *this;
}

template <class T>
template <class U>
Accumulator::NodePtr<T>& Accumulator::NodePtr<T>::operator=(const NodePtr<U>& other)
{
    Assign(other);
    return *this;
}

template <class T>
template <class U>
Accumulator::NodePtr<T>& Accumulator::NodePtr<T>::operator=(const NodePtr<U>&& other)
{
    Assign(other);
    return *this;
}

template <class T>
inline bool Accumulator::NodePtr<T>::operator==(const T& other) const
{
    return other.m_int_ptr == m_int_ptr;
}

template <class T>
inline Accumulator::NodePtr<T>::operator bool() const
{
    return m_int_ptr != nullptr;
}

template <class T>
int Accumulator::NodePtr<T>::RefCount()
{
    if (m_ref_count) {
        return *m_ref_count;
    }

    return 0;
}

};     // namespace utreexo
#endif // UTREEXO_NODEPOOL_H
