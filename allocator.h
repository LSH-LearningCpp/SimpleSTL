//
// Created by Lsh on 24-11-24.
//

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <cstddef>
#include <limits>
#include "construct.h"

namespace Lsh {
    /**
    * @brief A custom allocator class for managing memory.
    *
    * This class provides basic memory management functions, including:
    * - Allocating and deallocating memory for objects using new/delete
    * - Constructing objects in allocated memory using placement new.
    * - Destroying objects safely, with optimizations for trivial destructors.
    * - Rebinding to allocate memory for different types.
    *
    * @tparam T The type of objects this allocator will manage.
    */
    template<class T>
    class allocator {
    public:
        // Type definitions for convenience and compatibility
        typedef T              value_type;
        typedef T*             pointer;
        typedef const T*       const_pointer;
        typedef T&             reference;
        typedef const T&       const_reference;
        typedef std::size_t    size_type;
        typedef std::ptrdiff_t difference_type;

        template<class U>
        struct rebind {
            typedef allocator<U> other;
        };

    public:
        allocator() noexcept = default;

        ~allocator() = default;


        //Returns the actual address of the object, even if operator& is overloaded.
        pointer address(reference x) const noexcept;

        const_pointer address(const_reference x) const noexcept;

        [[nodiscard]] static size_type max_size() noexcept;

        static pointer allocate(size_type n);

        static void deallocate(pointer p, size_type n);

        void construct(pointer p);

        void construct(pointer p, const_reference x);

        template<class U, class... Args>
        void construct(U* p, Args&&... args);

        void destroy(pointer p);

        void destroy(pointer first, pointer last);
    };

    template<class T>
    typename allocator<T>::pointer allocator<T>::address(reference x) const noexcept {
        return std::addressof(x);
    }

    template<class T>
    typename allocator<T>::const_pointer allocator<T>::address(const_reference x) const noexcept {
        return std::addressof(x);
    }

    template<class T>
    typename allocator<T>::size_type allocator<T>::max_size() noexcept {
        return std::numeric_limits<size_type>::max() / sizeof(value_type);
    }

    template<class T>
    typename allocator<T>::pointer allocator<T>::allocate(size_type n) {
        return static_cast<pointer>(::operator new(n * sizeof(T)));
    }

    template<class T>
    void allocator<T>::deallocate(pointer p, size_type n) {
        ::operator delete(p, n * sizeof(T));
    }

    template<class T>
    void allocator<T>::construct(pointer p) {
        Lsh::construct(p);
    }

    template<class T>
    void allocator<T>::construct(pointer p, const_reference x) {
        Lsh::construct(p, x);
    }

    template<class T>
    template<class U, class... Args>
    void allocator<T>::construct(U* p, Args&&... args) {
        Lsh::construct(p, std::forward<Args>(args)...);
    }

    template<class T>
    void allocator<T>::destroy(pointer p) {
        Lsh::destroy(p);
    }

    template<class T>
    void allocator<T>::destroy(pointer first, pointer last) {
        Lsh::destroy(first, last);
    }
}
#endif //ALLOCATOR_H
