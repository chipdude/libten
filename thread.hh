#ifndef THREAD_HH
#define THREAD_HH

#include <ostream>
#include <signal.h>
#include <pthread.h>
#include <boost/utility.hpp>
#include "error.hh"

class mutex : boost::noncopyable {
public:
    mutex(const pthread_mutexattr_t *mutexattr = NULL) {
        pthread_mutex_init(&m, mutexattr);
    }

    ~mutex() {
        THROW_ON_NONZERO(pthread_mutex_destroy(&m));
    }

    class scoped_lock : boost::noncopyable {
    public:
        scoped_lock(mutex &m_, bool lock_=true) : m(m_) {
            if (lock_) { lock(); }
        }

        void lock() { m.lock(); }
        bool trylock() { return m.trylock(); }
        bool timedlock(const struct timespec &abs_timeout) { return m.timedlock(abs_timeout); }
        void unlock() { m.unlock(); }

        ~scoped_lock() {
            unlock();
        }
    protected:
        friend class condition;
        mutex &m;
    };

protected:
    friend class scoped_lock;

    void lock() {
        THROW_ON_NONZERO(pthread_mutex_lock(&m));
    }

    bool trylock() {
        int r = pthread_mutex_trylock(&m);
        if (r == 0) {
            return true;
        } else if (r == EBUSY) {
            return false;
        }
        THROW_ON_NONZERO(r);
        return false;
    }

    bool timedlock(const struct timespec &abs_timeout) {
        int r = pthread_mutex_timedlock(&m, &abs_timeout);
        if (r == 0) {
            return true;
        } else if (r == ETIMEDOUT) {
            return false;
        }
        THROW_ON_NONZERO(r);
        return false;
    }

    void unlock() {
        THROW_ON_NONZERO(pthread_mutex_unlock(&m));
    }
protected:
    friend class condition;
    pthread_mutex_t m;
};

class condition {
public:
    condition(const pthread_condattr_t *attr=NULL) {
        THROW_ON_NONZERO(pthread_cond_init(&c, attr));
    }

    void signal() {
        THROW_ON_NONZERO(pthread_cond_signal(&c));
    }

    void broadcast() {
        THROW_ON_NONZERO(pthread_cond_signal(&c));
    }

    void wait(mutex::scoped_lock &l) {
        THROW_ON_NONZERO(pthread_cond_wait(&c, &l.m.m));
    }

    ~condition() {
        THROW_ON_NONZERO(pthread_cond_destroy(&c));
    }
private:
    pthread_cond_t c;
};


namespace p {

// thin C++ wrapper around pthreads
struct thread {
    typedef void *(*proc)(void*);

    pthread_t id;

    explicit thread(pthread_t id_=0) : id(id_) {}

    void detach() {
        THROW_ON_NONZERO(pthread_detach(id));
    }

    void *join() {
        void *rvalue = NULL;
        THROW_ON_NONZERO(pthread_join(id, &rvalue));
        return rvalue;
    }

    void kill(int sig) {
        THROW_ON_NONZERO(pthread_kill(id, sig));
    }

    void cancel() {
        THROW_ON_NONZERO(pthread_cancel(id));
    }

    bool operator == (const thread &other) {
        return pthread_equal(id, other.id);
    }

    bool operator != (const thread &other) {
        return !pthread_equal(id, other.id);
    }

    friend std::ostream &operator <<(std::ostream &o, const thread &t) {
        o << t.id;
        return o;
    }

    static thread self() {
        return thread(pthread_self());
    }

    static void create(thread &t, proc start_routine, void *arg=NULL, const pthread_attr_t *attr=NULL) {
        THROW_ON_NONZERO(pthread_create(&t.id, attr, start_routine, arg));
    }
};

} // end namespace p

#endif // THREAD_HH
