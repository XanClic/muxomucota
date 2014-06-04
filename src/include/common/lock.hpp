#ifndef _LOCK_HPP
#define _LOCK_HPP

#include <sys/types.h>


namespace mu
{

class lock {
    public:
        lock(void);
        void init(void);

        void acquire(void);
        bool acquire_nonblock(void);
        void release(void);

        bool taken(void) const;

    private:
        pid_t holder;
};


#ifndef KERNEL

class rw_lock {
    public:
        rw_lock(void);
        void init(void);

        void acquire_r(void);
        void release_r(void);

        void acquire_w(void);
        void release_w(void);

        void require_w(void);
        void drop_w(void);

    private:
        lock write_lock, lock_lock;
        int reader_count;
};

#endif

}

#endif
