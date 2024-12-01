//
// Created by Lsh on 24-11-24.
//

#ifndef CONSTRUCT_H
#define CONSTRUCT_H

#include <iterator>
#include <utility>

/*
 * This header file defines two utilities for constructing and destroying objects:
 *
 * 1. `construct`: Functions for constructing objects in allocated memory.
 *    - Includes overloads for default construction, copy construction, and variadic argument construction.
 *    - These functions allow placement new operations, ensuring that objects are correctly initialized.
 *
 * 2. `destroy`: Functions for safely destroying objects.
 *    - Handles single objects and ranges of objects using iterators.
 *    - Optimized for trivial types (e.g., POD types), avoiding unnecessary destructor calls.
 */

namespace Lsh {
    // Construct utilities:
    // - Supports default, copy, and custom argument-based construction.
    template<typename T>
    void construct(T* p) {
        ::new((void*)p) T();
    }

    template<typename T, typename U>
    void construct(T* p, const U& value) {
        ::new((void*)p) T(value);
    }

    template<typename T, typename... Args>
    void construct(T* p, Args&&... args) {
        ::new((void*)p) T(std::forward<Args>(args)...);
    }

    // Destroy utilities:
    // - Handles single objects and iterator ranges.
    // - Leverages type traits to optimize destruction of trivially destructible types.
    template<typename T>
    void destroy_1(T* p, std::true_type) {
    }

    template<typename T>
    void destroy_1(T* p, std::false_type) {
        p->~T();
    }

    template<typename ForwardIter>
    void destroy_2(ForwardIter, ForwardIter, std::true_type) {
    }

    template<typename ForwardIter>
    void destroy_2(ForwardIter first, ForwardIter last, std::false_type) {
        for (; first != last; ++first) {
            destroy(&*first);
        }
    }

    template<typename T>
    void destroy(T* p) {
        destroy_1(p, std::is_trivially_destructible<T>());
    }

    template<typename ForwardIter>
    void destroy(ForwardIter first, ForwardIter last) {
        destroy_2(first, last, std::is_trivially_destructible<
                      typename std::iterator_traits<ForwardIter>::value_type>());
    }
}
#endif //CONSTRUCT_H
