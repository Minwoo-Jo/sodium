/**
 * Copyright (c) 2012-2013, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */
#ifndef _SODIUM_TRANSACTION_H_
#define _SODIUM_TRANSACTION_H_

#include <sodium/count_set.h>
#include <sodium/light_ptr.h>
#include <sodium/lock_pool.h>
#include <boost/optional.hpp>
#include <boost/intrusive_ptr.hpp>
#include <sodium/unit.h>
#include <pthread.h>
#include <map>
#include <set>
#include <list>
#include <memory>
#if defined(NO_CXX11)
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/fusion/adapted/boost_tuple.hpp>
#include <boost/fusion/include/boost_tuple.hpp>
#else
#include <forward_list>
#include <tuple>
#endif
#include <limits.h>

#if defined(NO_CXX11)
#define EQ_DEF_PART
#define SODIUM_SHARED_PTR   boost::shared_ptr
#define SODIUM_MAKE_SHARED  boost::make_shared
#define SODIUM_WEAK_PTR     boost::weak_ptr
#define SODIUM_TUPLE        boost::tuple
#define SODIUM_MAKE_TUPLE   boost::make_tuple
#define SODIUM_TUPLE_GET    boost::get
#define SODIUM_FORWARD_LIST std::list
#else
#define EQ_DEF_PART = sodium::def_part
#define SODIUM_SHARED_PTR   std::shared_ptr
#define SODIUM_MAKE_SHARED  std::make_shared
#define SODIUM_WEAK_PTR     std::weak_ptr
#define SODIUM_TUPLE        std::tuple
#define SODIUM_MAKE_TUPLE   std::make_tuple
#define SODIUM_TUPLE_GET    std::get
#define SODIUM_FORWARD_LIST std::forward_list
#endif

namespace sodium {

#if defined(NO_CXX11)
    template <class A>
    struct i_lambda0
    {
        i_lambda0() {}
        virtual ~i_lambda0() {}

        virtual A operator () () const = 0;
    };

    template <class A>
    struct lambda0 {
        lambda0() {}
        lambda0(i_lambda0<A>* f) : f(f) {}
        A operator () () const { return (*f)(); }
        SODIUM_SHARED_PTR<i_lambda0<A> > f;
    };

    template <class A, class B>
    struct i_lambda1
    {
        i_lambda1() {}
        virtual ~i_lambda1() {}

        virtual A operator () (B b) const = 0;
    };

    template <class A, class B>
    struct lambda1 {
        lambda1() {}
        lambda1(i_lambda1<A,B>* f) : f(f) {}
        A operator () (B b) const { return (*f)(b); }
        SODIUM_SHARED_PTR<i_lambda1<A,B> > f;
    };

    template <class A, class B, class C>
    struct i_lambda2
    {
        i_lambda2() {}
        virtual ~i_lambda2() {}

        virtual A operator () (B b, C c) const = 0;
    };

    template <class A, class B, class C>
    struct lambda2 {
        lambda2() {}
        lambda2(i_lambda2<A,B,C>* f) : f(f) {}
        A operator () (B b, C c) const { return (*f)(b, c); }
        SODIUM_SHARED_PTR<i_lambda2<A,B,C> > f;
    };

    template <class A, class B, class C, class D>
    struct i_lambda3
    {
        i_lambda3() {}
        virtual ~i_lambda3() {}

        virtual A operator () (B b, C c, D d) const = 0;
    };

    template <class A, class B, class C, class D>
    struct lambda3 {
        lambda3() {}
        lambda3(i_lambda3<A,B,C,D>* f) : f(f) {}
        A operator () (B b, C c, D d) const { return (*f)(b, c, d); }
        SODIUM_SHARED_PTR<i_lambda3<A,B,C,D> > f;
    };

    template <class A, class B, class C, class D, class E>
    struct i_lambda4
    {
        i_lambda4() {}
        virtual ~i_lambda4() {}

        virtual A operator () (B b, C c, D d, E e) const = 0;
    };

    template <class A, class B, class C, class D, class E>
    struct lambda4 {
        lambda4() {}
        lambda4(i_lambda4<A,B,C,D,E>* f) : f(f) {}
        A operator () (B b, C c, D d, E e) const { return (*f)(b, c, d, e); }
        SODIUM_SHARED_PTR<i_lambda4<A,B,C,D,E> > f;
    };
#endif

    class mutex
    {
    private:
        pthread_mutex_t mx;
        // ensure we don't copy or assign a mutex by value
        mutex(const mutex& other) {}
        mutex& operator = (const mutex& other) { return *this; }
    public:
        mutex();
        ~mutex();
        void lock()
        {
            pthread_mutex_lock(&mx);
        }
        void unlock()
        {
            pthread_mutex_unlock(&mx);
        }
    };

    struct partition {
        partition();
        ~partition();
        mutex mx;
        int depth;
        pthread_key_t key;
        bool processing_post;
#if defined(NO_CXX11)
        std::list<lambda0<void> > postQ;
        void post(const lambda0<void>& action);
#else
        std::list<std::function<void()>> postQ;
        void post(const std::function<void()>& action);
#endif
        void process_post();
    };

    /*!
     * The default partition which gets chosen when you don't specify one.
     */
    struct def_part {
        static partition* part();
    };

    namespace impl {
        struct transaction_impl;

        typedef unsigned long rank_t;
        #define SODIUM_IMPL_RANK_T_MAX ULONG_MAX

        class node;
        template <class Allocator>
        struct listen_impl_func {
#if defined(NO_CXX11)
            typedef lambda4<lambda0<void>*,
                transaction_impl*,
                const SODIUM_SHARED_PTR<impl::node>&,
                lambda3<void, const SODIUM_SHARED_PTR<impl::node>&, transaction_impl*, const light_ptr&>*,
                bool> closure;
#else
            typedef std::function<std::function<void()>*(
                transaction_impl*,
                const std::shared_ptr<impl::node>&,
                std::function<void(const std::shared_ptr<impl::node>&, transaction_impl*, const light_ptr&)>*,
                bool)> closure;
#endif
            listen_impl_func(closure* func)
                : func(func) {}
            ~listen_impl_func()
            {
                assert(cleanups.begin() == cleanups.end() && func == NULL);
            }
            count_set counts;
            closure* func;
#if defined(NO_CXX11)
            SODIUM_FORWARD_LIST<lambda0<void>*> cleanups;
#else
            SODIUM_FORWARD_LIST<std::function<void()>*> cleanups;
#endif
            inline void update_and_unlock(spin_lock* l) {
                if (func && !counts.active()) {
                    counts.inc_strong();
                    l->unlock();
#if defined(NO_CXX11)
                    for (std::list<lambda0<void>*>::iterator it = cleanups.begin(); it != cleanups.end(); ++it) {
#else
                    for (auto it = cleanups.begin(); it != cleanups.end(); ++it) {
#endif
                        (**it)();
                        delete *it;
                    }
                    cleanups.clear();
                    delete func;
                    func = NULL;
                    l->lock();
                    counts.dec_strong();
                }
                if (!counts.alive()) {
                    l->unlock();
                    delete this;
                }
                else
                    l->unlock();
            }
        };

        struct H_EVENT {};
        struct H_STRONG {};
        struct H_NODE {};

        void intrusive_ptr_add_ref(sodium::impl::listen_impl_func<sodium::impl::H_EVENT>* p);
        void intrusive_ptr_release(sodium::impl::listen_impl_func<sodium::impl::H_EVENT>* p);
        void intrusive_ptr_add_ref(sodium::impl::listen_impl_func<sodium::impl::H_STRONG>* p);
        void intrusive_ptr_release(sodium::impl::listen_impl_func<sodium::impl::H_STRONG>* p);
        void intrusive_ptr_add_ref(sodium::impl::listen_impl_func<sodium::impl::H_NODE>* p);
        void intrusive_ptr_release(sodium::impl::listen_impl_func<sodium::impl::H_NODE>* p);

        inline bool alive(const boost::intrusive_ptr<listen_impl_func<H_STRONG> >& li) {
            return li && li->func != NULL;
        }

        inline bool alive(const boost::intrusive_ptr<listen_impl_func<H_EVENT> >& li) {
            return li && li->func != NULL;
        }

        class node
        {
            public:
                struct target {
                    target(
                        void* h,
                        const SODIUM_SHARED_PTR<node>& n
                    ) : h(h),
                        n(n) {}

                    void* h;
                    SODIUM_SHARED_PTR<node> n;
                };

            public:
                node();
                node(rank_t rank);
                ~node();

                rank_t rank;
                SODIUM_FORWARD_LIST<node::target> targets;
                SODIUM_FORWARD_LIST<light_ptr> firings;
                SODIUM_FORWARD_LIST<boost::intrusive_ptr<listen_impl_func<H_EVENT> > > sources;
                boost::intrusive_ptr<listen_impl_func<H_NODE> > listen_impl;

                void link(void* holder, const SODIUM_SHARED_PTR<node>& target);
                void unlink(void* holder);

            private:
                void ensure_bigger_than(std::set<node*>& visited, rank_t limit);
        };
    }
}

namespace sodium {
    namespace impl {

        template <class A>
        struct ordered_value {
            ordered_value() : tid(-1) {}
            long long tid;
            boost::optional<A> oa;
        };

        struct entryID {
            entryID() : id(0) {}
            entryID(rank_t id) : id(id) {}
            rank_t id;
            entryID succ() const { return entryID(id+1); }
            inline bool operator < (const entryID& other) const { return id < other.id; }
        };

        rank_t rankOf(const SODIUM_SHARED_PTR<node>& target);

        struct prioritized_entry {
#if defined(NO_CXX11)
            prioritized_entry(const SODIUM_SHARED_PTR<node>& target,
                              const lambda1<void, transaction_impl*>& action)
#else
            prioritized_entry(const SODIUM_SHARED_PTR<node>& target,
                              const std::function<void(transaction_impl*)>& action)
#endif
                : target(target), action(action)
            {
            }
            SODIUM_SHARED_PTR<node> target;
#if defined(NO_CXX11)
            lambda1<void, transaction_impl*> action;
#else
            std::function<void(transaction_impl*)> action;
#endif
        };

        struct transaction_impl {
            transaction_impl(partition* part);
            ~transaction_impl();
            partition* part;
            entryID next_entry_id;
            std::map<entryID, prioritized_entry> entries;
            std::multimap<rank_t, entryID> prioritizedQ;
#if defined(NO_CXX11)
            std::list<lambda0<void> > lastQ;
#else
            std::list<std::function<void()>> lastQ;
#endif

            void prioritized(const SODIUM_SHARED_PTR<impl::node>& target,
#if defined(NO_CXX11)
                             const lambda1<void, impl::transaction_impl*>& action);
            void last(const lambda0<void>& action);
#else
                             const std::function<void(impl::transaction_impl*)>& action);
            void last(const std::function<void()>& action);
#endif

            bool to_regen;
            void check_regen();
            void process_transactional();
        };
    };

    class policy {
    public:
        policy() {}
        virtual ~policy() {}

        static policy* get_global();
        static void set_global(policy* policy);

        /*!
         * Get the current thread's active transaction for this partition, or NULL
         * if none is active.
         */
        virtual impl::transaction_impl* current_transaction(partition* part) = 0;

        virtual void initiate(impl::transaction_impl* impl) = 0;

        /*!
         * Dispatch the processing for this transaction according to the policy.
         * Note that post() will delete impl, so don't reference it after that.
         */
#if defined(NO_CXX11)
        virtual void dispatch(impl::transaction_impl* impl,
            const lambda0<void>& transactional,
            const lambda0<void>& post) = 0;
#else
        virtual void dispatch(impl::transaction_impl* impl,
            const std::function<void()>& transactional,
            const std::function<void()>& post) = 0;
#endif
    };

    namespace impl {
        class transaction_ {
        private:
            transaction_impl* impl_;
            transaction_(const transaction_& other) {}
            transaction_& operator = (const transaction_& other) { return *this; };
        public:
            transaction_(partition* part);
            ~transaction_();
            impl::transaction_impl* impl() const { return impl_; }
        };
    };

    template <class P = def_part>
    class transaction : public impl::transaction_
    {
        private:
            transaction(const transaction<P>& other) {}
            transaction<P>& operator = (const transaction<P>& other) { return *this; };
        public:
            transaction() : transaction_(P::part()) {}
    };

    class simple_policy : public policy
    {
    public:
        simple_policy();
        virtual ~simple_policy();
        virtual impl::transaction_impl* current_transaction(partition* part);
        virtual void initiate(impl::transaction_impl* impl);
        virtual void dispatch(impl::transaction_impl* impl,
#if defined(NO_CXX11)
            const lambda0<void>& transactional,
            const lambda0<void>& post);
#else
            const std::function<void()>& transactional,
            const std::function<void()>& post);
#endif
    };
}  // end namespace sodium

#endif

