/*****************************************************************************
 * id3tag.c: id3 tag parser/skipper based on libid3tag
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: id3tag.c,v 1.12 2003/10/20 01:07:28 hartman Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/input.h>

#include <sys/types.h>

#include <id3tag.h>
#include "id3genres.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  ParseID3Tags ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
set_description( _("id3 tag parser using libid3tag" ) );
set_capability( "id3", 70 );
set_callbacks( ParseID3Tags, NULL );
vlc_module_end();

/*****************************************************************************
 * Definitions of structures  and functions used by this plugins 
 *****************************************************************************/

/*****************************************************************************
 * ParseID3Tag : parse an id3tag into the info structures
 *****************************************************************************/
static void ParseID3Tag( input_thread_t *p_input, u8 *p_data, int i_size )
{
    playlist_t * p_playlist;
    struct id3_tag * p_id3_tag;
    struct id3_frame * p_frame;
    input_info_category_t * p_category;
    int i_strings;
    char * psz_temp;
    int i;
    vlc_value_t val;
    
    var_Get( p_input, "demuxed-id3", &val );

    if( val.b_bool )
    {
        msg_Dbg( p_input, "The ID3 tag was already parsed" );
        return;
    }
    
    p_id3_tag = id3_tag_parse( p_data, i_size );
    p_category = input_InfoCategory( p_input, "ID3" );
    i = 0;
    
    while ( ( p_frame = id3_tag_findframe( p_id3_tag , "T", i ) ) )
    {
        i_strings = id3_field_getnstrings( &p_frame->fields[1] );
        while ( i_strings > 0 )
        {
            psz_temp = id3_ucs4_utf8duplicate( id3_field_getstrings( &p_frame->fields[1], --i_strings ) );
            if ( !strcmp(p_frame->id, ID3_FRAME_GENRE ) )
            {
                int i_genre;
                char *psz_endptr;
                i_genre = strtol( psz_temp, &psz_endptr, 10 );
                if( psz_temp != psz_endptr && i_genre >= 0 && i_genre < NUM_GENRES )
                {
                    input_AddInfo( p_category, (char *)p_frame->description, ppsz_genres[atoi(psz_temp)]);
                }
                else
                {
                    input_AddInfo( p_category, (char *)p_frame->description, psz_temp );
                }
            }
            else if ( !strcmp(p_frame->id, ID3_FRAME_TITLE ) )
            {
                p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST, FIND_PARENT );
                if( p_playlist )
                {
                    p_playlist->pp_items[p_playlist->i_index]->psz_name = strdup( psz_temp );
                    vlc_object_release( p_playlist );
                }
                input_AddInfo( p_category, (char *)p_frame->description, psz_temp );
            }
            else if ( !strcmp(p_frame->id, ID3_FRAME_ARTIST ) )
            {
                p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST, FIND_PARENT );
                if( p_playlist )
                {
                    p_playlist->pp_items[p_playlist->i_index]->psz_author = strdup( psz_temp );
                    vlc_object_release( p_playlist );
                }
                input_AddInfo( p_category, (char *)p_frame->description, psz_temp );
            }
            else
            {
                input_AddInfo( p_category, (char *)p_frame->description, psz_temp );
            }
            free( psz_temp ); 
        }
        i++;
    }
    id3_tag_delete( p_id3_tag );
    val.b_bool = VLC_TRUE;
    var_Change( p_input, "demuxed-id3", VLC_VAR_SETVALUE, &val, NULL );
}

/*****************************************************************************
 * ParseID3Tags: check if ID3 tags at common locations. Parse them and skip it
 * if it's at the start of the file
 ****************************************************************************/
static int ParseID3Tags( vlc_object_t *p_this )
{
    input_thread_t *p_input;
    u8  *p_peek;
    int i_size;
    int i_size2;

    if ( p_this->i_object_type != VLC_OBJECT_INPUT )
    {
        return( VLC_EGENERIC );
    }
    p_input = (input_thread_t *)p_this;

    msg_Dbg( p_input, "Checking for ID3 tag" );

    if ( p_input->stream.b_seekable &&
         p_input->stream.i_method != INPUT_METHOD_NETWORK )
    {        
        stream_position_t pos;

        /*look for a id3v1 tag at the end of the file*/
        input_Tell( p_input, &pos );
        if ( pos.i_size >128 )
        {
            input_AccessReinit( p_input );
            p_input->pf_seek( p_input, pos.i_size - 128 );
            
            /* get 10 byte id3 header */    
            if( input_Peek( p_input, &p_peek, 10 ) < 10 )
            {
                msg_Err( p_input, "cannot peek()" );
                return( VLC_EGENERIC );
            }
            i_size2 = id3_tag_query( p_peek, 10 );
            if ( i_size2 == 128 )
            {
                /* peek the entire tag */
                if ( input_Peek( p_input, &p_peek, i_size2 ) < i_size2 )
                {
                    msg_Err( p_input, "cannot peek()" );
                    return( VLC_EGENERIC );
                }
                ParseID3Tag( p_input, p_peek, i_size2 );
            }

            /* look for id3v2.4 tag at end of file */
            /* get 10 byte id3 footer */    
            if( input_Peek( p_input, &p_peek, 128 ) < 128 )
            {
                msg_Err( p_input, "cannot peek()" );
                return( VLC_EGENERIC );
            }
            i_size2 = id3_tag_query( p_peek + 118, 10 );
            if ( i_size2 < 0  && pos.i_size > -i_size2 )
            {                                        /* id3v2.4 footer found */
                input_AccessReinit( p_input );
                p_input->pf_seek( p_input, pos.i_size + i_size2 );
                /* peek the entire tag */
                if ( input_Peek( p_input, &p_peek, i_size2 ) < i_size2 )
                {
                    msg_Err( p_input, "cannot peek()" );
                    return( VLC_EGENERIC );
                }
                ParseID3Tag( p_input, p_peek, i_size2 );
            }
        }
        input_AccessReinit( p_input );    
        p_input->pf_seek( p_input, 0 );
    }
    /* get 10 byte id3 header */    
    if( input_Peek( p_input, &p_peek, 10 ) < 10 )
    {
        msg_Err( p_input, "cannot peek()" );
        return( VLC_EGENERIC );
    }

    i_size = id3_tag_query( p_peek, 10 );
    if ( i_size <= 0 )
    {
        return( VLC_SUCCESS );
    }

    /* peek the entire tag */
    if ( input_Peek( p_input, &p_peek, i_size ) < i_size )
    {
        msg_Err( p_input, "cannot peek()" );
        return( VLC_EGENERIC );
    }

    ParseID3Tag( p_input, p_peek, i_size );
    msg_Dbg( p_input, "ID3 tag found, skiping %d bytes", i_size );
    p_input->p_current_data += i_size; /* seek passed end of ID3 tag */
    return( VLC_SUCCESS );
}
