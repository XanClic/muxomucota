#ifndef _PARITH_HPP
#define _PARITH_HPP

#include <cstddef>
#include <cstdint>


template<typename T, typename A = uint8_t> class ptr_diff_arith {
    public:
        ptr_diff_arith(T *from): ptr(from) {}

        ptr_diff_arith<T, A> operator+(ptrdiff_t diff)
        { return ptr_diff_arith<T, A>(reinterpret_cast<T *>(reinterpret_cast<A *>(ptr) + diff)); }

        ptr_diff_arith<T, A> operator-(ptrdiff_t diff)
        { return ptr_diff_arith<T, A>(reinterpret_cast<T *>(reinterpret_cast<A *>(ptr) - diff)); }

        ptrdiff_t operator-(const ptr_diff_arith<T, A> &o)
        { return reinterpret_cast<A *>(ptr) - reinterpret_cast<A *>(o.ptr); }

        operator T *(void) { return ptr; }


    private:
        T *ptr;
};


template<typename T> ptr_diff_arith<T, uint8_t> byte_ptr(T *ptr)
{ return ptr_diff_arith<T, uint8_t>(ptr); }

#endif
