/*****************************************************************************
 * intf_gnome.c: Gnome interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_gnome.c,v 1.1 2001/05/23 23:08:20 stef Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          St�phane Borel <stef@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#define MODULE_NAME gnome
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <gnome.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "modules.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_msg.h"
#include "interface.h"
#include "intf_playlist.h"

#include "video.h"
#include "video_output.h"

#include "gnome_callbacks.h"
#include "gnome_interface.h"
#include "gnome_support.h"
#include "gtk_display.h"
#include "intf_gtk.h"

#include "main.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Probe      ( probedata_t *p_data );
static int  intf_Open       ( intf_thread_t *p_intf );
static void intf_Close      ( intf_thread_t *p_intf );
static void intf_Run        ( intf_thread_t *p_intf );

static gint GnomeManage     ( gpointer p_data );

/*****************************************************************************
 * g_atexit: kludge to avoid the Gnome thread to segfault at exit
 *****************************************************************************
 * gtk_init() makes several calls to g_atexit() which calls atexit() to
 * register tidying callbacks to be called at program exit. Since the Gnome
 * plugin is likely to be unloaded at program exit, we have to export this
 * symbol to intercept the g_atexit() calls. Talk about crude hack.
 *****************************************************************************/
void g_atexit( GVoidFunc func )
{
    intf_thread_t *p_intf = p_main->p_intf;

    if( p_intf->p_sys->pf_gdk_callback == NULL )
    {
        p_intf->p_sys->pf_gdk_callback = func;
    }
    else if( p_intf->p_sys->pf_gtk_callback == NULL )
    {
        p_intf->p_sys->pf_gtk_callback = func;
    }
    /* else nothing, but we could do something here */
    return;
}

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( intf_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = intf_Probe;
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Probe: probe the interface and return a score
 *****************************************************************************
 * This function tries to initialize Gnome and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int intf_Probe( probedata_t *p_data )
{
    if( TestMethod( INTF_METHOD_VAR, "gnome" ) )
    {
        return( 999 );
    }

    if( TestProgram( "gnome-vlc" ) )
    {
        return( 200 );
    }

    return( 100 );
}

/*****************************************************************************
 * intf_Open: initialize and create window
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM));
        return( 1 );
    }

    /* Initialize Gnome thread */
    p_intf->p_sys->b_popup_changed = 0;
    p_intf->p_sys->b_window_changed = 0;
    p_intf->p_sys->b_playlist_changed = 0;

    p_intf->p_sys->i_playing = -1;
    p_intf->p_sys->b_slider_free = 1;

    p_intf->p_sys->pf_gtk_callback = NULL;
    p_intf->p_sys->pf_gdk_callback = NULL;

    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface window
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: Gnome thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * gtk_main() from within it without annoying the rest of the program.
 * XXX: the approach may look kludgy, and probably is, but I could not find
 * a better way to dynamically load a Gnome interface at runtime.
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    /* gnome_init needs to know the command line. We don't care, so we
     * give it an empty one */
    char *p_args[] = { "" };
    int   i_args   = 1;

    /* The data types we are allowed to receive */
    static GtkTargetEntry target_table[] =
    {
        { "text/uri-list", 0, DROP_ACCEPT_TEXT_URI_LIST },
        { "text/plain",    0, DROP_ACCEPT_TEXT_PLAIN }
    };

    /* intf_Manage callback timeout */
    int i_timeout;

    /* Initialize Gnome */
    gnome_init( p_main->psz_arg0, VERSION, i_args, p_args );

    /* Create some useful widgets that will certainly be used */
    p_intf->p_sys->p_window = create_intf_window( );
    p_intf->p_sys->p_popup = create_intf_popup( );
    p_intf->p_sys->p_playlist = create_intf_playlist();

    /* Set the title of the main window */
    gtk_window_set_title( GTK_WINDOW(p_intf->p_sys->p_window),
                          VOUT_TITLE " (Gnome interface)");

    /* Accept file drops on the main window */
    gtk_drag_dest_set( GTK_WIDGET( p_intf->p_sys->p_window ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       1, GDK_ACTION_COPY );
    /* Accept file drops on the playlist window */
    gtk_drag_dest_set( GTK_WIDGET( gtk_object_get_data( GTK_OBJECT(
                            p_intf->p_sys->p_playlist ), "playlist_clist") ),
                       GTK_DEST_DEFAULT_ALL, target_table,
                       1, GDK_ACTION_COPY );

    /* Get the interface labels */
    p_intf->p_sys->p_slider_frame = gtk_object_get_data(
                      GTK_OBJECT( p_intf->p_sys->p_window ), "slider_frame" );
    #define P_LABEL( name ) GTK_LABEL( gtk_object_get_data( \
                         GTK_OBJECT( p_intf->p_sys->p_window ), name ) )
    p_intf->p_sys->p_label_title = P_LABEL( "title_label" );
    p_intf->p_sys->p_label_chapter = P_LABEL( "chapter_label" );
    #undef P_LABEL

    /* Connect the date display to the slider */
    #define P_SLIDER GTK_RANGE( gtk_object_get_data( \
                         GTK_OBJECT( p_intf->p_sys->p_window ), "slider" ) )
    p_intf->p_sys->p_adj = gtk_range_get_adjustment( P_SLIDER );

    gtk_signal_connect ( GTK_OBJECT( p_intf->p_sys->p_adj ), "value_changed",
                         GTK_SIGNAL_FUNC( GtkDisplayDate ), NULL );
    p_intf->p_sys->f_adj_oldvalue = 0;
    #undef P_SLIDER

    /* We don't create these ones yet because we perhaps won't need them */
    p_intf->p_sys->p_about = NULL;
    p_intf->p_sys->p_modules = NULL;
    p_intf->p_sys->p_fileopen = NULL;
    p_intf->p_sys->p_disc = NULL;
    p_intf->p_sys->p_network = NULL;
    p_intf->p_sys->p_preferences = NULL;
    p_intf->p_sys->p_jump = NULL;

    /* Store p_intf to keep an eye on it */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_popup),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_playlist ),
                         "p_intf", p_intf );

    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_adj),
                         "p_intf", p_intf );

    /* Show the control window */
    gtk_widget_show( p_intf->p_sys->p_window );

    /* Sleep to avoid using all CPU - since some interfaces needs to access
     * keyboard events, a 100ms delay is a good compromise */
    i_timeout = gtk_timeout_add( INTF_IDLE_SLEEP / 1000, GnomeManage, p_intf );

    /* Enter gnome mode */
    gtk_main();

    /* Remove the timeout */
    gtk_timeout_remove( i_timeout );

    /* Get rid of stored callbacks so we can unload the plugin */
    if( p_intf->p_sys->pf_gtk_callback != NULL )
    {
        p_intf->p_sys->pf_gtk_callback( );
        p_intf->p_sys->pf_gtk_callback = NULL;

    }

    if( p_intf->p_sys->pf_gdk_callback != NULL )
    {
        p_intf->p_sys->pf_gdk_callback( );
        p_intf->p_sys->pf_gdk_callback = NULL;
    }
}

/* following functions are local */

/*****************************************************************************
 * GnomeManage: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/
static gint GnomeManage( gpointer p_data )
{
#define p_intf ((intf_thread_t *)p_data)

    vlc_mutex_lock( &p_intf->change_lock );

    /* If the "display popup" flag has changed */
    if( p_intf->b_menu_change )
    {
        if( !GTK_IS_WIDGET( p_intf->p_sys->p_popup ) )
        {
            p_intf->p_sys->p_popup = create_intf_popup();
            gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_popup ),
                                 "p_popup", p_intf );
        }

        gnome_popup_menu_do_popup( p_intf->p_sys->p_popup,
                                   NULL, NULL, NULL, NULL );
        p_intf->b_menu_change = 0;
    }

    /* update the playlist */
    GtkPlayListManage( p_intf ); 

    if( p_intf->p_input != NULL && !p_intf->b_die )
    {
        vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );

        /* New input or stream map change */
        if( p_intf->p_input->stream.b_changed )
        {
            GtkModeManage( p_intf );
        }

        /* Manage the slider */
        if( p_intf->p_input->stream.b_seekable )
        {
            float           newvalue;
            newvalue = p_intf->p_sys->p_adj->value;
    
#define p_area p_intf->p_input->stream.p_selected_area
            /* If the user hasn't touched the slider since the last time,
             * then the input can safely change it */
            if( newvalue == p_intf->p_sys->f_adj_oldvalue )
            {
                /* Update the value */
                p_intf->p_sys->p_adj->value = p_intf->p_sys->f_adj_oldvalue =
                    ( 100. * p_area->i_tell ) / p_area->i_size;
    
                gtk_signal_emit_by_name( GTK_OBJECT( p_intf->p_sys->p_adj ),
                                         "value_changed" );
            }
            /* Otherwise, send message to the input if the user has
             * finished dragging the slider */
            else if( p_intf->p_sys->b_slider_free )
            {
                off_t i_seek = ( newvalue * p_area->i_size ) / 100;
    
                vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );
                input_Seek( p_intf->p_input, i_seek );
                vlc_mutex_lock( &p_intf->p_input->stream.stream_lock );
    
                /* Update the old value */
                p_intf->p_sys->f_adj_oldvalue = newvalue;
            }
#undef p_area
        }
        vlc_mutex_unlock( &p_intf->p_input->stream.stream_lock );

        if( p_intf->p_sys->i_part !=
            p_intf->p_input->stream.p_selected_area->i_part )
        {
            p_intf->p_sys->b_chapter_update = 1;
            GtkSetupMenus( p_intf );
        }

    }
    else if( !p_intf->b_die )
    {
        GtkModeManage( p_intf );
    }

    /* Manage core vlc functions through the callback */
    p_intf->pf_manage( p_intf );

    if( p_intf->b_die )
    {
        vlc_mutex_unlock( &p_intf->change_lock );

        /* Prepare to die, young Skywalker */
        gtk_main_quit();

        /* Just in case */
        return( FALSE );
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    return( TRUE );

#undef p_intf
}
