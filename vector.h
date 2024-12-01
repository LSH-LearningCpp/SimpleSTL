//
// Created by Lsh on 24-11-24.
//

#ifndef VECTOR_H
#define VECTOR_H

#include <initializer_list>
#include <limits>
#include <memory>

namespace Lsh {
    template<class T>
    class vector {
    public:
        using value_type             = T;
        using pointer                = T*;
        using const_pointer          = const T*;
        using reference              = T&;
        using const_reference        = const T&;
        using iterator               = value_type*;
        using const_iterator         = const value_type*;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using size_type              = std::size_t;
        using difference_type        = std::ptrdiff_t;

    private:
        /*
         * [1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0]
         *  |               |                     |
         *  start          finish           end_of_storage
         */
        pointer start_{nullptr};          // 指向第一个元素
        pointer finish_{nullptr};         // 指向最后一个元素的下一个位置
        pointer end_of_storage_{nullptr}; // 分配空间的末尾

    public:
        //===============================构造函数==============================
        // 默认构造函数
        vector() = default;

        // 构造拥有 count 个默认插入的 T 对象的 vector
        explicit vector(size_type count) {
            count = check_init_len(count);
            default_initialize(count);
        }

        // 构造拥有 count 个值为 value 的元素的 vector
        vector(size_type count, const_reference value) {
            count = check_init_len(count);
            fill_initialize(count, value);
        }

        // 以范围 [first,last) 的内容构造 vector
        /*
         * !!! 加上迭代器的类型检查，避免编译器匹配参数时把非迭代器类型
         *  匹配到 (first,last) ，发生错误
         *  例如，若没有迭代器类型检查，调用 vector(10,10)
         *  本意是调用 vector(size_type count, const_reference value)
         *  但由于编译器会首先匹配到更为泛型的函数，所以会首先调用迭代器版本
         *  这就与本意发生冲突，并产生错误
         * -- 只有满足输入迭代器要求的类型才会被接受，并且能够正确触发对应的重载。
         * -- 在后面的 assign 等需要重载迭代器版本的函数实现中同样要考虑这个问题
         * -- 实现方法来自标准库
         */
        template<class InputIterator, typename = std::_RequireInputIter<InputIterator>>
        vector(InputIterator first, InputIterator last) {
            ranges_initialize(first, last, std::__iterator_category(first));
        }

        // 拷贝构造函数
        vector(const vector& other) {
            create_storage(check_init_len(other.size()));
            finish_ = std::uninitialized_copy(other.begin(), other.end(), start_);
        }

        /*
         * 移动构造函数
         * -- “偷走资源”,而非复制资源
         * -- 接管资源,清空源对象
         */
        vector(vector&& other) noexcept : start_(other.start_), finish_(other.finish_),
                                          end_of_storage_(other.end_of_storage_) {
            other.start_          = nullptr;
            other.finish_         = nullptr;
            other.end_of_storage_ = nullptr;
        }

        // 等价于 vector(init.first,init.end)
        vector(std::initializer_list<T> init) {
            create_storage(check_init_len(init.size()));
            finish_ = std::uninitialized_copy(init.begin(), init.end(), start_);
        }

        //===============================析构函数==================================
        ~vector() {
            // 析构元素
            std::destroy(start_, finish_);
            // 释放内存
            using Tr = std::allocator_traits<std::allocator<T>>;
            std::allocator<T> alloc_;
            Tr::deallocate(alloc_, start_, size());
            start_          = nullptr;
            finish_         = nullptr;
            end_of_storage_ = nullptr;
        }

        //============================ operator= ==================================
        vector& operator=(const vector& other) {
            // 自赋值检查
            if (std::addressof(other) != this) {
                const size_type other_size = other.size();

                /*
                 * -- 当 other 的元素数量大于当前容量时,重新分配内存
                 * -- 容量足够时,复用内存
                 */
                if (other_size > capacity()) {
                    // 重新分配内存并复制元素
                    pointer new_start = allocate_and_copy(other_size, other.begin(), other.end());

                    // 销毁当前元素并释放内存
                    std::destroy(start_, finish_);
                    deallocate(start_, end_of_storage_ - start_);

                    // 更新指针和容量
                    start_          = new_start;
                    finish_         = new_start + other_size;
                    end_of_storage_ = new_start + other_size;
                } else {
                    // 如果目标对象更小,先销毁多余元素
                    if (size() >= other_size) {
                        std::destroy(std::copy(other.begin(), other.end(), begin()), end());
                    } else {
                        // 如果目标对象更大,扩展当前容器
                        std::copy(other.begin(), other.begin() + size(), begin());
                        std::uninitialized_copy(other.begin() + size(), other.end(), end());
                    }
                    finish_ = begin() + other_size;
                }
            }
            return *this;
        }

        vector& operator=(vector&& other) noexcept {
            if (std::addressof(other) != this) {
                // 释放当前资源
                this->clear();
                this->deallocate(this->start_, this->end_of_storage_ - this->start_);

                // 转移资源
                this->start_          = other.start_;
                this->finish_         = other.finish_;
                this->end_of_storage_ = other.end_of_storage_;

                // 清空源对象
                other.start_          = nullptr;
                other.finish_         = nullptr;
                other.end_of_storage_ = nullptr;
            }
            return *this;
        }

        vector& operator=(std::initializer_list<value_type> ilist) {
            // 实现同 vector& operator=(const vector& other)
            size_type ilen = ilist.size();
            if (ilen > capacity()) {
                pointer new_start = allocate_and_copy(ilen, ilist.begin(), ilist.end());
                std::destroy(start_, finish_);
                deallocate(start_, end_of_storage_ - start_);
                start_          = new_start;
                finish_         = new_start + ilen;
                end_of_storage_ = new_start + ilen;
            } else {
                if (size() >= ilen) {
                    std::destroy(std::copy(ilist.begin(), ilist.end(), begin()), end());
                } else {
                    std::copy(ilist.begin(), ilist.begin() + size(), begin());
                    std::uninitialized_copy(ilist.begin() + size(), ilist.end(), end());
                }
                finish_ = begin() + ilen;
            }
            return *this;
        }

        //=============================== assign ===============================
        void assign(size_type count, const value_type& value) {
            if (count > capacity()) {
                clear();
                deallocate(start_, end_of_storage_ - start_);
                create_storage(check_init_len(count));
                fill_initialize(count, value);
            } else {
                if (size() >= count) {
                    // 复制并清除掉多余部分
                    erase_at_end(std::fill_n(start_, count, value));
                } else {
                    std::fill(begin(), end(), value);
                    const size_type diff = count - size();
                    std::uninitialized_fill_n(finish_, diff, value);
                    finish_ = start_ + count;
                }
            }
        }

        template<class InputIterator, typename = std::_RequireInputIter<InputIterator>>
        void assign(InputIterator first, InputIterator last) {
            this->range_assign(first, last, std::__iterator_category(first));
        }

        void assign(std::initializer_list<value_type> ilist) {
            this->range_assign(ilist.begin(), ilist.end(),
                               std::random_access_iterator_tag());
        }

        //================================ 元素访问 =================================
        // at 带边界检查
        reference at(size_type index) {
            if (index >= size()) {
                throw std::out_of_range("vector::at");
            }
            return *(start_ + index);
        }

        const_reference at(size_type index) const {
            if (index >= size()) {
                throw std::out_of_range("vector::at");
            }
            return *(start_ + index);
        }

        // operator[] 不带边界检查
        reference operator[](size_type index) {
            return *(start_ + index);
        }

        const_reference operator[](size_type index) const {
            return *(start_ + index);
        }

        reference front() {
            return *start_;
        }

        const_reference front() const {
            return *start_;
        }

        reference back() {
            return *(finish_ - 1);
        }

        const_reference back() const {
            return *(finish_ - 1);
        }

        T* data() {
            return (start_);
        }

        const T* data() const {
            return (start_);
        }

        //================================ 迭代器 ==================================
        iterator begin() noexcept {
            return iterator(start_);
        }

        const_iterator begin() const noexcept {
            return const_iterator(start_);
        }

        const_iterator cbegin() const noexcept {
            return const_iterator(start_);
        }

        iterator end() noexcept {
            return iterator(finish_);
        }

        const_iterator end() const noexcept {
            return const_iterator(finish_);
        }

        const_iterator cend() const noexcept {
            return const_iterator(finish_);
        }

        reverse_iterator rbegin() noexcept {
            return reverse_iterator(end());
        }

        const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        }

        const_reverse_iterator crbegin() const noexcept {
            return const_reverse_iterator(end());
        }

        reverse_iterator rend() noexcept {
            return reverse_iterator(begin());
        }

        reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        }

        const_reverse_iterator crend() const noexcept {
            return const_reverse_iterator(begin());
        }

        //==================================== 容量 ================================
        [[nodiscard]] bool empty() const {
            return finish_ == start_;
        }

        [[nodiscard]] size_type size() const {
            return size_type(finish_ - start_);
        }

        // 能分配的最大内存
        [[nodiscard]] size_type max_size() const {
            using Tr           = std::allocator_traits<std::allocator<T>>;
            size_type diffmax  = std::numeric_limits<ptrdiff_t>::max() / sizeof(T);
            size_type allocmax = Tr::max_size(std::allocator<T>());
            return std::min(diffmax, allocmax);
        }

        // 增加 vector 的容量（即 vector 在不重新分配存储的情况下能最多能持有的元素的数量）
        // 到大于或等于 new_cap 的值。如果 new_cap 大于当前的 capacity()，
        // 那么就会分配新存储，否则该方法不做任何事。
        // 这里的实现标准库有内存迁移的部分，有点复杂，仅采用传统方式
        void reserve(size_type new_cap) {
            if (new_cap > this->max_size()) {
                throw std::length_error("vector::reserve(): new capacity too large");
            }
            // 重新分配
            if (this->capacity() < new_cap) {
                size_type old_size = this->size();
                pointer temp       = allocate_and_copy(new_cap, this->start_, this->finish_);
                std::destroy(this->start_, this->finish_);
                deallocate(this->start_, this->end_of_storage_ - this->start_);
                this->start_          = temp;
                this->finish_         = temp + old_size;
                this->end_of_storage_ = temp + new_cap;
            }
        }

        [[nodiscard]] size_type capacity() const {
            return size_type(end_of_storage_ - start_);
        }

        //请求移除未使用的容量。
        //它是减少 capacity() 到 size() 的非强制性请求。
        void shrink_to_fit() {
            if (this->capacity() > this->size()) {
                deallocate(this->finish_, this->end_of_storage_ - this->finish_);
                this->end_of_storage_ = this->finish_;
            }
        }

        //======================================================================
        //-------------------------------- 修改器 -------------------------------
        //======================================================================
        void clear() {
            erase_at_end(start_);
        }

        //================================ insert ===============================
        iterator insert(const_iterator pos, const value_type& value) {
            const auto diff = pos - cbegin();
            // 内存够,不扩容插入
            if (this->finish_ != this->end_of_storage_) {
                // 使用 begin() + (position - cbegin()) 保持类型兼容
                this->unrealloc_insert(begin() + (pos - cbegin()), value);
            } else {
                // 内存不够,扩容插入
                this->realloc_insert(begin() + (pos - cbegin()), value);
            }
            return iterator(this->start_ + diff);
        }

        iterator insert(const_iterator pos, value_type&& value) {
            const auto diff = pos - cbegin();
            if (this->finish_ != this->end_of_storage_) {
                this->unrealloc_insert(begin() + (pos - cbegin()), std::move(value));
            } else {
                this->realloc_insert(begin() + (pos - cbegin()), std::move(value));
            }
            return iterator(this->start_ + diff);
        }

        iterator insert(const_iterator pos, size_type count, const value_type& value) {
            const auto diff = pos - cbegin();
            this->fill_insert(begin() + diff, count, value);
            return iterator(this->start_ + diff);
        }

        template<class InputIterator, typename = std::_RequireInputIter<InputIterator>>
        iterator insert(const_iterator position, InputIterator first, InputIterator last) {
            const auto diff = position - cbegin();
            this->range_insert(begin() + diff, first, last, std::__iterator_category(first));
            return iterator(this->start_ + diff);
        }

        iterator insert(const_iterator position, std::initializer_list<value_type> ilist) {
            const auto diff = position - cbegin();
            this->range_insert(begin() + diff, ilist.begin(), ilist.end(),
                               std::random_access_iterator_tag());
            return iterator(this->start_ + diff);
        }

        //====================================== emplace =====================================
        template<class... Args>
        void emplace(const_iterator position, Args&&... args) {
            // 与 insert() 相同
            if (this->finish_ != this->end_of_storage_) {
                this->unrealloc_insert(begin() + (position - cbegin()), std::forward<Args>(args)...);
            } else {
                this->realloc_insert(begin() + (position - cbegin()), std::forward<Args>(args)...);
            }
        }

        template<class... Args>
        void emplace_back(Args&&... args) {
            if (this->finish_ != this->end_of_storage_) {
                typedef std::allocator_traits<std::allocator<T>> Tr;
                std::allocator<value_type> alloc_;
                Tr::construct(alloc_, this->finish_, std::forward<Args>(args)...);
                ++this->finish_;
            } else {
                this->realloc_insert(end(), std::forward<Args>(args)...);
            }
        }

        // C++17 起返回最后一个元素的引用
        /*template<class... Args>
        reference emplace_back(Args&&... args) {
            if (this->finish_ != this->end_of_storage_) {
                typedef std::iterator_traits<std::allocator<T>> Tr;
                std::allocator<T> alloc_;
                Tr::construct(alloc_, this->finish_, std::forward<Args>(args)...);
            } else {
                this->realloc_insert(end(), std::forward<Args>(args)...);
            }
            return back();
        }*/

        //=================================== erase ==========================================
        iterator erase(const_iterator pos) {
            // 转换迭代器类型
            iterator pos_ = begin() + (pos - cbegin());
            if ((pos_ + 1) != end()) {
                // [pos + 1,end()) 的元素正序依次复制到 {pos, pos++ ...}
                std::move(pos_ + 1, end(), pos_);
            }
            --this->finish_;
            using Tr = std::allocator_traits<std::allocator<T>>;
            std::allocator<value_type> alloc_;
            Tr::destroy(alloc_, this->finish_);
            return pos_;
        }

        iterator erase(const_iterator first, const_iterator last) {
            iterator first_ = begin() + (first - cbegin());
            iterator last_  = begin() + (last - cbegin());
            if (first_ != last_) {
                if (last_ != end()) {
                    std::move(last_, end(), first_);
                }
                this->erase_at_end(first_ + (end() - last_));
            }
            return first_;
        }

        //=================================== pop/push_back =================================
        void push_back(const value_type& value) {
            if (this->finish_ != this->end_of_storage_) {
                using Tr = std::allocator_traits<std::allocator<T>>;
                std::allocator<value_type> alloc_;
                Tr::construct(alloc_, this->finish_, value);
                ++this->finish_;
            } else {
                this->realloc_insert(end(), value);
            }
        }

        void push_back(value_type&& value) {
            emplace_back(std::move(value));
        }

        void pop_back() {
            --this->finish_;
            using Tr = std::allocator_traits<std::allocator<T>>;
            std::allocator<value_type> alloc_;
            Tr::destroy(alloc_, this->finish_);
        }

        //====================================== resize ========================================
        void resize(size_type new_size) {
            if (new_size > this->size()) {
                const size_type diff = new_size - this->size();
                this->default_append(diff);
            } else if (new_size < this->size()) {
                erase_at_end(begin() + new_size);
            }
        }

        void resize(size_type new_size, const value_type& value) {
            if (new_size > this->size()) {
                insert(end(), new_size - size(), value);
            } else if (new_size < this->size()) {
                erase_at_end(this->start_ + new_size);
            }
        }

        //====================================== swap ============================================
        void swap(vector& other) noexcept {
            using std::swap;  // 确保使用标准库的swap，避免可能的递归问题
            swap(start_, other.start_);
            swap(finish_, other.finish_);
            swap(end_of_storage_, other.end_of_storage_);
        }

    private:
        //==========================工具函数=======================
        // 分配大小为 count 的内存
        pointer allocate(size_type count) {
            using Tr = std::allocator_traits<std::allocator<T>>;
            std::allocator<T> alloc_;
            return count != 0 ? Tr::allocate(alloc_, count) : pointer();
        }

        // 回收内存
        void deallocate(pointer p, size_type count) {
            using Tr = std::allocator_traits<std::allocator<T>>;
            if (p) {
                std::allocator<value_type> alloc_;
                Tr::deallocate(alloc_, p, count);
            }
        }

        // 为 vector 创建大小为 count 的内存空间，未构造元素
        void create_storage(size_type count) {
            start_          = allocate(count);
            finish_         = start_;
            end_of_storage_ = start_ + count;
        }

        // 分配大小为 count 的内存空间,并把 [first,last) 的元素复制进去
        template<class ForwardIterator>
        pointer allocate_and_copy(size_type count, ForwardIterator first, ForwardIterator last) {
            pointer result = this->allocate(count);
            std::uninitialized_copy(first, last, result);
            return result;
        }

        // 把 [first,last) 内的元素赋值给 vector, 被 assign 调用
        template<class ForwardIterator>
        void range_assign(ForwardIterator first, ForwardIterator last, std::forward_iterator_tag) {
            const size_type len = std::distance(first, last);
            if (len > capacity()) {
                check_init_len(len);
                pointer new_start = allocate_and_copy(len, first, last);
                std::destroy(start_, finish_);
                deallocate(start_, end_of_storage_ - start_);

                start_          = new_start;
                finish_         = new_start + len;
                end_of_storage_ = new_start + len;
            } else {
                if (size() >= len) {
                    erase_at_end(std::copy(first, last, this->start_));
                } else {
                    /* 不要用 first + size() ,会导致类型不兼容*/
                    // std::copy(first, first + size(), this->start_);
                    // finish_ = std::uninitialized_copy(first + size(), last, this->finish_);
                    ForwardIterator mid = first;
                    std::advance(mid, size());
                    std::copy(first, mid, this->start_);
                    finish_ = std::uninitialized_copy(mid, last, this->finish_);
                }
            }
        }

        // 清除 [position,finish) 的元素
        void erase_at_end(pointer position) {
            if (size_type(finish_ - position)) {
                std::destroy(position, finish_);
                finish_ = position;
            }
        }

        // 对 vector 进行不扩容插入
        template<class... Args>
        void unrealloc_insert(iterator position, Args&&... args) {
            using Tr = std::allocator_traits<std::allocator<T>>;
            std::allocator<value_type> alloc_;
            if (position == end()) {
                Tr::construct(alloc_, this->finish_, std::forward<Args>(args)...);
                ++this->finish_;
            } else {
                Tr::construct(alloc_, this->finish_, *(this->finish_ - 1));
                for (iterator it = end() - 1; it != position; --it) {
                    *it = *(it - 1);
                }
                Tr::destroy(alloc_, position);
                Tr::construct(alloc_, position, std::forward<Args>(args)...);
                ++this->finish_;
            }
        }

        // 对 vector 进行扩容插入
        template<class... Args>
        void realloc_insert(iterator position, Args&&... args) {
            // 扩容
            const size_type new_capacity = check_len(size_type(1), "vector::realloc_insert()");
            pointer old_start            = this->start_;
            pointer old_finish           = this->finish_;
            const size_type elems_before = position - begin();
            pointer new_start            = allocate(new_capacity);

            // 构造新元素
            typedef std::allocator_traits<std::allocator<T>> Tr;
            std::allocator<value_type> alloc_;
            Tr::construct(alloc_, new_start + elems_before, std::forward<Args>(args)...);

            pointer new_finish = std::uninitialized_move(old_start, position, new_start);
            ++new_finish;
            new_finish = std::uninitialized_move(position, old_finish, new_finish);

            std::destroy(old_start, old_finish);
            deallocate(old_start, end_of_storage_ - old_start);

            this->start_          = new_start;
            this->finish_         = new_finish;
            this->end_of_storage_ = new_start + new_capacity;
        }

        void fill_insert(iterator position, size_type count, const value_type& value) {
            if (count != 0) {
                // 不需要扩容
                if (size_type(this->end_of_storage_ - this->finish_) > count) {
                    const size_type elems_after = finish_ - position;
                    pointer old_finish          = this->finish_;
                    if (elems_after > count) {
                        std::uninitialized_move(old_finish - count, old_finish, old_finish);
                        this->finish_ = old_finish + count;
                        // 将 [position,old_finish - count) 的元素倒序依次复制到 {old_finish,old_finish-- ...}
                        // 1111111 123456789abcdefgh 00000000000000000000000
                        //        |                 |
                        //   position          old_finish      count = 5
                        // 1111111 12345123456789abc 00000000000000000000000
                        std::move_backward(position, old_finish - count, old_finish);
                        std::fill(position, position + count, value);
                    } else {
                        this->finish_ = std::uninitialized_fill_n(old_finish, count - elems_after, value);
                        std::uninitialized_move(position, old_finish, this->finish_);
                        this->finish_ += elems_after;
                        std::fill(position, old_finish, value);
                    }
                } else {
                    // 扩容,实现方法类似 realloc_insert
                    const size_type new_capacity = check_len(size_type(count), "vector::fill_insert()");
                    pointer old_start            = this->start_;
                    pointer old_finish           = this->finish_;
                    pointer new_start            = allocate(new_capacity);
                    const size_type elems_before = position - begin();

                    std::uninitialized_fill_n(new_start + elems_before, count, value);
                    pointer new_finish = std::uninitialized_move(old_start, position, new_start);
                    new_finish += count;
                    new_finish = std::uninitialized_move(position, old_finish, new_finish);

                    std::destroy(old_start, old_finish);
                    deallocate(old_start, end_of_storage_ - old_start);

                    this->start_          = new_start;
                    this->finish_         = new_finish;
                    this->end_of_storage_ = new_start + new_capacity;
                }
            }
        }

        template<class InputIterator>
        void range_insert(iterator pos, InputIterator first, InputIterator last, std::input_iterator_tag) {
            // 实现逻辑同 fill_insert
            if (first != last) {
                const size_type n = std::distance(first, last);
                if (size_type(this->end_of_storage_ - this->finish_) > n) {
                    const size_type elems_after = finish_ - pos;
                    pointer old_finish          = this->finish_;
                    if (elems_after > n) {
                        std::uninitialized_move(old_finish - n, old_finish, old_finish);
                        this->finish_ = old_finish + n;
                        std::move_backward(pos, old_finish - n, old_finish);
                        std::copy(first, last, pos);
                    } else {
                        InputIterator mid = first;
                        std::advance(mid, elems_after);
                        this->finish_ = std::uninitialized_copy(mid, last, this->finish_);
                        std::uninitialized_move(pos, old_finish, this->finish_);
                        this->finish_ += elems_after;
                        std::copy(first, mid, pos);
                    }
                } else {
                    const size_type new_capacity = check_len(n, "vector::range_insert()");
                    pointer old_start            = this->start_;
                    pointer old_finish           = this->finish_;
                    pointer new_start            = allocate(new_capacity);
                    const size_type elems_before = pos - begin();

                    std::uninitialized_copy(first, last, new_start + elems_before);
                    pointer new_finish = std::uninitialized_move(old_start, pos, new_start);
                    new_finish += n;
                    new_finish = std::uninitialized_move(pos, old_finish, new_finish);

                    std::destroy(old_start, old_finish);
                    deallocate(old_start, end_of_storage_ - old_start);

                    this->start_          = new_start;
                    this->finish_         = new_finish;
                    this->end_of_storage_ = new_start + new_capacity;
                }
            }
        }

        // 在 end() 之后添加 n 个默认对象
        void default_append(size_type n) {
            if (n != 0) {
                const size_type size      = this->size();
                const size_type available = this->end_of_storage_ - this->finish_;
                if (available >= n) {
                    this->finish_ =
                            std::uninitialized_default_construct_n(this->finish_, n);
                } else {
                    pointer old_start            = this->start_;
                    pointer old_finish           = this->finish_;
                    const size_type new_capacity = check_len(n, "vector::default_append()");
                    pointer new_start            = allocate(new_capacity);
                    pointer new_finish           =
                            std::uninitialized_move(old_start, old_finish, new_start);
                    std::uninitialized_default_construct_n(new_finish, n);
                    std::destroy(old_start, old_finish);
                    deallocate(old_start, end_of_storage_ - old_start);
                    this->start_          = new_start;
                    this->finish_         = new_start + size + n;
                    this->end_of_storage_ = new_start + new_capacity;
                }
            }
        }

        /*
         * 在未初始化空间上构造元素
         * 被 vector 的各个构造函数调用
         */
        //被 vector(size_type count) 调用
        void default_initialize(size_type count) {
            create_storage(count);
            finish_ = std::uninitialized_default_construct_n(start_, count);
        }

        // 被 vector(szie_type count,const_reference value) 调用
        void fill_initialize(size_type count, const_reference value) {
            create_storage(count);
            finish_ = std::uninitialized_fill_n(start_, count, value);
        }

        // 被 vector(InputIterator first,InputIterator last) 调用
        template<class InputIterator>
        void ranges_initialize(InputIterator first, InputIterator last, std::input_iterator_tag) {
            size_type count = std::distance(first, last);
            create_storage(check_init_len(count));
            finish_ = std::uninitialized_copy_n(first, count, start_);
        }

        size_type check_init_len(size_type count) {
            if (count > max_size()) {
                throw std::length_error("vector size is greater than max_size()");
            }
            return count;
        }

        // 扩容时调用
        size_type check_len(size_type count, const char* s) const {
            if (max_size() < count + size()) {
                throw std::length_error(s);
            }
            // 一般情况下，二倍扩容
            const size_type len = size() + std::max(count, size());
            // 溢出或者超过 max_size() 时返回 max_size()
            return (len < size() || len > max_size()) ? max_size() : len;
        }
    };

    //==================================== 非成员函数 ==========================
    template<class T>
    bool operator==(const vector<T>& lhs, const vector<T>& rhs) {
        return (lhs.size() == rhs.size()) &&
               std::equal(lhs.begin(), lhs.end(), rhs.begin());
    }

    template<class T>
    bool operator!=(const vector<T>& lhs, const vector<T>& rhs) {
        return !(lhs == rhs);
    }

    template<class T>
    bool operator<(const vector<T>& lhs, const vector<T>& rhs) {
        return std::lexicographical_compare(lhs.begin(), lhs.end(),
                                            rhs.begin(), rhs.end());
    }

    template<class T>
    bool operator<=(const vector<T>& lhs, const vector<T>& rhs) {
        return !(rhs < lhs);
    }

    template<class T>
    bool operator>(const vector<T>& lhs, const vector<T>& rhs) {
        return rhs < lhs;
    }

    template<class T>
    bool operator>=(const vector<T>& lhs, const vector<T>& rhs) {
        return !(lhs < rhs);
    }

    template<class T>
    void swap(vector<T>& lhs, vector<T>& rhs) noexcept {
        lhs.swap(rhs);
    }
}
#endif //VECTOR_H
