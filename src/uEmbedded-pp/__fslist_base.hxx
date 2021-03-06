//! @brief      A list base class
//! @file       __fslist_base.hxx
//!
//! @author     Seungwoo Kang (ki6080@gmail.com)
//! @copyright  Copyright (c) 2019. Seungwoo Kang. All rights reserved.
//!
//! @details
//!             This class implements thread-unsafe lightweight linked list
#pragma once
#include <iterator>
#include <new>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utility>
#include "../uEmbedded/uassert.h"

namespace upp { namespace impl {
//! @addtogroup uEmbedded_Cpp
//! @{
//! @defgroup   uEmbedded_Cpp_FreeSpaceList
//! @brief
//!             Free space list template class
//! @details
//!              This class is a free-space list wrapped in an easy to use
//!             template. However, it does not recycle the uEmbedded_C library,
//!             but has been rewritten to fit STL container interface. (e.g.
//!             iterator)
//! @{

//! @brief      This class represents a node in a free space list.
//! @tparam     nty_
//!              The free space list accepts the node index type as a
//!             template argument. If you don't use that many nodes, you can
//!             save memory by reducing the space taken up by one node.
//! @note
//!             Unlike the C language version of the library, the C ++ version
//!             does not  use this node class directly. If you need to access an
//!             element of a list, use an iterator instead.
template <typename nty_>
struct fslist_node
{
    enum
    {
        NODE_NONE = (nty_)-1
    };
    nty_ nxt_;
    nty_ prv_;

    nty_ cur() { return cur_; }

    fslist_node<nty_>& prev() { return by_( prv_ ); }
    fslist_node<nty_>& next() { return by_( nxt_ ); }

private:
    //! \brief
    //!         Calculate distance from given argument using offset
    fslist_node<nty_>& by_( int absolute )
    {
        uassert( absolute != NODE_NONE );
        uassert( cur_ != NODE_NONE );
        return *( this - cur_ + absolute );
    }

    nty_ cur_;
    template <typename t_>
    friend class fslist_alloc_base;
};

//! @brief
//!             Base class of fslist that manages fslist nodes based on size
//!             type template argument
//! @details
//!             Manages node list of free space list. This is managed separately
//!             from the data type, where the location of the data is
//!             represented by an index representing the offset in the data
//!             array instead of a pointer to the data directly.
//!              Functions declared as 'public' here are interfaces that are
//!             compatible with STL containers and can be called externally
//!             through class instances.
//! @tparam     nty_ \ref upp::impl::fslist_node
//!
//! @todo       Provide functionality for dynamic capacity features.
//!              Due to the nature of the free space list, it is difficult to
//!             implement a shrink operation for memory copy, so shrinkand
//!             extend are implemented by connecting the active node to the new
//!             node buffer from the front.
//! @todo       Implement heap based fslist
template <typename nty_>
class fslist_alloc_base
{
protected:
    using size_type       = nty_;
    using difference_type = ptrdiff_t;
    using node_type       = fslist_node<size_type>;
    enum
    {
        NODE_NONE = (size_type)-1
    };

    //! @brief      Constructor of node management class
    //! @param      capacity Given node array's capacity.
    //! @param      narray Provided node array
    fslist_alloc_base( size_type capacity, node_type* narray ) noexcept
        : size_( 0 )
        , capacity_( capacity )
        , head_( NODE_NONE )
        , tail_( NODE_NONE )
        , idle_front_( 0 )
        , idle_back_( capacity - 1 )
        , narray_( narray )
    {
        // Link all available nodes
        for ( size_t i = 0; i < capacity; i++ ) {
            auto p  = narray_ + i;
            p->nxt_ = static_cast<size_type>( i + 1 );
            p->cur_ = NODE_NONE;
            p->prv_ = static_cast<size_type>( i - 1 );
        }
        narray_[0].prv_             = NODE_NONE;
        narray_[capacity_ - 1].nxt_ = NODE_NONE;
    }

    //! @brief      Insert new node at given location
    //! @param      i
    //! @param      at
    void insert_node( size_type i, size_type at ) noexcept
    {
        node_type& n = narray_[i];
        if ( at == NODE_NONE ) { // Insert at last
            push_back_node( i );
        }
        else if ( at == head_ ) {
            push_front_node( i );
        }
        else {
            node_type& m = narray_[at];
            uassert( n.cur_ != NODE_NONE );
            uassert( m.cur_ != NODE_NONE );
            uassert( m.prv_ != NODE_NONE );

            m.prev().nxt_ = i;
            n.prv_        = m.prv_;
            m.prv_        = i;
            n.nxt_        = at;
        }
    }

    //! @brief      Append new node to backward
    void push_back_node( size_type i ) noexcept
    {
        node_type& n = narray_[i];
        uassert( n.cur_ != NODE_NONE );

        if ( tail_ != NODE_NONE )
            narray_[tail_].nxt_ = i;
        else {
            uassert( head_ == NODE_NONE );
            head_ = i;
        }

        n.prv_ = tail_;
        tail_  = i;
    }

    //! @brief      Insert front-most node
    void push_front_node( size_type i ) noexcept
    {
        node_type& n = narray_[i];
        uassert( n.cur_ != NODE_NONE );

        if ( head_ != NODE_NONE )
            narray_[head_].prv_ = i;
        else {
            uassert( tail_ == NODE_NONE );
            tail_ = i;
        }

        n.nxt_ = head_;
        head_  = i;
    }

    //! @brief      Allocate new node from memory pool
    //! @details
    //!              The node's links are returned unbroken, so the front and
    //!             back links must be redirected.
    size_type alloc_node() noexcept
    {
        uassert( size_ < capacity_ );
        auto& n     = narray_[idle_front_];
        n.cur_      = idle_front_;
        idle_front_ = n.nxt_;
        if ( idle_front_ == NODE_NONE )
            idle_back_ = NODE_NONE;
        else
            narray_[idle_front_].prv_ = NODE_NONE;
        n.nxt_ = NODE_NONE;
        n.prv_ = NODE_NONE;
        ++size_;
        return n.cur_;
    }

    //! @brief      Unlink given node and put it back to memory pool.
    //! @param      i Node index to unlink
    void dealloc_node( size_type i ) noexcept
    {
        auto& n = narray_[i];
        uassert( n.cur_ != NODE_NONE );
        uassert( i >= 0 && i < capacity_ );

        if ( n.nxt_ != NODE_NONE ) {
            n.next().prv_ = n.prv_;
        }
        else { // It's tail
            tail_ = n.prv_;
        }

        if ( n.prv_ != NODE_NONE ) {
            n.prev().nxt_ = n.nxt_;
        }
        else { // It's head
            head_ = n.nxt_;
        }

        if ( idle_back_ != NODE_NONE ) {
            narray_[idle_back_].nxt_ = i;
        }
        else {
            uassert( idle_front_ == NODE_NONE );
            idle_front_ = i;
        }
        n.prv_     = idle_back_;
        n.nxt_     = NODE_NONE;
        n.cur_     = NODE_NONE;
        idle_back_ = i;
        --size_;
    }

    //! @brief      Get front node index
    size_type head() const noexcept { return head_; }

    //! @brief      Get last valid node index.
    size_type tail() const noexcept { return tail_; }

    size_type next( size_type n ) const noexcept { return narray_[n].nxt_; }
    size_type prev( size_type n ) const noexcept { return narray_[n].prv_; }

    bool valid_node( size_type n ) const noexcept
    {
        return n != NODE_NONE && narray_[n].cur_ != NODE_NONE;
    }

public:
    //! @brief      Get maximum number of nodes that can be held.
    size_type max_size() const noexcept { return capacity_; }

    //! @brief      Get number of currently activated nodes
    size_type size() const noexcept { return size_; }

    //! @brief      Check if list is empty
    bool empty() const noexcept { return size_ == 0; }

    template <typename ty1_, typename ty_2>
    friend class fslist_const_iterator;

private:
    size_type  size_;
    size_type  capacity_;
    size_type  head_;
    size_type  tail_;
    size_type  idle_front_;
    size_type  idle_back_;
    node_type* narray_;
};

//! @brief      list iterator definition
template <typename dty_, typename nty_>
class fslist_const_iterator
{
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = dty_;
    using difference_type   = ptrdiff_t;
    using pointer           = dty_ const*;
    using reference         = dty_ const&;
    using size_type         = nty_;
    using container_type    = fslist_alloc_base<nty_>;
    enum
    {
        NODE_NONE = (size_type)-1
    };

    fslist_const_iterator<dty_, nty_>& operator++() noexcept;
    fslist_const_iterator<dty_, nty_>  operator++( int ) noexcept;
    fslist_const_iterator<dty_, nty_>& operator--() noexcept;
    fslist_const_iterator<dty_, nty_>  operator--( int ) noexcept;
    reference                          operator*() const noexcept;
    pointer                            operator->() const noexcept;

    bool operator!=( const fslist_const_iterator<dty_, nty_>& r ) const noexcept
    {
        return r.container_ != container_ || r.cur_ != cur_;
    }
    bool operator==( const fslist_const_iterator<dty_, nty_>& r ) const noexcept
    {
        return r.container_ == container_ && r.cur_ == cur_;
    }

    bool valid() const noexcept { return container_->valid_node( cur_ ); }
         operator bool() const noexcept { return valid(); }

    size_type fs_idx__() const noexcept { return cur_; }

private:
    container_type const* container_;
    size_type             cur_;

private:
    template <typename ty1_, typename ty2_>
    friend class fslist_base;
};

//! @brief      Modifiable list iterator
template <typename dty_, typename nty_>
class fslist_iterator : public fslist_const_iterator<dty_, nty_>
{
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = dty_;
    using difference_type   = ptrdiff_t;
    using pointer           = dty_*;
    using reference         = dty_&;
    using size_type         = nty_;
    using container_type    = fslist_alloc_base<nty_>;
    using super             = fslist_const_iterator<dty_, nty_>;

    fslist_iterator<dty_, nty_>& operator++() noexcept
    {
        return static_cast<fslist_iterator<dty_, nty_>&>( super::operator++() );
    }
    fslist_iterator<dty_, nty_> operator++( int ) noexcept
    {
        auto r = *this;
        return ++r;
    }

    fslist_iterator<dty_, nty_>& operator--() noexcept
    {
        return static_cast<fslist_iterator&>( super::operator--() );
    }
    fslist_iterator<dty_, nty_> operator--( int ) noexcept
    {
        auto r = *this;
        return ++r;
    }
    reference operator*() const noexcept
    {
        return const_cast<reference>( super::operator*() );
    }
    pointer operator->() const noexcept
    {
        return const_cast<pointer>( super::operator->() );
    }

    bool operator!=( fslist_iterator const& b ) const noexcept
    {
        return (super&)*this != (super&)b;
    }
    bool operator==( fslist_iterator const& b ) const noexcept
    {
        return (super&)*this == (super&)b;
    }

    operator super() const noexcept { return static_cast<super&>( *this ); }
};

//! @brief      Base class for list
//! @details
//!              It provides an interface that can be used similar to a list of
//!             common STL containers. Complex behaviors, such as nodes managed
//!             by indexes, are simplified by implementing them in classes that
//!             inherit them.
template <typename dty_, typename nty_ = size_t>
class fslist_base : public fslist_alloc_base<nty_>
{
public:
    using super_type      = fslist_alloc_base<nty_>;
    using value_type      = dty_;
    using size_type       = nty_;
    using difference_type = ptrdiff_t;
    using pointer         = value_type*;
    using reference       = value_type&;
    using const_pointer   = value_type const*;
    using const_reference = value_type const;
    using node_type       = fslist_node<size_type>;
    using iterator        = fslist_iterator<value_type, size_type>;
    using const_iterator  = fslist_const_iterator<value_type, size_type>;
    using super           = fslist_alloc_base<nty_>;
    enum
    {
        NODE_NONE = (size_type)-1
    };

public:
    ~fslist_base() noexcept { clear(); }

    void clear() noexcept
    {
        for ( size_type i = super::head(); i != NODE_NONE; ) {
            varray_[i].~value_type();
            auto k = i;
            i      = super::next( i );
            super::dealloc_node( k );
        }
    }

    fslist_base(
      size_type  capacity,
      pointer    varray,
      node_type* narray ) noexcept
        : super_type( capacity, narray )
        , varray_( varray )
    {
    }

    template <typename... arg_>
    reference emplace_front( arg_&&... args ) noexcept
    {
        auto n = super::alloc_node();
        super::push_front_node( n );
        auto p
          = new ( varray_ + n ) value_type( std::forward<arg_>( args )... );
        return *p;
    }

    template <typename... arg_>
    reference emplace_back( arg_&&... args ) noexcept
    {
        auto n = super::alloc_node();
        super::push_back_node( n );
        auto p
          = new ( varray_ + n ) value_type( std::forward<arg_>( args )... );
        return *p;
    }

    void push_back( const_reference arg ) noexcept { emplace_back( arg ); }

    void push_front( const_reference arg ) noexcept { emplace_front( arg ); }

    const_iterator cbegin() const noexcept
    {
        const_iterator i;
        i.container_ = this;
        i.cur_       = super::head();
        return i;
    }

    const_iterator cend() const noexcept
    {
        const_iterator i;
        i.container_ = this;
        i.cur_       = NODE_NONE;
        return i;
    }

    iterator begin() noexcept
    {
        auto d = cbegin();
        return static_cast<iterator&>( d );
    }
    iterator end() noexcept
    {
        auto d = cend();
        return static_cast<iterator&>( d );
    }

    const_reference front() const noexcept
    {
        uassert( super::valid_node( super::head() ) );
        return varray_[super::head()];
    }

    const_reference back() const noexcept
    {
        uassert( super::valid_node( super::tail() ) );
        return varray_[super::tail()];
    }

    reference front() noexcept
    {
        uassert( super::valid_node( super::head() ) );
        return varray_[super::head()];
    }

    reference back() noexcept
    {
        uassert( super::valid_node( super::tail() ) );
        return varray_[super::tail()];
    }

    template <typename... ty__>
    iterator emplace( const_iterator pos, ty__&&... args ) noexcept
    {
        uassert( super::size() < super::max_size() );
        auto n = super::alloc_node();
        super::insert_node( n, pos.cur_ );

        new ( varray_ + n ) value_type( std::forward<ty__>( args )... );

        const_iterator r;
        r.container_ = this;
        r.cur_       = n;
        return static_cast<iterator&>( r );
    }

    iterator insert( const_iterator pos, const value_type& arg ) noexcept
    {
        return emplace( pos, arg );
    }

    template <typename it_>
    iterator insert( const_iterator pos, it_ begin, it_ end ) noexcept
    {
        for ( ; begin != end; ++begin ) {
            pos = emplace( pos, *begin );
        }
    }

    void pop_back() noexcept { release( super::tail() ); }

    void pop_front() noexcept { release( super::head() ); }

    void erase( const_iterator pos ) noexcept { release( pos.cur_ ); }

    const_pointer at__( size_type fs_idx ) const noexcept
    {
        return super::valid_node( fs_idx ) ? varray_ + fs_idx : nullptr;
    }

    pointer at__( size_type fs_idx ) noexcept
    {
        return super::valid_node( fs_idx ) ? varray_ + fs_idx : nullptr;
    }

private:
    template <typename ty1_, typename ty2_>
    friend class fslist_const_iterator;

    void release( size_type n )
    {
        uassert( n != NODE_NONE );
        varray_[n].~value_type();
        super::dealloc_node( n );
    }

    pointer get_arg( size_type node ) noexcept
    {
        uassert( node != NODE_NONE );
        uassert( super::valid_node( node ) );
        return varray_ + node;
    }

private:
    pointer varray_;
};

template <typename dty_, typename nty_>
inline fslist_const_iterator<dty_, nty_>&
fslist_const_iterator<dty_, nty_>::operator++() noexcept
{
    uassert( container_ && cur_ != NODE_NONE );
    cur_ = container_->next( cur_ );
    return *this;
}

template <typename dty_, typename nty_>
inline fslist_const_iterator<dty_, nty_>
fslist_const_iterator<dty_, nty_>::operator++( int ) noexcept
{
    auto ret = *this;
    return ++ret;
}

template <typename dty_, typename nty_>
inline fslist_const_iterator<dty_, nty_>&
fslist_const_iterator<dty_, nty_>::operator--() noexcept
{
    uassert( container_ && cur_ != container_.head() );
    if ( cur_ == NODE_NONE ) {
        cur_ = container_->tail();
    }
    else {
        cur_ = container_->prev( cur_ );
    }
    return *this;
}

template <typename dty_, typename nty_>
inline fslist_const_iterator<dty_, nty_>
fslist_const_iterator<dty_, nty_>::operator--( int ) noexcept
{
    auto ret = *this;
    return --ret;
}

template <typename dty_, typename nty_>
inline typename fslist_const_iterator<dty_, nty_>::reference
fslist_const_iterator<dty_, nty_>::operator*() const noexcept
{
    auto c = static_cast<fslist_base<dty_, nty_>*>(
      const_cast<fslist_alloc_base<nty_>*>( container_ ) );
    return *c->get_arg( cur_ );
}

template <typename dty_, typename nty_>
inline typename fslist_const_iterator<dty_, nty_>::pointer
fslist_const_iterator<dty_, nty_>::operator->() const noexcept
{
    auto c = static_cast<fslist_base<dty_, nty_>*>(
      const_cast<fslist_alloc_base<nty_>*>( container_ ) );
    return c->get_arg( cur_ );
}

//! @}
//! @}
}; }; // namespace upp::impl
