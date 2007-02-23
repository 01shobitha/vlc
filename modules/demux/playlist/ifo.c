/*****************************************************************************
 * ifo.c: Dummy ifo demux to enable opening DVDs rips by double cliking on VIDEO_TS.IFO
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea @t videolan d.t org>
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
#include <vlc_demux.h>

#include <errno.h>                                                 /* ENOMEM */
#include "playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Import_IFO: main import function
 *****************************************************************************/
int E_(Import_IFO)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    char *psz_file = p_demux->psz_path + strlen( p_demux->psz_path )
                     - strlen( "VIDEO_TS.IFO" );
    /* Valid filenamed are :
     *  - VIDEO_TS.IFO
     *  - VTS_XX_X.IFO where X are digits
     */
    if( strlen( p_demux->psz_path ) > strlen( "VIDEO_TS.IFO" )
        && ( !strcasecmp( psz_file, "VIDEO_TS.IFO" )
        || (!strncasecmp( psz_file, "VTS_", 4 )
        && !strcasecmp( psz_file + strlen( "VTS_00_0" ) , ".IFO" ) ) ) )
    {
        int i_peek;
        byte_t *p_peek;
        i_peek = stream_Peek( p_demux->s, &p_peek, 8 );

        if( strncmp( p_peek, "DVDVIDEO", 8 ) )
            return VLC_EGENERIC;
    }
    else
        return VLC_EGENERIC;

//    STANDARD_DEMUX_INIT_MSG( "found valid VIDEO_TS.IFO" )
    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_IFO)( vlc_object_t *p_this )
{
}

static int Demux( demux_t *p_demux )
{
    char *psz_url = NULL;
    char *psz_file = p_demux->psz_path + strlen( p_demux->psz_path )
                     - strlen( "VIDEO_TS.IFO" );
    size_t len = 0;
    input_item_t *p_input;

    INIT_PLAYLIST_STUFF;

    len = strlen( "dvd://" ) + strlen( p_demux->psz_path )
          - strlen( "VIDEO_TS.IFO" );
    psz_url = (char *)malloc( len+1 );
    snprintf( psz_url, len+1, "dvd://%s", p_demux->psz_path );

    p_input = input_ItemNewExt( p_playlist, psz_url, psz_url, 0, NULL, -1 );
    playlist_BothAddInput( p_playlist, p_input,
                           p_item_in_category,
                           PLAYLIST_APPEND | PLAYLIST_SPREPARSE,
                           PLAYLIST_END, NULL, NULL, VLC_FALSE );

    HANDLE_PLAY_AND_RELEASE;

    return -1; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
