/*****************************************************************************
 * vlc_threads_funcs.h : threads implementation for the VideoLAN client
 * This header provides a portable threads implementation.
 *****************************************************************************
 * Copyright (C) 1999-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _VLC_THREADFUNCS_H_
#define _VLC_THREADFUNCS_H_

#if defined( WIN32 ) && !defined ETIMEDOUT
#  define ETIMEDOUT 10060 /* This is the value in winsock.h. */
#endif

/*****************************************************************************
 * Function definitions
 *****************************************************************************/
VLC_EXPORT( int,  __vlc_mutex_init,    ( vlc_mutex_t * ) );
VLC_EXPORT( int,  __vlc_mutex_init_recursive, ( vlc_mutex_t * ) );
VLC_EXPORT( void,  __vlc_mutex_destroy, ( const char *, int, vlc_mutex_t * ) );
VLC_EXPORT( int,  __vlc_cond_init,     ( vlc_cond_t * ) );
VLC_EXPORT( void,  __vlc_cond_destroy,  ( const char *, int, vlc_cond_t * ) );
VLC_EXPORT( int, __vlc_threadvar_create, (vlc_threadvar_t * ) );
VLC_EXPORT( int,  __vlc_thread_create, ( vlc_object_t *, const char *, int, const char *, void * ( * ) ( void * ), int, bool ) );
VLC_EXPORT( int,  __vlc_thread_set_priority, ( vlc_object_t *, const char *, int, int ) );
VLC_EXPORT( void, __vlc_thread_ready,  ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_thread_join,   ( vlc_object_t *, const char *, int ) );

/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
#define vlc_mutex_init( P_MUTEX )                                   \
    __vlc_mutex_init( P_MUTEX )

/*****************************************************************************
 * vlc_mutex_init: initialize a recursive mutex (Don't use it)
 *****************************************************************************/
#define vlc_mutex_init_recursive( P_MUTEX )                         \
    __vlc_mutex_init_recursive( P_MUTEX )

/*****************************************************************************
 * vlc_mutex_lock: lock a mutex
 *****************************************************************************/
#define vlc_mutex_lock( P_MUTEX )                                           \
    __vlc_mutex_lock( __FILE__, __LINE__, P_MUTEX )

#if defined(LIBVLC_USE_PTHREAD)
VLC_EXPORT(void, vlc_pthread_fatal, (const char *action, int error, const char *file, unsigned line));

# define VLC_THREAD_ASSERT( action ) \
    if (val) \
        vlc_pthread_fatal (action, val, psz_file, i_line)
#else
# define VLC_THREAD_ASSERT (void)0
#endif

static inline void __vlc_mutex_lock( const char * psz_file, int i_line,
                                    vlc_mutex_t * p_mutex )
{
#if defined( UNDER_CE )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    EnterCriticalSection( &p_mutex->csection );

#elif defined( WIN32 )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    WaitForSingleObject( *p_mutex, INFINITE );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    acquire_sem( p_mutex->lock );

#elif defined(LIBVLC_USE_PTHREAD)
#   define vlc_assert_locked( m ) \
           assert (pthread_mutex_lock (m) == EDEADLK)
    int val = pthread_mutex_lock( p_mutex );
    VLC_THREAD_ASSERT ("locking mutex");

#endif
}

#ifndef vlc_assert_locked
# define vlc_assert_locked( m ) (void)0
#endif

/*****************************************************************************
 * vlc_mutex_unlock: unlock a mutex
 *****************************************************************************/
#define vlc_mutex_unlock( P_MUTEX )                                         \
    __vlc_mutex_unlock( __FILE__, __LINE__, P_MUTEX )

static inline void __vlc_mutex_unlock( const char * psz_file, int i_line,
                                      vlc_mutex_t *p_mutex )
{
#if defined( UNDER_CE )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    LeaveCriticalSection( &p_mutex->csection );

#elif defined( WIN32 )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    ReleaseMutex( *p_mutex );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    release_sem( p_mutex->lock );

#elif defined(LIBVLC_USE_PTHREAD)
    int val = pthread_mutex_unlock( p_mutex );
    VLC_THREAD_ASSERT ("unlocking mutex");

#endif
}

/*****************************************************************************
 * vlc_mutex_destroy: destroy a mutex
 *****************************************************************************/
#define vlc_mutex_destroy( P_MUTEX )                                        \
    __vlc_mutex_destroy( __FILE__, __LINE__, P_MUTEX )

/*****************************************************************************
 * vlc_cond_init: initialize a condition
 *****************************************************************************/
#define vlc_cond_init( P_THIS, P_COND )                                     \
    __vlc_cond_init( P_COND )

/*****************************************************************************
 * vlc_cond_signal: start a thread on condition completion
 *****************************************************************************/
#define vlc_cond_signal( P_COND )                                           \
    __vlc_cond_signal( __FILE__, __LINE__, P_COND )

static inline void __vlc_cond_signal( const char * psz_file, int i_line,
                                      vlc_cond_t *p_condvar )
{
#if defined( UNDER_CE ) || defined( WIN32 )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    /* Release one waiting thread if one is available. */
    /* For this trick to work properly, the vlc_cond_signal must be surrounded
     * by a mutex. This will prevent another thread from stealing the signal */
    /* PulseEvent() only works if none of the waiting threads is suspended.
     * This is particularily problematic under a debug session.
     * as documented in http://support.microsoft.com/kb/q173260/ */
    PulseEvent( p_condvar->event );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    while( p_condvar->thread != -1 )
    {
        thread_info info;
        if( get_thread_info(p_condvar->thread, &info) == B_BAD_VALUE )
            return;

        if( info.state != B_THREAD_SUSPENDED )
        {
            /* The  waiting thread is not suspended so it could
             * have been interrupted beetwen the unlock and the
             * suspend_thread line. That is why we sleep a little
             * before retesting p_condver->thread. */
            snooze( 10000 );
        }
        else
        {
            /* Ok, we have to wake up that thread */
            resume_thread( p_condvar->thread );
        }
    }

#elif defined(LIBVLC_USE_PTHREAD)
    int val = pthread_cond_signal( p_condvar );
    VLC_THREAD_ASSERT ("signaling condition variable");

#endif
}

/*****************************************************************************
 * vlc_cond_wait: wait until condition completion
 *****************************************************************************/
#define vlc_cond_wait( P_COND, P_MUTEX )                                     \
    __vlc_cond_wait( __FILE__, __LINE__, P_COND, P_MUTEX  )

static inline void __vlc_cond_wait( const char * psz_file, int i_line,
                                    vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex )
{
#if defined( UNDER_CE )
    p_condvar->i_waiting_threads++;
    LeaveCriticalSection( &p_mutex->csection );
    WaitForSingleObject( p_condvar->event, INFINITE );
    p_condvar->i_waiting_threads--;

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

#elif defined( WIN32 )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    /* Increase our wait count */
    p_condvar->i_waiting_threads++;
    SignalObjectAndWait( *p_mutex, p_condvar->event, INFINITE, FALSE );
    p_condvar->i_waiting_threads--;

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

#elif defined( HAVE_KERNEL_SCHEDULER_H )
    /* The p_condvar->thread var is initialized before the unlock because
     * it enables to identify when the thread is interrupted beetwen the
     * unlock line and the suspend_thread line */
    p_condvar->thread = find_thread( NULL );
    vlc_mutex_unlock( p_mutex );
    suspend_thread( p_condvar->thread );
    p_condvar->thread = -1;

    vlc_mutex_lock( p_mutex );

#elif defined(LIBVLC_USE_PTHREAD)
    int val = pthread_cond_wait( p_condvar, p_mutex );
    VLC_THREAD_ASSERT ("waiting on condition");

#endif
}


/*****************************************************************************
 * vlc_cond_timedwait: wait until condition completion or expiration
 *****************************************************************************
 * Returns 0 if object signaled, an error code in case of timeout or error.
 *****************************************************************************/
#define vlc_cond_timedwait( P_COND, P_MUTEX, DEADLINE )                      \
    __vlc_cond_timedwait( __FILE__, __LINE__, P_COND, P_MUTEX, DEADLINE  )

static inline int __vlc_cond_timedwait( const char * psz_file, int i_line,
                                        vlc_cond_t *p_condvar,
                                        vlc_mutex_t *p_mutex,
                                        mtime_t deadline )
{
#if defined( UNDER_CE )
    mtime_t delay_ms = (deadline - mdate())/1000;

    DWORD result;
    if( delay_ms < 0 )
        delay_ms = 0;

    p_condvar->i_waiting_threads++;
    LeaveCriticalSection( &p_mutex->csection );
    result = WaitForSingleObject( p_condvar->event, delay_ms );
    p_condvar->i_waiting_threads--;

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );

    if(result == WAIT_TIMEOUT)
       return ETIMEDOUT; /* this error is perfectly normal */

#elif defined( WIN32 )
    VLC_UNUSED( psz_file); VLC_UNUSED( i_line );

    DWORD result;

    mtime_t delay_ms = (deadline - mdate())/1000;
    if( delay_ms < 0 )
        delay_ms = 0;

    /* Increase our wait count */
    p_condvar->i_waiting_threads++;
    result = SignalObjectAndWait( *p_mutex, p_condvar->event,
                                  delay_ms, FALSE );
    p_condvar->i_waiting_threads--;

    /* Reacquire the mutex before returning. */
    vlc_mutex_lock( p_mutex );
    if(result == WAIT_TIMEOUT)
       return ETIMEDOUT; /* this error is perfectly normal */

#elif defined( HAVE_KERNEL_SCHEDULER_H )
#   error Unimplemented

#elif defined(LIBVLC_USE_PTHREAD)
    lldiv_t d = lldiv( deadline, 1000000 );
    struct timespec ts = { d.quot, d.rem * 1000 };

    int val = pthread_cond_timedwait (p_condvar, p_mutex, &ts);
    if (val == ETIMEDOUT)
        return ETIMEDOUT; /* this error is perfectly normal */
    VLC_THREAD_ASSERT ("timed-waiting on condition");

#endif

    return 0;
}

/*****************************************************************************
 * vlc_cond_destroy: destroy a condition
 *****************************************************************************/
#define vlc_cond_destroy( P_COND )                                          \
    __vlc_cond_destroy( __FILE__, __LINE__, P_COND )

/*****************************************************************************
 * vlc_threadvar_create: create a thread-local variable
 *****************************************************************************/
#define vlc_threadvar_create( PTHIS, P_TLS )                                 \
   __vlc_threadvar_create( P_TLS )

/*****************************************************************************
 * vlc_threadvar_set: create: set the value of a thread-local variable
 *****************************************************************************/
static inline int vlc_threadvar_set( vlc_threadvar_t * p_tls, void *p_value )
{
    int i_ret;

#if defined( HAVE_KERNEL_SCHEDULER_H )
    i_ret = EINVAL;

#elif defined( UNDER_CE ) || defined( WIN32 )
    i_ret = TlsSetValue( *p_tls, p_value ) ? EINVAL : 0;

#elif defined(LIBVLC_USE_PTHREAD)
    i_ret = pthread_setspecific( *p_tls, p_value );

#endif

    return i_ret;
}

/*****************************************************************************
 * vlc_threadvar_get: create: get the value of a thread-local variable
 *****************************************************************************/
static inline void* vlc_threadvar_get( vlc_threadvar_t * p_tls )
{
    void *p_ret;

#if defined( HAVE_KERNEL_SCHEDULER_H )
    p_ret = NULL;

#elif defined( UNDER_CE ) || defined( WIN32 )
    p_ret = TlsGetValue( *p_tls );

#elif defined(LIBVLC_USE_PTHREAD)
    p_ret = pthread_getspecific( *p_tls );

#endif

    return p_ret;
}

# if defined (_POSIX_SPIN_LOCKS) && ((_POSIX_SPIN_LOCKS - 0) > 0)
typedef struct
{
    pthread_spinlock_t  spin;
} vlc_spinlock_t;

/**
 * Initializes a spinlock.
 */
static inline int vlc_spin_init (vlc_spinlock_t *spin)
{
    return pthread_spin_init (&spin->spin, PTHREAD_PROCESS_PRIVATE);
}

/**
 * Acquires a spinlock.
 */
static inline void vlc_spin_lock (vlc_spinlock_t *spin)
{
    int val = pthread_spin_lock (&spin->spin);
    assert (val == 0);
    (void)val;
}

/**
 * Releases a spinlock.
 */
static inline void vlc_spin_unlock (vlc_spinlock_t *spin)
{
    int val = pthread_spin_unlock (&spin->spin);
    assert (val == 0);
    (void)val;
}

/**
 * Deinitializes a spinlock.
 */
static inline void vlc_spin_destroy (vlc_spinlock_t *spin)
{
    int val = pthread_spin_destroy (&spin->spin);
    assert (val == 0);
    (void)val;
}

#elif defined( WIN32 )

typedef CRITICAL_SECTION vlc_spinlock_t;

/**
 * Initializes a spinlock.
 */
static inline int vlc_spin_init (vlc_spinlock_t *spin)
{
    return !InitializeCriticalSectionAndSpinCount(spin, 4000);
}

/**
 * Acquires a spinlock.
 */
static inline void vlc_spin_lock (vlc_spinlock_t *spin)
{
    EnterCriticalSection(spin);
}

/**
 * Releases a spinlock.
 */
static inline void vlc_spin_unlock (vlc_spinlock_t *spin)
{
    LeaveCriticalSection(spin);
}

/**
 * Deinitializes a spinlock.
 */
static inline void vlc_spin_destroy (vlc_spinlock_t *spin)
{
    DeleteCriticalSection(spin);
}


#else

/* Fallback to plain mutexes if spinlocks are not available */
typedef vlc_mutex_t vlc_spinlock_t;

static inline int vlc_spin_init (vlc_spinlock_t *spin)
{
    return __vlc_mutex_init (spin);
}

# define vlc_spin_lock    vlc_mutex_lock
# define vlc_spin_unlock  vlc_mutex_unlock
# define vlc_spin_destroy vlc_mutex_destroy
#endif

/*****************************************************************************
 * vlc_thread_create: create a thread
 *****************************************************************************/
#define vlc_thread_create( P_THIS, PSZ_NAME, FUNC, PRIORITY, WAIT )         \
    __vlc_thread_create( VLC_OBJECT(P_THIS), __FILE__, __LINE__, PSZ_NAME, (void * ( * ) ( void * ))FUNC, PRIORITY, WAIT )

/*****************************************************************************
 * vlc_thread_set_priority: set the priority of the calling thread
 *****************************************************************************/
#define vlc_thread_set_priority( P_THIS, PRIORITY )                         \
    __vlc_thread_set_priority( VLC_OBJECT(P_THIS), __FILE__, __LINE__, PRIORITY )

/*****************************************************************************
 * vlc_thread_ready: tell the parent thread we were successfully spawned
 *****************************************************************************/
#define vlc_thread_ready( P_THIS )                                          \
    __vlc_thread_ready( VLC_OBJECT(P_THIS) )

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits
 *****************************************************************************/
#define vlc_thread_join( P_THIS )                                           \
    __vlc_thread_join( VLC_OBJECT(P_THIS), __FILE__, __LINE__ )

#endif
