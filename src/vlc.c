/*****************************************************************************
 * vlc.c: the vlc player
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Lots of other people, see the libvlc AUTHORS file
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

#include "config.h"

#include <vlc/vlc.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>


/* Explicit HACK */
extern void LocaleFree (const char *);
extern char *FromLocale (const char *);


/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#ifdef WIN32
#include <windows.h>
extern void __wgetmainargs(int *argc, wchar_t ***wargv, wchar_t ***wenviron,
                           int expand_wildcards, int *startupinfo);
static inline void Kill(void) { }
#else

# include <signal.h>
# include <time.h>
# include <pthread.h>

static void Kill (void);
static void *SigHandler (void *set);
#endif

/*****************************************************************************
 * main: parse command line, start interface and spawn threads.
 *****************************************************************************/
int main( int i_argc, char *ppsz_argv[] )
{
    int i_ret;

    setlocale (LC_ALL, "");

#ifndef __APPLE__
    /* This clutters OSX GUI error logs */
    fprintf( stderr, "VLC media player %s\n", VLC_Version() );
#endif

#ifdef HAVE_PUTENV
#   ifdef DEBUG
    /* Activate malloc checking routines to detect heap corruptions. */
    putenv( (char*)"MALLOC_CHECK_=2" );

    /* Disable the ugly Gnome crash dialog so that we properly segfault */
    putenv( (char *)"GNOME_DISABLE_CRASH_DIALOG=1" );
#   endif

    /* If the user isn't using VLC_VERBOSE, set it to 0 by default */
    if( getenv( "VLC_VERBOSE" ) == NULL )
    {
        putenv( (char *)"VLC_VERBOSE=0" );
    }
#endif

#if defined (HAVE_GETEUID) && !defined (SYS_BEOS)
    /* FIXME: rootwrap (); */
#endif

    /* Create a libvlc structure */
    i_ret = VLC_Create();
    if( i_ret < 0 )
    {
        return -i_ret;
    }

#if !defined(WIN32) && !defined(UNDER_CE)
    /* Synchronously intercepted signals. Thy request a clean shutdown,
     * and force an unclean shutdown if they are triggered again 2+ seconds
     * later. We have to handle SIGTERM cleanly because of daemon mode.
     * Note that we set the signals after the vlc_create call. */
    static const int exitsigs[] = { SIGINT, SIGHUP, SIGQUIT, SIGTERM };
    static const int dummysigs[] = { SIGALRM, SIGPIPE, SIGCHLD };

    sigset_t set;
    pthread_t sigth;

    sigemptyset (&set);
    for (unsigned i = 0; i < sizeof (exitsigs) / sizeof (exitsigs[0]); i++)
        sigaddset (&set, exitsigs[i]);
    for (unsigned i = 0; i < sizeof (dummysigs) / sizeof (dummysigs[0]); i++)
        sigaddset (&set, dummysigs[i]);

    /* Block all these signals */
    pthread_sigmask (SIG_BLOCK, &set, NULL);

    for (unsigned i = 0; i < sizeof (dummysigs) / sizeof (dummysigs[0]); i++)
        sigdelset (&set, dummysigs[i]);

    pthread_create (&sigth, NULL, SigHandler, &set);
#endif

#ifdef WIN32
    /* Replace argv[1..n] with unicode for Windows NT and above */
    if( GetVersion() < 0x80000000 )
    {
        wchar_t **wargv, **wenvp;
        int i,i_wargc;
        int si = { 0 };
        __wgetmainargs(&i_wargc, &wargv, &wenvp, 0, &si);

        for( i = 0; i < i_wargc; i++ )
        {
            int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
            if( len > 0 )
            {
                if( len > 1 ) {
                    char *utf8arg = (char *)malloc(len);
                    if( NULL != utf8arg )
                    {
                        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, utf8arg, len, NULL, NULL);
                        ppsz_argv[i] = utf8arg;
                    }
                    else
                    {
                        /* failed!, quit */
                        return 1;
                    }
                }
                else
                {
                    ppsz_argv[i] = strdup ("");
                }
            }
            else
            {
                /* failed!, quit */
                return 1;
            }
        }
    }
    else
#endif
    {
        for (int i = 0; i < i_argc; i++)
            if ((ppsz_argv[i] = FromLocale (ppsz_argv[i])) == NULL)
                return 1; // BOOM!
    }

    /* Initialize libvlc */
    i_ret = VLC_Init( 0, i_argc, ppsz_argv );
    if( i_ret < 0 )
    {
        VLC_Destroy( 0 );
        return i_ret == VLC_EEXITSUCCESS ? 0 : -i_ret;
    }

    i_ret = VLC_AddIntf( 0, NULL, VLC_TRUE, VLC_TRUE );

    /* Finish the threads */
    VLC_CleanUp( 0 );

    Kill ();

    /* Destroy the libvlc structure */
    VLC_Destroy( 0 );

    for (int i = 0; i < i_argc; i++)
        LocaleFree (ppsz_argv[i]);

#if !defined(WIN32) && !defined(UNDER_CE)
    pthread_cancel (sigth);
# ifdef __APPLE__
    /* In Mac OS X up to 10.4.8 sigwait (among others) is not a pthread
     * cancellation point, so we throw a dummy quit signal to end
     * sigwait() in the sigth thread */
    pthread_kill (sigth, SIGQUIT);
# endif
    pthread_join (sigth, NULL);
#endif

    return -i_ret;
}

#if !defined(WIN32) && !defined(UNDER_CE)
/*****************************************************************************
 * SigHandler: system signal handler
 *****************************************************************************
 * This thread receives all handled signals synchronously.
 * It tries to end the program in a clean way.
 *****************************************************************************/
static void *SigHandler (void *data)
{
    const sigset_t *set = (sigset_t *)data;
    time_t abort_time = 0;

    for (;;)
    {
        int i_signal, state;
        (void)sigwait (set, &i_signal);

#ifdef __APPLE__
        /* In Mac OS X up to 10.4.8 sigwait (among others) is not a pthread
         * cancellation point */
        pthread_testcancel();
#endif

        /* Once a signal has been trapped, the termination sequence will be
         * armed and subsequent signals will be ignored to avoid sending
         * signals to a libvlc structure having been destroyed */

        pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state);
        if (abort_time == 0)
        {
            time (&abort_time);
            abort_time += 2;

            fprintf (stderr, "signal %d received, terminating vlc - do it "
                            "again in case it gets stuck\n", i_signal);

            /* Acknowledge the signal received */
            Kill ();
        }
        else
        if (time (NULL) <= abort_time)
        {
            /* If user asks again more than 2 seconds later, die badly */
            pthread_sigmask (SIG_UNBLOCK, set, NULL);
            fprintf (stderr, "user insisted too much, dying badly\n");
            abort ();
        }
        pthread_setcancelstate (state, NULL);
    }
    /* Never reached */
}


static void KillOnce (void)
{
    VLC_Die (0);
}

static void Kill (void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once (&once, KillOnce);
}

#endif

#if defined(UNDER_CE)
#   if defined( _MSC_VER ) && defined( UNDER_CE )
#       include "vlc_common.h"
#   endif
/*****************************************************************************
 * WinMain: parse command line, start interface and spawn threads. (WinCE only)
 *****************************************************************************/
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPTSTR lpCmdLine, int nCmdShow )
{
    char **argv, psz_cmdline[MAX_PATH];
    int argc, i_ret;

    WideCharToMultiByte( CP_ACP, 0, lpCmdLine, -1,
                         psz_cmdline, MAX_PATH, NULL, NULL );

    argv = vlc_parse_cmdline( psz_cmdline, &argc );
    argv = realloc( argv, (argc + 1) * sizeof(char *) );
    if( !argv ) return -1;

    if( argc ) memmove( argv + 1, argv, argc * sizeof(char *) );
    argv[0] = ""; /* Fake program path */

    i_ret = main( argc + 1, argv );

    /* No need to free the argv memory */
    return i_ret;
}
#endif
