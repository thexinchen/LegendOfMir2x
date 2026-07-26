#pragma once
#include <cstddef>

#ifdef _WIN32
    #include <absl/container/inlined_vector.h>

namespace std
{
    template<typename T, size_t Capacity>
    using inplace_vector = absl::InlinedVector<T, Capacity>;
}
#else
    #include <inplace_vector>
#endif
