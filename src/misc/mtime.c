/*****************************************************************************
 * mtime.c: high resolution time management functions
 * Functions are prototyped in vlc_mtime.h.
 *****************************************************************************
 * Copyright (C) 1998-2007 the VideoLAN team
 * Copyright © 2006-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Rémi Denis-Courmont <rem$videolan,org>
 *          Gisle Vanem
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>

#include <stdio.h>                                              /* sprintf() */
#include <time.h>                      /* clock_gettime(), clock_nanosleep() */
#include <stdlib.h>                                               /* lldiv() */
#include <assert.h>


#if defined( PTH_INIT_IN_PTH_H )                                  /* GNU Pth */
#   include <pth.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                           /* select() */
#endif

#ifdef HAVE_KERNEL_OS_H
#   include <kernel/OS.h>
#endif

#if defined( WIN32 ) || defined( UNDER_CE )
#   include <windows.h>
#endif
#if defined(HAVE_SYS_TIME_H)
#   include <sys/time.h>
#endif

#if !defined(HAVE_STRUCT_TIMESPEC)
struct timespec
{
    time_t  tv_sec;
    int32_t tv_nsec;
};
#endif

#if defined(HAVE_NANOSLEEP) && !defined(HAVE_DECL_NANOSLEEP)
int nanosleep(struct timespec *, struct timespec *);
#endif

/**
 * Return a date in a readable format
 *
 * This function converts a mtime date into a string.
 * psz_buffer should be a buffer long enough to store the formatted
 * date.
 * \param date to be converted
 * \param psz_buffer should be a buffer at least MSTRTIME_MAX_SIZE characters
 * \return psz_buffer is returned so this can be used as printf parameter.
 */
char *mstrtime( char *psz_buffer, mtime_t date )
{
    static mtime_t ll1000 = 1000, ll60 = 60, ll24 = 24;

    snprintf( psz_buffer, MSTRTIME_MAX_SIZE, "%02d:%02d:%02d-%03d.%03d",
             (int) (date / (ll1000 * ll1000 * ll60 * ll60) % ll24),
             (int) (date / (ll1000 * ll1000 * ll60) % ll60),
             (int) (date / (ll1000 * ll1000) % ll60),
             (int) (date / ll1000 % ll1000),
             (int) (date % ll1000) );
    return( psz_buffer );
}

/**
 * Convert seconds to a time in the format h:mm:ss.
 *
 * This function is provided for any interface function which need to print a
 * time string in the format h:mm:ss
 * date.
 * \param secs  the date to be converted
 * \param psz_buffer should be a buffer at least MSTRTIME_MAX_SIZE characters
 * \return psz_buffer is returned so this can be used as printf parameter.
 */
char *secstotimestr( char *psz_buffer, int i_seconds )
{
    snprintf( psz_buffer, MSTRTIME_MAX_SIZE, "%d:%2.2d:%2.2d",
              (int) (i_seconds / (60 *60)),
              (int) ((i_seconds / 60) % 60),
              (int) (i_seconds % 60) );
    return( psz_buffer );
}

/**
 * Return a value that is no bigger than the clock precision
 * (possibly zero).
 */
static inline unsigned mprec( void )
{
#if defined (HAVE_CLOCK_NANOSLEEP)
    struct timespec ts;
    if( clock_getres( CLOCK_MONOTONIC, &ts ))
        clock_getres( CLOCK_REALTIME, &ts );

    return ts.tv_nsec / 1000;
#endif
    return 0;
}

static unsigned prec = 0;
static volatile mtime_t cached_time = 0;
#if (_POSIX_MONOTONIC_CLOCK - 0 < 0)
# define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

/**
 * Return high precision date
 *
 * Uses the gettimeofday() function when possible (1 MHz resolution) or the
 * ftime() function (1 kHz resolution).
 */
mtime_t mdate( void )
{
    mtime_t res;

#if defined (HAVE_CLOCK_NANOSLEEP)
    struct timespec ts;

    /* Try to use POSIX monotonic clock if available */
    if( clock_gettime( CLOCK_MONOTONIC, &ts ) == EINVAL )
        /* Run-time fallback to real-time clock (always available) */
        (void)clock_gettime( CLOCK_REALTIME, &ts );

    res = ((mtime_t)ts.tv_sec * (mtime_t)1000000)
           + (mtime_t)(ts.tv_nsec / 1000);

#elif defined( HAVE_KERNEL_OS_H )
    res = real_time_clock_usecs();

#elif defined( WIN32 ) || defined( UNDER_CE )
    /* We don't need the real date, just the value of a high precision timer */
    static mtime_t freq = I64C(-1);
    mtime_t usec_time;

    if( freq == I64C(-1) )
    {
        /* Extract from the Tcl source code:
         * (http://www.cs.man.ac.uk/fellowsd-bin/TIP/7.html)
         *
         * Some hardware abstraction layers use the CPU clock
         * in place of the real-time clock as a performance counter
         * reference.  This results in:
         *    - inconsistent results among the processors on
         *      multi-processor systems.
         *    - unpredictable changes in performance counter frequency
         *      on "gearshift" processors such as Transmeta and
         *      SpeedStep.
         * There seems to be no way to test whether the performance
         * counter is reliable, but a useful heuristic is that
         * if its frequency is 1.193182 MHz or 3.579545 MHz, it's
         * derived from a colorburst crystal and is therefore
         * the RTC rather than the TSC.  If it's anything else, we
         * presume that the performance counter is unreliable.
         */
        LARGE_INTEGER buf;

        freq = ( QueryPerformanceFrequency( &buf ) &&
                 (buf.QuadPart == I64C(1193182) || buf.QuadPart == I64C(3579545) ) )
               ? buf.QuadPart : 0;
    }

    if( freq != 0 )
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter (&counter);

        /* Convert to from (1/freq) to microsecond resolution */
        /* We need to split the division to avoid 63-bits overflow */
        lldiv_t d = lldiv (counter.QuadPart, freq);

        res = (d.quot * 1000000) + ((d.rem * 1000000) / freq);
    }
    else
    {
        /* Fallback on GetTickCount() which has a milisecond resolution
         * (actually, best case is about 10 ms resolution)
         * GetTickCount() only returns a DWORD thus will wrap after
         * about 49.7 days so we try to detect the wrapping. */

        static CRITICAL_SECTION date_lock;
        static mtime_t i_previous_time = I64C(-1);
        static int i_wrap_counts = -1;

        if( i_wrap_counts == -1 )
        {
            /* Initialization */
            i_previous_time = I64C(1000) * GetTickCount();
            InitializeCriticalSection( &date_lock );
            i_wrap_counts = 0;
        }

        EnterCriticalSection( &date_lock );
        res = I64C(1000) *
            (i_wrap_counts * I64C(0x100000000) + GetTickCount());
        if( i_previous_time > res )
        {
            /* Counter wrapped */
            i_wrap_counts++;
            usec_time += I64C(0x100000000) * 1000;
        }
        i_previous_time = usec_time;
        LeaveCriticalSection( &date_lock );
    }
#else
    struct timeval tv_date;

    /* gettimeofday() cannot fail given &tv_date is a valid address */
    (void)gettimeofday( &tv_date, NULL );
    res = (mtime_t) tv_date.tv_sec * 1000000 + (mtime_t) tv_date.tv_usec;
#endif

    return cached_time = res;
}

/**
 * Wait for a date
 *
 * This function uses select() and an system date function to wake up at a
 * precise date. It should be used for process synchronization. If current date
 * is posterior to wished date, the function returns immediately.
 * \param date The date to wake up at
 */
void mwait( mtime_t date )
{
    if( prec == 0 )
        prec = mprec();

    /* If the deadline is already elapsed, or within the clock precision,
     * do not even bother the clock. */
    if( ( date - cached_time ) < (mtime_t)prec ) // OK: mtime_t is signed
        return;

#if 0 && defined (HAVE_CLOCK_NANOSLEEP)
    lldiv_t d = lldiv( date, 1000000 );
    struct timespec ts = { d.quot, d.rem * 1000 };

    int val;
    while( ( val = clock_nanosleep( CLOCK_MONOTONIC, TIMER_ABSTIME, &ts,
                                    NULL ) ) == EINTR );
    if( val == EINVAL )
    {
        ts.tv_sec = d.quot; ts.tv_nsec = d.rem * 1000;
        while( clock_nanosleep( CLOCK_REALTIME, 0, &ts, NULL ) == EINTR );
    }
#else

    mtime_t delay = date - mdate();
    if( delay > 0 )
        msleep( delay );

#endif
}

/**
 * More precise sleep()
 *
 * Portable usleep() function.
 * \param delay the amount of time to sleep
 */
void msleep( mtime_t delay )
{
    mtime_t earlier = cached_time;

#if defined( HAVE_CLOCK_NANOSLEEP ) 
    lldiv_t d = lldiv( delay, 1000000 );
    struct timespec ts = { d.quot, d.rem * 1000 };

    int val;
    while( ( val = clock_nanosleep( CLOCK_MONOTONIC, 0, &ts, &ts ) ) == EINTR );
    if( val == EINVAL )
    {
        ts.tv_sec = d.quot; ts.tv_nsec = d.rem * 1000;
        while( clock_nanosleep( CLOCK_REALTIME, 0, &ts, &ts ) == EINTR );
    }

#elif defined( HAVE_KERNEL_OS_H )
    snooze( delay );

#elif defined( PTH_INIT_IN_PTH_H )
    pth_usleep( delay );

#elif defined( ST_INIT_IN_ST_H )
    st_usleep( delay );

#elif defined( WIN32 ) || defined( UNDER_CE )
    Sleep( (int) (delay / 1000) );

#elif defined( HAVE_NANOSLEEP )
    struct timespec ts_delay;

    ts_delay.tv_sec = delay / 1000000;
    ts_delay.tv_nsec = (delay % 1000000) * 1000;

    while( nanosleep( &ts_delay, &ts_delay ) && ( errno == EINTR ) );

#else
    struct timeval tv_delay;

    tv_delay.tv_sec = delay / 1000000;
    tv_delay.tv_usec = delay % 1000000;

    /* If a signal is caught, you are screwed. Update your OS to nanosleep()
     * or clock_nanosleep() if this is an issue. */
    select( 0, NULL, NULL, NULL, &tv_delay );
#endif

    earlier += delay;
    if( cached_time < earlier )
        cached_time = earlier;
}

/*
 * Date management (internal and external)
 */

/**
 * Initialize a date_t.
 *
 * \param date to initialize
 * \param divider (sample rate) numerator
 * \param divider (sample rate) denominator
 */

void date_Init( date_t *p_date, uint32_t i_divider_n, uint32_t i_divider_d )
{
    p_date->date = 0;
    p_date->i_divider_num = i_divider_n;
    p_date->i_divider_den = i_divider_d;
    p_date->i_remainder = 0;
}

/**
 * Change a date_t.
 *
 * \param date to change
 * \param divider (sample rate) numerator
 * \param divider (sample rate) denominator
 */

void date_Change( date_t *p_date, uint32_t i_divider_n, uint32_t i_divider_d )
{
    p_date->i_divider_num = i_divider_n;
    p_date->i_divider_den = i_divider_d;
}

/**
 * Set the date value of a date_t.
 *
 * \param date to set
 * \param date value
 */
void date_Set( date_t *p_date, mtime_t i_new_date )
{
    p_date->date = i_new_date;
    p_date->i_remainder = 0;
}

/**
 * Get the date of a date_t
 *
 * \param date to get
 * \return date value
 */
mtime_t date_Get( const date_t *p_date )
{
    return p_date->date;
}

/**
 * Move forwards or backwards the date of a date_t.
 *
 * \param date to move
 * \param difference value
 */
void date_Move( date_t *p_date, mtime_t i_difference )
{
    p_date->date += i_difference;
}

/**
 * Increment the date and return the result, taking into account
 * rounding errors.
 *
 * \param date to increment
 * \param incrementation in number of samples
 * \return date value
 */
mtime_t date_Increment( date_t *p_date, uint32_t i_nb_samples )
{
    mtime_t i_dividend = (mtime_t)i_nb_samples * 1000000;
    p_date->date += i_dividend / p_date->i_divider_num * p_date->i_divider_den;
    p_date->i_remainder += (int)(i_dividend % p_date->i_divider_num);

    if( p_date->i_remainder >= p_date->i_divider_num )
    {
        /* This is Bresenham algorithm. */
        p_date->date += p_date->i_divider_den;
        p_date->i_remainder -= p_date->i_divider_num;
    }

    return p_date->date;
}

#ifdef WIN32
/*
 * Number of micro-seconds between the beginning of the Windows epoch
 * (Jan. 1, 1601) and the Unix epoch (Jan. 1, 1970).
 *
 * This assumes all Win32 compilers have 64-bit support.
 */
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS) || defined(__WATCOMC__)
#   define DELTA_EPOCH_IN_USEC  11644473600000000Ui64
#else
#   define DELTA_EPOCH_IN_USEC  11644473600000000ULL
#endif

static uint64_t filetime_to_unix_epoch (const FILETIME *ft)
{
    uint64_t res = (uint64_t) ft->dwHighDateTime << 32;

    res |= ft->dwLowDateTime;
    res /= 10;                   /* from 100 nano-sec periods to usec */
    res -= DELTA_EPOCH_IN_USEC;  /* from Win epoch to Unix epoch */
    return (res);
}

static int gettimeofday (struct timeval *tv, void *tz )
{
    FILETIME  ft;
    uint64_t tim;

    if (!tv) {
        return VLC_EGENERIC;
    }
    GetSystemTimeAsFileTime (&ft);
    tim = filetime_to_unix_epoch (&ft);
    tv->tv_sec  = (long) (tim / 1000000L);
    tv->tv_usec = (long) (tim % 1000000L);
    return (0);
}
#endif



/**
 * @return NTP 64-bits timestamp in host byte order.
 */
uint64_t NTPtime64 (void)
{
    struct timespec ts;
#if defined (CLOCK_REALTIME)
    clock_gettime (CLOCK_REALTIME, &ts);
#else
    {
        struct timeval tv;
        gettimeofday (&tv, NULL);
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * 1000;
    }
#endif

    /* Convert nanoseconds to 32-bits fraction (232 picosecond units) */
    uint64_t t = (uint64_t)(ts.tv_nsec) << 32;
    t /= 1000000000;


    /* There is 70 years (incl. 17 leap ones) offset to the Unix Epoch.
     * No leap seconds during that period since they were not invented yet.
     */
    assert (t < 0x100000000);
    t |= ((70LL * 365 + 17) * 24 * 60 * 60 + ts.tv_sec) << 32;
    return t;
}

