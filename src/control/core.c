/*****************************************************************************
 * core.c: Core libvlc new API functions : initialization, exceptions handling
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <stdarg.h>
#include "libvlc_internal.h"
#include <vlc/libvlc.h>

#include <vlc_interface.h>

/*************************************************************************
 * Exceptions handling
 *************************************************************************/
inline void libvlc_exception_init( libvlc_exception_t *p_exception )
{
    p_exception->b_raised = 0;
    p_exception->psz_message = NULL;
}

void libvlc_exception_clear( libvlc_exception_t *p_exception )
{
    if( p_exception->psz_message )
        free( p_exception->psz_message );
    p_exception->psz_message = NULL;
    p_exception->b_raised = 0;
}

inline int libvlc_exception_raised( libvlc_exception_t *p_exception )
{
    return (NULL != p_exception) && p_exception->b_raised;
}

inline char* libvlc_exception_get_message( libvlc_exception_t *p_exception )
{
    if( p_exception->b_raised == 1 && p_exception->psz_message )
    {
        return p_exception->psz_message;
    }
    return NULL;
}

inline void libvlc_exception_raise( libvlc_exception_t *p_exception,
                                    const char *psz_format, ... )
{
    va_list args;

    /* does caller care about exceptions ? */
    if( p_exception == NULL ) return;

    /* remove previous exception if it wasn't cleared */
    if( p_exception->b_raised && p_exception->psz_message )
    {
        free(p_exception->psz_message);
        p_exception->psz_message = NULL;
    }

    va_start( args, psz_format );
    vasprintf( &p_exception->psz_message, psz_format, args );
    va_end( args );

    p_exception->b_raised = 1;
}

libvlc_instance_t * libvlc_new( int argc, char **argv,
                                libvlc_exception_t *p_e )
{
    libvlc_instance_t *p_new;

    libvlc_int_t *p_libvlc_int = libvlc_InternalCreate();
    if( !p_libvlc_int ) RAISENULL( "VLC initialization failed" );

    p_new = (libvlc_instance_t *)malloc( sizeof( libvlc_instance_t ) );
    if( !p_new ) RAISENULL( "Out of memory" );

    /** \todo Look for interface settings. If we don't have any, add -I dummy */
    /* Because we probably don't want a GUI by default */    

    if( libvlc_InternalInit( p_libvlc_int, argc, argv ) )
        RAISENULL( "VLC initialization failed" );

    p_new->p_libvlc_int = p_libvlc_int;
    p_new->p_vlm = NULL;
    p_new->b_playlist_locked = 0;
    p_new->p_callback_list = NULL;
    vlc_mutex_init(p_libvlc_int, &p_new->instance_lock);

    return p_new;
}

void libvlc_destroy( libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
    struct libvlc_callback_entry_list_t *p_listitem = p_instance->p_callback_list;

    while( p_listitem )
    {
        struct libvlc_callback_entry_list_t *p_nextlistitem = p_listitem->next;
        free( p_listitem );
        p_listitem = p_nextlistitem;
    }
    
    libvlc_InternalCleanup( p_instance->p_libvlc_int );
    libvlc_InternalDestroy( p_instance->p_libvlc_int, VLC_FALSE );
}

int libvlc_get_vlc_id( libvlc_instance_t *p_instance )
{
    return p_instance->p_libvlc_int->i_object_id;
}
