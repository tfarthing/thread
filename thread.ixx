module;

#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <map>

#ifdef WIN32
#include <Windows.h>
#endif

export module cpp.thread;

export namespace cpp
{
    class Thread
    {
    public:
                                            Thread( ) noexcept;
                                            Thread( Thread && other ) noexcept;
                                            template< class Function, class... Args >
                                            Thread( Function && f, Args&&... args );
                                            Thread( const Thread & ) = delete;

        Thread &                            operator=( Thread && move ) noexcept;

        bool                                isRunning( ) const;
        void                                join( );
        void                                interrupt( );
        void                                check( );
        void                                detach( );
        void                                reset( );

        using                               id_t = std::thread::id;
        using                               duration_t = std::chrono::microseconds;

        static id_t                         id( );
        static void                         yield( );
        static void                         sleep( duration_t duration );
        static bool                         isInterrupted( );
        static void                         checkInterrupt( );
        static std::stop_token              stopToken( );

        static std::string                  name( );
        static void                         setName( std::string name );

    private:
        struct Info
        {
            typedef std::shared_ptr<Info>   ptr_t;

                                            Info( );
            void                            checkInterrupt( );
            void                            checkException( );

            std::string                     name;
            std::exception_ptr              exception;
            std::stop_token                 stopToken;
            std::map<void *, void *>        localData;
            std::map<void *, void *>        parentData;
        };
        static Info::ptr_t &                info( );

    private:
        std::jthread                        m_thread;
        Info::ptr_t                         m_info;
    };


    class Mutex
    {
    public:
        class Lock;

        Lock                                lock( );
        Lock                                tryLock( );

    private:
        std::mutex                          m_mutex;
        std::condition_variable_any         m_cv;
    };


    class Mutex::Lock
    {
    public:
        void                                lock( );
        void                                unlock( );
        bool                                hasLock( ) const;

        using                               time_point = std::chrono::steady_clock::time_point;
        using                               duration_t = std::chrono::microseconds;

        void                                wait( );
        std::cv_status                      waitUntil( time_point time );
        std::cv_status                      waitFor( duration_t duration );

        template<class Predicate>
        void                                wait( Predicate stopWaiting );
        template<class Predicate>
        bool                                waitUntil( time_point time, Predicate stopWaiting );
        template<class Predicate>
        bool                                waitFor( duration_t duration, Predicate stopWaiting );

        void                                notifyOne( );
        void                                notifyAll( );

    private:
        friend class Mutex;
                                            Lock( std::mutex & mutex, std::condition_variable_any & cv);
                                            Lock( std::mutex & mutex, std::condition_variable_any & cv, std::try_to_lock_t t );

    private:
        std::unique_lock<std::mutex>        m_lock;
        std::condition_variable_any &       m_cv;
    };


    struct InterruptException
        : public std::exception
    {
        InterruptException( );
    };
}


namespace cpp
{
    InterruptException::InterruptException( )
        : std::exception( "Thread execution was interrupted." ) 
    { 
    }


    Thread::Thread( ) noexcept
        : m_thread( ), m_info( nullptr )
    {
    }


    Thread::Thread( Thread && move ) noexcept
        : m_thread( std::move( move.m_thread ) ), m_info( move.m_info )
    {
    }


    template< class Function, class... Args >
    Thread::Thread( Function && f, Args &&... args )
        : m_info( nullptr )
    {
        Mutex mutex;
        auto lock = mutex.lock( );
        m_thread = std::jthread{ [&mutex, this, f, args...]( std::stop_token st )
        {
            auto lock = mutex.lock( );
            m_info = info( );
            m_info->stopToken = st;
            lock.unlock( );
            lock.notifyAll( );

            try
            {
                f( args... );
            }
            catch ( ... )
            {
                info( )->exception = std::current_exception( );
            }
        } };
        while ( !m_info )
        {
            lock.wait( );
        }
    }


    Thread & Thread::operator=( Thread && move ) noexcept
    {
        reset( );

        m_thread = std::move( move.m_thread );
        m_info = std::move( move.m_info );
        return *this;
    }


    inline bool Thread::isRunning( ) const
    {
        return m_thread.joinable( );
    }


    inline void Thread::join( )
    {
        if ( isRunning( ) )
            { m_thread.join( ); }
        check( );
    }


    inline void Thread::interrupt( )
    {
        m_thread.request_stop( );
    }


    inline void Thread::check( )
    {
        if ( m_info )
            { m_info->checkException( ); }
    }


    inline void Thread::detach( )
    {
        if ( isRunning( ) )
            { m_thread.detach( ); m_info = nullptr; }
    }


    inline void Thread::reset( )
    {
        if ( isRunning( ) )
        {
            interrupt( );
            if ( Thread::id( ) != m_thread.get_id( ) )
                { m_thread.join( ); }
            else
                { m_thread.detach( ); }
            m_info = nullptr;
        }
    }


    inline Thread::id_t Thread::id( )
    {
        return std::this_thread::get_id( );
    }


    inline void Thread::yield( )
    {
        checkInterrupt( ); 
        std::this_thread::yield( ); 
        checkInterrupt( );
    }


    inline void Thread::sleep( duration_t duration )
    {
        Mutex mutex; 
        auto lock = mutex.lock( ); 
        lock.waitFor( duration );
    }


    inline bool Thread::isInterrupted( )
    {
        return info( )->stopToken.stop_requested( );
    }


    inline void Thread::checkInterrupt( )
    {
        info( )->checkInterrupt( );
    }


    std::stop_token Thread::stopToken( )
    {
        return info( )->stopToken;
    }


    inline Thread::Info::ptr_t & Thread::info( )
    {
        static thread_local Info::ptr_t info = std::make_shared<Info>( );
        return info;
    }


    inline Thread::Info::Info( )
    {
    }


    inline void Thread::Info::checkInterrupt( )
    {
        if ( stopToken.stop_requested( ) )
        {
            throw InterruptException{ };
        }
    }


    inline void Thread::Info::checkException( )
    {
        if ( exception )
        {
            auto e = std::move( exception );
            exception = nullptr;
            std::rethrow_exception( e );
        }
    }


    inline std::string Thread::name( )
    {
        return info( )->name;
    }

    void setThreadName( const char * threadName )
    {
#ifdef WIN32
        struct THREADNAME_INFO
        {
            DWORD dwType; // must be 0x1000
            LPCSTR szName; // pointer to name (in user addr space)
            DWORD dwThreadID; // thread ID (-1=caller thread)
            DWORD dwFlags; // reserved for future use, must be zero
        };

        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = threadName;
        info.dwThreadID = -1;
        info.dwFlags = 0;

        __try
        {
            RaiseException( 0x406D1388, 0, sizeof( info ) / sizeof( DWORD ), (ULONG_PTR *)&info );
        }
        __except ( EXCEPTION_CONTINUE_EXECUTION )
        {
        }
#endif
    }

    inline void Thread::setName( std::string name )
    {
        info( )->name = name;
        setThreadName( name.c_str( ) );
    }


    inline Mutex::Lock Mutex::lock( )
        { return Lock( m_mutex, m_cv ); }


    inline Mutex::Lock Mutex::tryLock( )
        { return Lock( m_mutex, m_cv, std::try_to_lock_t{ } ); }


    inline Mutex::Lock::Lock( std::mutex & mutex, std::condition_variable_any & cv )
        : m_lock( mutex ), m_cv( cv )
    {
    }


    inline Mutex::Lock::Lock( std::mutex & mutex, std::condition_variable_any & cv, std::try_to_lock_t t )
        : m_lock( mutex, t ), m_cv( cv )
    {
    }


    inline bool Mutex::Lock::hasLock( ) const
    {
        return m_lock.owns_lock( );
    }


    inline void Mutex::Lock::lock( )
    {
        m_lock.lock( );
    }


    inline void Mutex::Lock::unlock( )
    {
        m_lock.unlock( );
    }


    void Mutex::Lock::wait( )
    {
        bool initial = true;
        wait( [&]( ) { return initial = !initial; } );
    }


    inline std::cv_status Mutex::Lock::waitFor( duration_t duration )
    {
        bool initial = true;
        return waitFor( duration, [&]( ) { return initial = !initial; } )
            ? std::cv_status::timeout
            : std::cv_status::no_timeout;
    }


    std::cv_status Mutex::Lock::waitUntil( time_point timeout )
    {
        bool initial = true;
        return waitUntil( timeout, [&]( ) { return initial = !initial; } )
            ? std::cv_status::timeout
            : std::cv_status::no_timeout;
    }


    template<class Predicate>
    void Mutex::Lock::wait( Predicate stopWaiting )
    {
        waitUntil( time_point::max( ), stopWaiting );
    }


    template<class Predicate> bool Mutex::Lock::waitFor( duration_t duration, Predicate stopWaiting )
    {
        bool isInfinite = duration == duration_t::max( );
        if ( duration <= duration_t::zero( ) )
            { Thread::checkInterrupt( ); return stopWaiting( ); }
        time_point timeout = isInfinite
            ? time_point::max( )
            : time_point::clock::now( ) + duration;

        return waitUntil( timeout, stopWaiting );
    }


    template<class Predicate>
    bool Mutex::Lock::waitUntil( time_point timeout, Predicate stopWaiting )
    {
        Thread::checkInterrupt( );
        bool result = m_cv.wait_until( m_lock, Thread::stopToken( ), timeout, stopWaiting );
        Thread::checkInterrupt( );
        return result;
    }


    inline void Mutex::Lock::notifyOne( )
    {
        m_cv.notify_one();
    }


    inline void Mutex::Lock::notifyAll( )
    {
        m_cv.notify_all( );
    }
}
