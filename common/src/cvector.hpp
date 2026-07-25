#pragma once
#include <cstddef>
#include <initializer_list>

// constexpr-friendly span constructible from initializer_list
// replaces std::initializer_list as a struct member in constexpr aggregate arrays
// to avoid MSVC ICE (C1001) on large constexpr arrays with initializer_list members
//
// works because in constexpr evaluation the compiler tracks temporary backing arrays

template<typename T>
struct cvector
{
    const T *m_ptr  = nullptr;
    size_t   m_size = 0;

    constexpr cvector() = default;
    constexpr cvector(std::initializer_list<T> il) : m_ptr(il.begin()), m_size(il.size()) {}

    constexpr const T * begin()  const { return m_ptr; }
    constexpr const T * end()    const { return m_ptr + m_size; }
    constexpr size_t    size()   const { return m_size; }
    constexpr bool      empty()  const { return m_size == 0; }

    constexpr const T & operator[](size_t i) const { return m_ptr[i]; }
    constexpr const T & at(size_t i)       const { return m_ptr[i]; }

    constexpr const T * data() const { return m_ptr; }
};
