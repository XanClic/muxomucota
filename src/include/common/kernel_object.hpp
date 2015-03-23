#ifndef _KERNEL_OBJECT_HPP
#define _KERNEL_OBJECT_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <sudoku.hpp>


#ifdef KERNEL

namespace kobj_ids
{

enum values {
    shm_reference,
    popup_return_memory,
};

}

template<typename T> struct kobj_id {};

#define _(name) struct name; template<> struct kobj_id<name> { enum { id = kobj_ids::name }; };

_(shm_reference)
_(popup_return_memory)

#undef _


class kernel_object {
    public:
        kernel_object(size_t sz);

        operator uintptr_t(void)
        {
            refresh_checksum();
            return reinterpret_cast<uintptr_t>(this);
        }

        template<typename T> static T *fetch(uintptr_t address)
        {
            static_assert(std::is_base_of<kernel_object, T>::value, "kernel_object::fetch() can only be used for kernel objects");

            const kernel_object *kobj = reinterpret_cast<const kernel_object *>(address);
            if (!kobj->check_integrity()) {
                SUDOKU("Illegal kernel object referenced (integrity check failed)");
            }

            if (kobj->tid != kobj_id<T>::id) {
                SUDOKU("Illegal kernel object referenced (type mismatch)");
            }

            return reinterpret_cast<T *>(const_cast<kernel_object *>(kobj));
        }


    private:
        int tid;
        size_t size;
        uint32_t digest;

        bool check_integrity(void) const;
        void refresh_checksum(void);
};

#else

namespace mu
{

typedef uintptr_t kernel_object;

}

#endif

#endif
