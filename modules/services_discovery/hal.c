/*****************************************************************************
 * hal.c :  HAL interface module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * Copyright (C) 2006 Rafaël Carré
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Rafaël Carré <funman at videolanorg>
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

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <vlc/input.h>

#include "network.h"

#include <errno.h>                                                 /* ENOMEM */

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#include <hal/libhal.h>

#define MAX_LINE_LENGTH 256

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_HAL_1
/* store relation between item id and udi for ejection */
struct udi_input_id_t
{
    char    *psz_udi;
    int     i_id;
};
#endif

struct services_discovery_sys_t
{
    LibHalContext *p_ctx;
    playlist_item_t *p_node_cat;
    playlist_item_t *p_node_one;
#ifdef HAVE_HAL_1
    int                     i_devices_number;
    struct udi_input_id_t   **pp_devices;
#endif
};
static void Run    ( services_discovery_t *p_intf );

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#ifdef HAVE_HAL_1
/* HAL callbacks */
void DeviceAdded( LibHalContext *p_ctx, const char *psz_udi );
void DeviceRemoved( LibHalContext *p_ctx, const char *psz_udi );
/* to retrieve p_sd in HAL callbacks */
services_discovery_t        *p_sd_global;
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("HAL devices detection") );
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_SD );

    set_capability( "services_discovery", 0 );
    set_callbacks( Open, Close );

vlc_module_end();


/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = malloc(
                                    sizeof( services_discovery_sys_t ) );

    playlist_t          *p_playlist;

#ifdef HAVE_HAL_1
    DBusError           dbus_error;
    DBusConnection      *p_connection;

    p_sd_global = p_sd;
    p_sys->i_devices_number = 0;
    p_sys->pp_devices = NULL;
#endif

    p_sd->pf_run = Run;
    p_sd->p_sys  = p_sys;

#ifdef HAVE_HAL_1
    dbus_error_init( &dbus_error );

    p_sys->p_ctx = libhal_ctx_new();
    if( !p_sys->p_ctx )
    {
        msg_Err( p_sd, "unable to create HAL context") ;
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_connection = dbus_bus_get( DBUS_BUS_SYSTEM, &dbus_error );
    if( dbus_error_is_set( &dbus_error ) )
    {
        msg_Err( p_sd, "unable to connect to DBUS: %s", dbus_error.message );
        dbus_error_free( &dbus_error );
        free( p_sys );
        return VLC_EGENERIC;
    }
    libhal_ctx_set_dbus_connection( p_sys->p_ctx, p_connection );
    if( !libhal_ctx_init( p_sys->p_ctx, &dbus_error ) )
#else
    if( !(p_sys->p_ctx = hal_initialize( NULL, FALSE ) ) )
#endif
    {
#ifdef HAVE_HAL_1
        msg_Err( p_sd, "hal not available : %s", dbus_error.message );
        dbus_error_free( &dbus_error );
#else
        msg_Err( p_sd, "hal not available" );
#endif
        free( p_sys );
        return VLC_EGENERIC;
    }

#ifdef HAVE_HAL_1
        if( !libhal_ctx_set_device_added( p_sys->p_ctx, DeviceAdded ) ||
                !libhal_ctx_set_device_removed( p_sys->p_ctx, DeviceRemoved ) )
        {
            msg_Err( p_sd, "unable to add callback" );
            dbus_error_free( &dbus_error );
            free( p_sys );
            return VLC_EGENERIC;
        }
#endif

    /* Create our playlist node */
    p_playlist = (playlist_t *)vlc_object_find( p_sd, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Warn( p_sd, "unable to find playlist, cancelling HAL listening");
        return VLC_EGENERIC;
    }

    playlist_NodesPairCreate( p_playlist, _("Devices"),
                              &p_sys->p_node_cat, &p_sys->p_node_one,
                              VLC_TRUE );
    vlc_object_release( p_playlist );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = p_sd->p_sys;
    playlist_t *p_playlist =  (playlist_t *) vlc_object_find( p_sd,
                                 VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist )
    {
        playlist_NodeDelete( p_playlist, p_sys->p_node_cat, VLC_TRUE,VLC_TRUE );
        playlist_NodeDelete( p_playlist, p_sys->p_node_one, VLC_TRUE,VLC_TRUE );
        vlc_object_release( p_playlist );
    }
    free( p_sys );
#ifdef HAVE_HAL_1
    struct udi_input_id_t *p_udi_entry;

    while( p_sys->i_devices_number > 0 )
    {
        p_udi_entry = p_sys->pp_devices[0];
        if( p_udi_entry->psz_udi ) free( p_udi_entry->psz_udi );
        TAB_REMOVE( p_sys->i_devices_number, p_sys->pp_devices,
                p_sys->pp_devices[0] );
        if( p_udi_entry ) free( p_udi_entry );
    }
    p_sys->pp_devices = NULL;
#endif
}

static void AddItem( services_discovery_t *p_sd, input_item_t * p_input
#ifdef HAVE_HAL_1
                ,char* psz_device
#endif
                    )
{
    playlist_item_t *p_item;
    services_discovery_sys_t *p_sys  = p_sd->p_sys;
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_sd,
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_sd, "playlist not found" );
        return;
    }
    p_item = playlist_NodeAddInput( p_playlist, p_input,p_sd->p_sys->p_node_cat,
                                    PLAYLIST_APPEND, PLAYLIST_END );
    p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;
    p_item = playlist_NodeAddInput( p_playlist, p_input,p_sd->p_sys->p_node_one,
                                    PLAYLIST_APPEND, PLAYLIST_END );
    p_item->i_flags &= ~PLAYLIST_SKIP_FLAG;

    vlc_object_release( p_playlist );

#ifdef HAVE_HAL_1
    struct udi_input_id_t *p_udi_entry;
    p_udi_entry = malloc( sizeof( struct udi_input_id_t ) );
    if( !p_udi_entry )
    {
        return;
    }
    p_udi_entry->i_id = p_item->i_id;
    p_udi_entry->psz_udi = strdup( psz_device );
    TAB_APPEND( p_sys->i_devices_number, p_sys->pp_devices, p_udi_entry );
#endif
}

static void AddDvd( services_discovery_t *p_sd, char *psz_device )
{
    char *psz_name;
    char *psz_uri;
    char *psz_blockdevice;
    input_item_t        *p_input;
#ifdef HAVE_HAL_1
    psz_name = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
                                        psz_device, "volume.label", NULL );
    psz_blockdevice = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
                                        psz_device, "block.device", NULL );
#else
    psz_name = hal_device_get_property_string( p_sd->p_sys->p_ctx,
                                               psz_device, "volume.label" );
    psz_blockdevice = hal_device_get_property_string( p_sd->p_sys->p_ctx,
                                                 psz_device, "block.device" );
#endif
    asprintf( &psz_uri, "dvd://%s", psz_blockdevice );
    /* Create the playlist item here */
    p_input = input_ItemNew( p_sd, psz_uri, psz_name );
    free( psz_uri );
    if( !p_input )
    {
        return;
    }
#ifdef HAVE_HAL_1
    AddItem( p_sd, p_input, psz_device );
#else
    AddItem( p_sd, p_input );
#endif
}

#ifdef HAVE_HAL_1
static void DelItem( services_discovery_t *p_sd, char* psz_udi )
{
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;
    int                         i;

    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_sd,
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_sd, "playlist not found" );
        return;
    }

    for( i = 0; i < p_sys->i_devices_number; i++ )
    {
        if( strcmp( psz_udi, p_sys->pp_devices[i]->psz_udi ) == 0 )
        {
            playlist_DeleteFromItemId( p_playlist, p_sys->pp_devices[i]->i_id );
            TAB_REMOVE( p_sys->i_devices_number, p_sys->pp_devices,
                    p_sys->pp_devices[i] );
        }
    }

    vlc_object_release( p_playlist );
}
#endif

static void AddCdda( services_discovery_t *p_sd, char *psz_device )
{
    char *psz_uri;
    char *psz_blockdevice;
    input_item_t     *p_input;
#ifdef HAVE_HAL_1
    psz_blockdevice = libhal_device_get_property_string( p_sd->p_sys->p_ctx,
                                            psz_device, "block.device", NULL );
#else
    psz_blockdevice = hal_device_get_property_string( p_sd->p_sys->p_ctx,
                                                 psz_device, "block.device" );
#endif
    asprintf( &psz_uri, "cdda://%s", psz_blockdevice );
    /* Create the playlist item here */
    p_input = input_ItemNew( p_sd, psz_uri, "Audio CD" );
    free( psz_uri );
    if( !p_input )
        return;
#ifdef HAVE_HAL_1
    AddItem( p_sd, p_input, psz_device );
#else
    AddItem( p_sd, p_input );
#endif
}

static void ParseDevice( services_discovery_t *p_sd, char *psz_device )
{
    char *psz_disc_type;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;
#ifdef HAVE_HAL_1
    if( libhal_device_property_exists( p_sys->p_ctx, psz_device,
                                       "volume.disc.type", NULL ) )
    {
        psz_disc_type = libhal_device_get_property_string( p_sys->p_ctx,
                                                        psz_device,
                                                        "volume.disc.type",
                                                        NULL );
#else
    if( hal_device_property_exists( p_sys->p_ctx, psz_device,
                                    "volume.disc.type" ) )
    {
        psz_disc_type = hal_device_get_property_string( p_sys->p_ctx,
                                                        psz_device,
                                                        "volume.disc.type" );
#endif
        if( !strcmp( psz_disc_type, "dvd_rom" ) )
        {
#ifdef HAVE_HAL_1
            /* hal 0.2.9.7 (HAVE_HAL) has not is_videodvd
             * but hal 0.5.0 (HAVE_HAL_1) has */
            if (libhal_device_get_property_bool( p_sys->p_ctx, psz_device,
                                         "volume.disc.is_videodvd", NULL ) )
#endif
            AddDvd( p_sd, psz_device );
        }
        else if( !strcmp( psz_disc_type, "cd_rom" ) )
        {
#ifdef HAVE_HAL_1
            if( libhal_device_get_property_bool( p_sys->p_ctx, psz_device,
                                         "volume.disc.has_audio" , NULL ) )
#else
            if( hal_device_get_property_bool( p_sys->p_ctx, psz_device,
                                         "volume.disc.has_audio" ) )
#endif
            {
                AddCdda( p_sd, psz_device );
            }
        }
    }
}

/*****************************************************************************
 * Run: main HAL thread
 *****************************************************************************/
static void Run( services_discovery_t *p_sd )
{
    int i, i_devices;
    char **devices;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;

    /* parse existing devices first */
#ifdef HAVE_HAL_1
    if( ( devices = libhal_get_all_devices( p_sys->p_ctx, &i_devices, NULL ) ) )
#else
    if( ( devices = hal_get_all_devices( p_sys->p_ctx, &i_devices ) ) )
#endif
    {
        for( i = 0; i < i_devices; i++ )
        {
            ParseDevice( p_sd, devices[ i ] );
#ifdef HAVE_HAL_1
            libhal_free_string( devices[ i ] );
#else
            hal_free_string( devices[ i ] );
#endif

        }
    }
#ifdef HAVE_HAL_1
    while( !p_sd->b_die )
    {
    /* look for events on the bus, blocking 1 second */
    dbus_connection_read_write_dispatch(
            libhal_ctx_get_dbus_connection(p_sys->p_ctx), 1000 );
    }
#endif

}

#ifdef HAVE_HAL_1
void DeviceAdded( LibHalContext *p_ctx, const char *psz_udi )
{
        ParseDevice( p_sd_global, (char*) psz_udi );
}
void DeviceRemoved( LibHalContext *p_ctx, const char *psz_udi )
{
        DelItem( p_sd_global, (char*) psz_udi );
}
#endif

