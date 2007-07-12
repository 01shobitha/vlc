/*****************************************************************************
 * control.h: private header for mediacontrol
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: vlc.h 10101 2005-03-02 16:47:31Z robux4 $
 *
 * Authors: Olivier Aubert <olivier.aubert@liris.univ-lyon1.fr>
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

#ifndef _VLC_MEDIACONTROL_INTERNAL_H
#define _VLC_MEDIACONTROL_INTERNAL_H 1

# ifdef __cplusplus
extern "C" {
# endif

#include <vlc/vlc.h>
#include <vlc/mediacontrol_structures.h>
#include "libvlc_internal.h"
#include <vlc/libvlc.h>

struct mediacontrol_Instance {
    struct libvlc_instance_t * p_instance;
    playlist_t    *p_playlist;
};

vlc_int64_t mediacontrol_unit_convert( input_thread_t *p_input,
                       mediacontrol_PositionKey from,
                       mediacontrol_PositionKey to,
                       vlc_int64_t value );
vlc_int64_t mediacontrol_position2microsecond(
    input_thread_t *p_input,
    const mediacontrol_Position *pos );

/**
 * Allocate a RGBPicture structure.
 * \param datasize: the size of the data
 */
mediacontrol_RGBPicture *private_mediacontrol_RGBPicture__alloc( int datasize );

mediacontrol_RGBPicture *private_mediacontrol_createRGBPicture( int, int, long, vlc_int64_t l_date,
                                  char *, int);

mediacontrol_PlaylistSeq *private_mediacontrol_PlaylistSeq__alloc( int size );


#define RAISE( c, m )  if( exception ) { exception->code = c;	\
                                         exception->message = strdup(m); }

#define RAISE_NULL( c, m ) { RAISE( c, m ); return NULL; }
#define RAISE_VOID( c, m ) { RAISE( c, m ); return; }

#define HANDLE_LIBVLC_EXCEPTION_VOID( e )  if( libvlc_exception_raised( e ) ) {	\
	RAISE( mediacontrol_InternalException, libvlc_exception_get_message( e )); \
        libvlc_exception_clear( e ); \
        return; }

#define HANDLE_LIBVLC_EXCEPTION_NULL( e )  if( libvlc_exception_raised( e ) ) { 	\
        RAISE( mediacontrol_InternalException, libvlc_exception_get_message( e )); \
        libvlc_exception_clear( e ); \
        return NULL; }

#define HANDLE_LIBVLC_EXCEPTION_ZERO( e )  if( libvlc_exception_raised( e ) ) { \
        RAISE( mediacontrol_InternalException, libvlc_exception_get_message( e )); \
        libvlc_exception_clear( e ); \
        return 0; }


# ifdef __cplusplus
}
# endif

#endif
