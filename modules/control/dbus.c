/*****************************************************************************
 * dbus.c : D-Bus control interface
 *****************************************************************************
 * Copyright (C) 2006 Rafaël Carré
 * $Id$
 *
 * Authors:    Rafaël Carré <funman at videolanorg>
 *             Mirsal Ennaime <mirsal dot ennaime at gmail dot com>
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

/*
 * D-Bus Specification:
 *      http://dbus.freedesktop.org/doc/dbus-specification.html
 * D-Bus low-level C API (libdbus)
 *      http://dbus.freedesktop.org/doc/dbus/api/html/index.html
 */

/*
 * TODO:
 *  properties ?
 *
 *  macros to read incoming arguments
 *
 *  explore different possible types (arrays..)
 *
 *  what must we do if org.videolan.vlc already exist on the bus ?
 *  ( there is more than one vlc instance )
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbus.h"

#include <vlc/vlc.h>
#include <vlc_aout.h>
#include <vlc_interface.h>
#include <vlc_meta.h>
#include <vlc_input.h>
#include <vlc_playlist.h>


/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );


static int TrackChange( vlc_object_t *p_this, const char *psz_var,
                    vlc_value_t oldval, vlc_value_t newval, void *p_data );

static int GetInputMeta ( input_item_t *p_input, 
                    DBusMessageIter *args);

struct intf_sys_t
{
    DBusConnection *p_conn;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_shortname( _("dbus"));
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_description( _("D-Bus control interface") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Methods
 *****************************************************************************/
#if 0
DBUS_METHOD( PlaylistExport_XSPF )
{ /*export playlist to an xspf file */

  /* reads the filename to export to */
  /* returns the status as int32:
   *    0 : success
   *    1 : error
   *    2 : playlist empty
   */
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusError error; 
    dbus_error_init( &error );

    char *psz_file;
    dbus_int32_t i_ret;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_file,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );

    if( ( !playlist_IsEmpty( p_playlist ) ) &&
            ( p_playlist->p_root_category->i_children > 0 ) )
    {
        if( playlist_Export( p_playlist, psz_file,
                         p_playlist->p_root_category->pp_children[0],
                         "export-xspf" ) == VLC_SUCCESS )
            i_ret = 0;
        else
            i_ret = 1;
    }
    else
        i_ret = 2;

    pl_Release( ((vlc_object_t*) p_this ) );

    ADD_INT32( &i_ret );
    REPLY_SEND;
}
#endif

/* Player */

DBUS_METHOD( Quit )
{ /* exits vlc */
    REPLY_INIT;
    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    playlist_Stop( p_playlist );
    pl_Release( ((vlc_object_t*) p_this) );
    vlc_object_kill(((vlc_object_t*)p_this)->p_libvlc);
    REPLY_SEND;
}

DBUS_METHOD( PositionGet )
{ /* returns position as an int in the range [0;1000] */
    REPLY_INIT;
    OUT_ARGUMENTS;
    vlc_value_t position;
    dbus_int32_t i_pos;

    playlist_t *p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    input_thread_t *p_input = p_playlist->p_input;

    if( !p_input )
        i_pos = 0;
    else
    {
        var_Get( p_input, "position", &position );
        i_pos = position.f_float * 1000 ;
    }
    pl_Release( ((vlc_object_t*) p_this) );
    ADD_INT32( &i_pos );
    REPLY_SEND;
}

DBUS_METHOD( PositionSet )
{ /* set position from an int in the range [0;1000] */

    REPLY_INIT;
    vlc_value_t position;
    playlist_t* p_playlist = NULL;
    dbus_int32_t i_pos;

    DBusError error;
    dbus_error_init( &error );

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_INT32, &i_pos,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    input_thread_t *p_input = p_playlist->p_input;

    if( p_input )
    {
        position.f_float = ((float)i_pos) / 1000;
        var_Set( p_input, "position", position );
    }
    pl_Release( ((vlc_object_t*) p_this) );
    REPLY_SEND;
}

DBUS_METHOD( VolumeGet )
{ /* returns volume in percentage */
    REPLY_INIT;
    OUT_ARGUMENTS;
    dbus_int32_t i_dbus_vol;
    audio_volume_t i_vol;
    /* 2nd argument of aout_VolumeGet is int32 */
    aout_VolumeGet( (vlc_object_t*) p_this, &i_vol );
    i_dbus_vol = ( 100 * i_vol ) / AOUT_VOLUME_MAX;
    ADD_INT32( &i_dbus_vol );
    REPLY_SEND;
}

DBUS_METHOD( VolumeSet )
{ /* set volume in percentage */
    REPLY_INIT;

    DBusError error;
    dbus_error_init( &error );

    dbus_int32_t i_dbus_vol;
    audio_volume_t i_vol;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_INT32, &i_dbus_vol,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    i_vol = ( AOUT_VOLUME_MAX / 100 ) *i_dbus_vol;
    aout_VolumeSet( (vlc_object_t*) p_this, i_vol );

    REPLY_SEND;
}

DBUS_METHOD( Next )
{ /* next playlist item */
    REPLY_INIT;
    playlist_t *p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    playlist_Next( p_playlist );
    pl_Release( ((vlc_object_t*) p_this) );
    REPLY_SEND;
}

DBUS_METHOD( Prev )
{ /* previous playlist item */
    REPLY_INIT;
    playlist_t *p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    playlist_Prev( p_playlist );
    pl_Release( ((vlc_object_t*) p_this) );
    REPLY_SEND;
}

DBUS_METHOD( Stop )
{ /* stop playing */
    REPLY_INIT;
    playlist_t *p_playlist = pl_Yield( ((vlc_object_t*) p_this) );
    playlist_Stop( p_playlist );
    pl_Release( ((vlc_object_t*) p_this) );
    REPLY_SEND;
}

DBUS_METHOD( GetStatus )
{ /* returns an int: 0=playing 1=paused 2=stopped */
    REPLY_INIT;
    OUT_ARGUMENTS;

    dbus_int32_t i_status;
    vlc_value_t val;

    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    input_thread_t *p_input = p_playlist->p_input;

    i_status = 2;
    if( p_input )
    {
        var_Get( p_input, "state", &val );
        if( val.i_int == PAUSE_S )
            i_status = 1;
        else if( val.i_int == PLAYING_S )
            i_status = 0;
    }

    pl_Release( p_playlist );

    ADD_INT32( &i_status );
    REPLY_SEND;
}

DBUS_METHOD( Pause )
{
    REPLY_INIT;
    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    playlist_Pause( p_playlist );
    pl_Release( p_playlist );
    REPLY_SEND;
}

DBUS_METHOD( Play )
{
    REPLY_INIT;
    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    playlist_Play( p_playlist );
    pl_Release( p_playlist );
    REPLY_SEND;
}

DBUS_METHOD( GetCurrentMetadata )
{
    REPLY_INIT;
    OUT_ARGUMENTS;
    playlist_t* p_playlist = pl_Yield( (vlc_object_t*) p_this );

    GetInputMeta( p_playlist->status.p_item->p_input, &args );

    pl_Release( p_playlist );
    REPLY_SEND;
}

/* Media Player information */

DBUS_METHOD( Identity )
{
    REPLY_INIT;
    OUT_ARGUMENTS;
    char *psz_identity = malloc( strlen( PACKAGE ) + strlen( VERSION ) + 1 );
    sprintf( psz_identity, "%s %s", PACKAGE, VERSION );
    ADD_STRING( &psz_identity );
    free( psz_identity );
    REPLY_SEND;
}

/* TrackList */

DBUS_METHOD( AddTrack )
{ /* add the string to the playlist, and play it if the boolean is true */
    REPLY_INIT;

    DBusError error;
    dbus_error_init( &error );
    playlist_t* p_playlist = NULL;

    char *psz_mrl;
    dbus_bool_t b_play;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_STRING, &psz_mrl,
            DBUS_TYPE_BOOLEAN, &b_play,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    p_playlist = pl_Yield( (vlc_object_t*) p_this );
    playlist_Add( p_playlist, psz_mrl, NULL, PLAYLIST_APPEND |
            ( ( b_play == TRUE ) ? PLAYLIST_GO : 0 ) ,
            PLAYLIST_END, VLC_TRUE, VLC_FALSE );
    pl_Release( p_playlist );

    REPLY_SEND;
}

DBUS_METHOD( GetCurrentTrack )
{
    REPLY_INIT;
    OUT_ARGUMENTS;
    dbus_int32_t i_position = 0;
    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    playlist_item_t* p_tested_item = p_playlist->p_root_onelevel;

    while ( p_tested_item->p_input->i_id !=
                    p_playlist->status.p_item->p_input->i_id )
    {
        i_position++;
        TEST_NEXT;
    }

    pl_Release( p_playlist );

    ADD_INT32( &i_position );
    REPLY_SEND;
}

DBUS_METHOD( GetMetadata )
{
    REPLY_INIT;
    OUT_ARGUMENTS;
    DBusError error;
    dbus_error_init( &error );

    dbus_int32_t i_position, i_count = 0;

    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    playlist_item_t* p_tested_item = p_playlist->p_root_onelevel;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_INT32, &i_position,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    
    while ( i_count < i_position ) 
    {
        i_count++;
	    TEST_NEXT;
    }

    GetInputMeta ( p_tested_item->p_input, &args );

    pl_Release( p_playlist );
    REPLY_SEND;
}

DBUS_METHOD( GetLength )
{ 
    REPLY_INIT;
    OUT_ARGUMENTS;

    dbus_int32_t i_elements = 0;
    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    playlist_item_t* p_tested_item = p_playlist->p_root_onelevel;
    playlist_item_t* p_last_item = playlist_GetLastLeaf( p_playlist, 
                    p_playlist->p_root_onelevel ); 

    while ( p_tested_item->p_input->i_id != p_last_item->p_input->i_id )
    {
        i_elements++;
	TEST_NEXT;
    }

    pl_Release( p_playlist );
    
    ADD_INT32( &i_elements );
    REPLY_SEND;
}

DBUS_METHOD( DelTrack )
{
    REPLY_INIT;

    DBusError error;
    dbus_error_init( &error );

    dbus_int32_t i_position, i_count = 0;
    playlist_t *p_playlist = pl_Yield( (vlc_object_t*) p_this );
    playlist_item_t* p_tested_item = p_playlist->p_root_onelevel;

    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_INT32, &i_position,
            DBUS_TYPE_INVALID );

    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    
    while ( i_count < i_position ) 
    {
        i_count++;
        TEST_NEXT;
    }

    PL_LOCK; 
    playlist_DeleteFromInput( p_playlist, 
		    p_tested_item->p_input->i_id, 
		    VLC_TRUE );
    PL_UNLOCK;

    pl_Release( p_playlist );
    
    REPLY_SEND;
}

DBUS_METHOD( Loop )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusError error;
    dbus_bool_t b_loop;
    vlc_value_t val;
    playlist_t* p_playlist = NULL;
    
    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_BOOLEAN, &b_loop,
            DBUS_TYPE_INVALID );
    
    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    val.b_bool = ( b_loop == TRUE ) ? VLC_TRUE : VLC_FALSE ;
    p_playlist = pl_Yield( (vlc_object_t*) p_this );
    var_Set ( p_playlist, "loop", val );
    pl_Release( ((vlc_object_t*) p_this) );

    REPLY_SEND;
}

DBUS_METHOD( Repeat )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusError error;
    dbus_bool_t b_repeat;
    vlc_value_t val;
    playlist_t* p_playlist = NULL;
    
    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_BOOLEAN, &b_repeat,
            DBUS_TYPE_INVALID );
    
    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    val.b_bool = ( b_repeat == TRUE ) ? VLC_TRUE : VLC_FALSE ;
    
    p_playlist = pl_Yield( (vlc_object_t*) p_this );
    var_Set ( p_playlist, "repeat", val );
    pl_Release( ((vlc_object_t*) p_this) );

    REPLY_SEND;
}

DBUS_METHOD( Random )
{
    REPLY_INIT;
    OUT_ARGUMENTS;

    DBusError error;
    dbus_bool_t b_random;
    vlc_value_t val;
    playlist_t* p_playlist = NULL;
    
    dbus_error_init( &error );
    dbus_message_get_args( p_from, &error,
            DBUS_TYPE_BOOLEAN, &b_random,
            DBUS_TYPE_INVALID );
    
    if( dbus_error_is_set( &error ) )
    {
        msg_Err( (vlc_object_t*) p_this, "D-Bus message reading : %s\n",
                error.message );
        dbus_error_free( &error );
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    val.b_bool = ( b_random == TRUE ) ? VLC_TRUE : VLC_FALSE ;
    
    p_playlist = pl_Yield( (vlc_object_t*) p_this );
    var_Set ( p_playlist, "random", val );
    pl_Release( ((vlc_object_t*) p_this) );

    REPLY_SEND;
}
/*****************************************************************************
 * Introspection method
 *****************************************************************************/

DBUS_METHOD( handle_introspect_root )
{ /* handles introspection of /org/videolan/vlc */
    REPLY_INIT;
    OUT_ARGUMENTS;
    ADD_STRING( &psz_introspection_xml_data_root );
    REPLY_SEND;
}

DBUS_METHOD( handle_introspect_player )
{
    REPLY_INIT;
    OUT_ARGUMENTS;
    ADD_STRING( &psz_introspection_xml_data_player );
    REPLY_SEND;
}

DBUS_METHOD( handle_introspect_tracklist )
{
    REPLY_INIT;
    OUT_ARGUMENTS;
    ADD_STRING( &psz_introspection_xml_data_tracklist );
    REPLY_SEND;
}

/*****************************************************************************
 * handle_*: answer to incoming messages
 *****************************************************************************/

#define METHOD_FUNC( method, function ) \
    else if( dbus_message_is_method_call( p_from, VLC_DBUS_INTERFACE, method ) )\
        return function( p_conn, p_from, p_this )

DBUS_METHOD( handle_root )
{

    if( dbus_message_is_method_call( p_from,
                DBUS_INTERFACE_INTROSPECTABLE, "Introspect" ) )
        return handle_introspect_root( p_conn, p_from, p_this );

    /* here D-Bus method's names are associated to an handler */

    METHOD_FUNC( "Identity",                Identity );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


DBUS_METHOD( handle_player )
{
    if( dbus_message_is_method_call( p_from,
                DBUS_INTERFACE_INTROSPECTABLE, "Introspect" ) )
    return handle_introspect_player( p_conn, p_from, p_this );

    /* here D-Bus method's names are associated to an handler */

    METHOD_FUNC( "Prev",                    Prev );
    METHOD_FUNC( "Next",                    Next );
    METHOD_FUNC( "Quit",                    Quit );
    METHOD_FUNC( "Stop",                    Stop );
    METHOD_FUNC( "Play",                    Play );
    METHOD_FUNC( "Pause",                   Pause );
    METHOD_FUNC( "VolumeSet",               VolumeSet );
    METHOD_FUNC( "VolumeGet",               VolumeGet );
    METHOD_FUNC( "PositionSet",             PositionSet );
    METHOD_FUNC( "PositionGet",             PositionGet );
    METHOD_FUNC( "GetStatus",               GetStatus );
    METHOD_FUNC( "GetMetadata",             GetCurrentMetadata );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBUS_METHOD( handle_tracklist )
{
    if( dbus_message_is_method_call( p_from,
                DBUS_INTERFACE_INTROSPECTABLE, "Introspect" ) )
    return handle_introspect_tracklist( p_conn, p_from, p_this );

    /* here D-Bus method's names are associated to an handler */

    METHOD_FUNC( "GetMetadata",             GetMetadata );
    METHOD_FUNC( "GetCurrentTrack",         GetCurrentTrack );
    METHOD_FUNC( "GetLength",               GetLength );
    METHOD_FUNC( "AddTrack",                AddTrack );
    METHOD_FUNC( "DelTrack",                DelTrack );
    METHOD_FUNC( "Loop",                    Loop );
    METHOD_FUNC( "Repeat",                  Repeat );
    METHOD_FUNC( "Random",                  Random );

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/

static int Open( vlc_object_t *p_this )
{ /* initialisation of the connection */
    intf_thread_t   *p_intf = (intf_thread_t*)p_this;
    intf_sys_t      *p_sys  = malloc( sizeof( intf_sys_t ) );
    playlist_t      *p_playlist;
    DBusConnection  *p_conn;
    DBusError       error;

    if( !p_sys )
        return VLC_ENOMEM;

    dbus_threads_init_default();

    dbus_error_init( &error );

    /* connect to the session bus */
    p_conn = dbus_bus_get( DBUS_BUS_SESSION, &error );
    if( !p_conn )
    {
        msg_Err( p_this, "Failed to connect to the D-Bus session daemon: %s",
                error.message );
        dbus_error_free( &error );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* register a well-known name on the bus */
    dbus_bus_request_name( p_conn, "org.freedesktop.MediaPlayer", 0, &error );
    if( dbus_error_is_set( &error ) )
    {
        msg_Err( p_this, "Error requesting org.freedesktop.MediaPlayer service:"                " %s\n", error.message );
        dbus_error_free( &error );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* we register the objects */
    dbus_connection_register_object_path( p_conn, VLC_DBUS_ROOT_PATH,
            &vlc_dbus_root_vtable, p_this );
    dbus_connection_register_object_path( p_conn, VLC_DBUS_PLAYER_PATH,
            &vlc_dbus_player_vtable, p_this );
    dbus_connection_register_object_path( p_conn, VLC_DBUS_TRACKLIST_PATH,
            &vlc_dbus_tracklist_vtable, p_this );

    dbus_connection_flush( p_conn );

    p_playlist = pl_Yield( p_intf );
    PL_LOCK;
    var_AddCallback( p_playlist, "playlist-current", TrackChange, p_intf );
    PL_UNLOCK;
    pl_Release( p_playlist );

    p_intf->pf_run = Run;
    p_intf->p_sys = p_sys;
    p_sys->p_conn = p_conn;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/

static void Close   ( vlc_object_t *p_this )
{
    intf_thread_t   *p_intf     = (intf_thread_t*) p_this;
    playlist_t      *p_playlist = pl_Yield( p_intf );;

    PL_LOCK;
    var_DelCallback( p_playlist, "playlist-current", TrackChange, p_intf );
    PL_UNLOCK;
    pl_Release( p_playlist );

    dbus_connection_unref( p_intf->p_sys->p_conn );

    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/

static void Run          ( intf_thread_t *p_intf )
{
    while( !p_intf->b_die )
    {
        msleep( INTF_IDLE_SLEEP );
        dbus_connection_read_write_dispatch( p_intf->p_sys->p_conn, 0 );
    }
}

/*****************************************************************************
 * TrackChange: Playlist item change callback
 *****************************************************************************/

DBUS_SIGNAL( TrackChangeSignal )
{ /* emit the metadata of the new item */
    SIGNAL_INIT( "TrackChange" );
    OUT_ARGUMENTS;

    input_thread_t *p_input = (input_thread_t*) p_data;
    GetInputMeta ( input_GetItem(p_input), &args );

    SIGNAL_SEND;
}

static int TrackChange( vlc_object_t *p_this, const char *psz_var,
            vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t       *p_intf     = ( intf_thread_t* ) p_data;
    intf_sys_t          *p_sys      = p_intf->p_sys;
    playlist_t          *p_playlist;
    input_thread_t      *p_input    = NULL;
    (void)p_this; (void)psz_var; (void)oldval; (void)newval;

    p_playlist = pl_Yield( p_intf );
    PL_LOCK;
    p_input = p_playlist->p_input;

    if( !p_input )
    {
        PL_UNLOCK;
        pl_Release( p_playlist );
        return VLC_SUCCESS;
    }

    vlc_object_yield( p_input );
    PL_UNLOCK;
    pl_Release( p_playlist );

    TrackChangeSignal( p_sys->p_conn, p_input );

    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * GetInputMeta: Fill a DBusMessage with the given input item metadata
 *****************************************************************************/

#define ADD_META( entry, type, data ) \
    if( data ) { \
        dbus_message_iter_open_container( &dict, DBUS_TYPE_DICT_ENTRY, \
                NULL, &dict_entry ); \
        dbus_message_iter_append_basic( &dict_entry, DBUS_TYPE_STRING, \
                &ppsz_meta_items[entry] ); \
	dbus_message_iter_open_container( &dict_entry, DBUS_TYPE_VARIANT, \
                type##_AS_STRING, &variant ); \
        dbus_message_iter_append_basic( &variant, \
                type, \
                & data ); \
        dbus_message_iter_close_container( &dict_entry, &variant ); \
        dbus_message_iter_close_container( &dict, &dict_entry ); }\

#define ADD_VLC_META_STRING( entry, item ) \
        ADD_META( entry, DBUS_TYPE_STRING, \
                p_input->p_meta->psz_##item );

static int GetInputMeta( input_item_t* p_input, 
                        DBusMessageIter *args )
{ /*FIXME: Works only for already read metadata. */ 
  
    DBusMessageIter dict, dict_entry, variant;
    /* We need the track length to be expressed in seconds
     * instead of milliseconds */
    dbus_int64_t i_length = (p_input->i_duration / 1000);


    const char* ppsz_meta_items[] = 
    {
    "title", "artist", "genre", "copyright", "album", "tracknum",
    "description", "rating", "date", "setting", "url", "language",
    "nowplaying", "publisher", "encodedby", "arturl", "trackid",
    "status", "URI", "length", "video-codec", "audio-codec",
    "video-bitrate", "audio-bitrate", "audio-samplerate"
    };
    
    dbus_message_iter_open_container( args, DBUS_TYPE_ARRAY, "{sv}", &dict );

    ADD_VLC_META_STRING( 0, title );
    ADD_VLC_META_STRING( 1, artist );
    ADD_VLC_META_STRING( 2, genre );
    ADD_VLC_META_STRING( 3, copyright );
    ADD_VLC_META_STRING( 4, album );
    ADD_VLC_META_STRING( 5, tracknum );
    ADD_VLC_META_STRING( 6, description );
    ADD_VLC_META_STRING( 7, rating );
    ADD_VLC_META_STRING( 8, date );
    ADD_VLC_META_STRING( 9, setting );
    ADD_VLC_META_STRING( 10, url );
    ADD_VLC_META_STRING( 11, language );
    ADD_VLC_META_STRING( 12, nowplaying );
    ADD_VLC_META_STRING( 13, publisher );
    ADD_VLC_META_STRING( 14, encodedby );
    ADD_VLC_META_STRING( 15, arturl );
    ADD_VLC_META_STRING( 16, trackid ); 

    ADD_META( 17, DBUS_TYPE_INT32, p_input->p_meta->i_status );
    ADD_META( 18, DBUS_TYPE_STRING, p_input->psz_uri ); 
    ADD_META( 19, DBUS_TYPE_INT64, i_length ); 

    dbus_message_iter_close_container( args, &dict );
    return VLC_SUCCESS;
}

#undef ADD_META
#undef ADD_VLC_META_STRING
