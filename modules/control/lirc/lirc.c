/*****************************************************************************
 * lirc.c : lirc module for vlc
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: lirc.c,v 1.10 2004/01/25 16:17:03 anil Exp $
 *
 * Author: Sigmund Augdal <sigmunau@idi.ntnu.no>
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

#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <osd.h>

#include <lirc/lirc_client.h>

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
struct intf_sys_t
{
    struct lirc_config *config;
    vlc_mutex_t         change_lock;

    input_thread_t *    p_input;
    vout_thread_t *     p_vout;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );
static void Feedback( intf_thread_t *, char * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Infrared remote control interface") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    int i_fd;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return 1;
    }

    p_intf->pf_run = Run;

    i_fd = lirc_init( "vlc", 1 );
    if( i_fd == -1 )
    {
        msg_Err( p_intf, "lirc_init failed" );
        free( p_intf->p_sys );
        return 1;
    }

    /* We want polling */
    fcntl( i_fd, F_SETFL, fcntl( i_fd, F_GETFL ) | O_NONBLOCK );

    if( lirc_readconfig( NULL, &p_intf->p_sys->config, NULL ) != 0 )
    {
        msg_Err( p_intf, "lirc_readconfig failed" );
        lirc_deinit();
        free( p_intf->p_sys );
        return 1;
    }

    p_intf->p_sys->p_input = NULL;

    return 0;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }
    if( p_intf->p_sys->p_vout )
    {
        vlc_object_release( p_intf->p_sys->p_vout );
    }
    /* Destroy structure */
    lirc_freeconfig( p_intf->p_sys->config );
    lirc_deinit();
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    char *code, *c;
    playlist_t *p_playlist;
    input_thread_t *p_input;
    vout_thread_t *p_vout = NULL;

    while( !p_intf->b_die )
    {
        /* Sleep a bit */
        msleep( INTF_IDLE_SLEEP );

        /* Update the input */
        if( p_intf->p_sys->p_input == NULL )
        {
            p_intf->p_sys->p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                              FIND_ANYWHERE );
        }
        else if( p_intf->p_sys->p_input->b_dead )
        {
            vlc_object_release( p_intf->p_sys->p_input );
            p_intf->p_sys->p_input = NULL;
        }
        p_input = p_intf->p_sys->p_input;

        /* Update the vout */
        if( p_vout == NULL )
        {
            p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                      FIND_ANYWHERE );
            p_intf->p_sys->p_vout = p_vout;
        }
        else if( p_vout->b_die )
        {
            vlc_object_release( p_vout );
            p_vout = NULL;
            p_intf->p_sys->p_vout = NULL;
        }

        /* We poll the lircsocket */
        if( lirc_nextcode(&code) != 0 )
        {
            break;
        }

        if( code == NULL )
        {
            continue;
        }

        while( !p_intf->b_die
                && lirc_code2char( p_intf->p_sys->config, code, &c ) == 0
                && c != NULL )
        {

            if( !strcmp( c, "QUIT" ) )
            {
                p_intf->p_vlc->b_die = VLC_TRUE;
                Feedback( p_intf, _("Quit" ) );
                continue;
            }
            if( !strcmp( c, "VOL_UP" ) )
            {
                audio_volume_t i_newvol;
                char string[9];
                aout_VolumeUp( p_intf, 1, &i_newvol );
                sprintf( string, "Vol %%%d", i_newvol*100/AOUT_VOLUME_MAX );
                Feedback( p_intf, string );
            }
            if( !strcmp( c, "VOL_DOWN" ) )
            {
                audio_volume_t i_newvol;
                char string[9];
                aout_VolumeDown( p_intf, 1, &i_newvol );
                sprintf( string, "Vol %%%d", i_newvol*100/AOUT_VOLUME_MAX );
                Feedback( p_intf, string );
            }
            if( p_vout )
            {
                if( !strcmp( c, "FULLSCREEN" ) )
                {
                    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                    continue;
                }
                if( !strcmp( c, "ACTIVATE" ) )
                {
                    vlc_value_t val;
                    val.psz_string = "ENTER";
                    if (var_Set( p_vout, "key-pressed", val ) != VLC_SUCCESS)
                    {
                        msg_Warn( p_intf, "key-press failed" );
                    }
                    continue;
                }

                if( !strcmp( c, "LEFT" ) )
                {
                    vlc_value_t val;
                    val.psz_string = "LEFT";
                    if (var_Set( p_vout, "key-pressed", val ) != VLC_SUCCESS)
                    {
                        msg_Warn( p_intf, "key-press failed" );
                    }
                    continue;
                }

                if( !strcmp( c, "RIGHT" ) )
                {
                    vlc_value_t val;
                    val.psz_string = "RIGHT";
                    if (var_Set( p_vout, "key-pressed", val ) != VLC_SUCCESS)
                    {
                        msg_Warn( p_intf, "key-press failed" );
                    }
                    continue;
                }

                if( !strcmp( c, "UP" ) )
                {
                    vlc_value_t val;
                    val.psz_string = "UP";
                    if (var_Set( p_vout, "key-pressed", val ) != VLC_SUCCESS)
                    {
                        msg_Warn( p_intf, "key-press failed" );
                    }
                    continue;
                }

                if( !strcmp( c, "DOWN" ) )
                {
                    vlc_value_t val;
                    val.psz_string = "DOWN";
                    if (var_Set( p_vout, "key-pressed", val ) != VLC_SUCCESS)
                    {
                        msg_Warn( p_intf, "key-press failed" );
                    }
                    continue;
                }
            }

            if( !strcmp( c, "PLAY" ) )
            {
                p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                      FIND_ANYWHERE );
                if( p_playlist )
                {
                    vlc_mutex_lock( &p_playlist->object_lock );
                    if( p_playlist->i_size )
                    {
                        vlc_mutex_unlock( &p_playlist->object_lock );
                        playlist_Play( p_playlist );
                        vlc_object_release( p_playlist );
                    }
                }
                continue;
            }

            if( !strcmp( c, "PLAYPAUSE" ) )
            {
                if( p_input &&
                    p_input->stream.control.i_status != PAUSE_S )
                {
                    Feedback( p_intf, _( "Pause" ) );
                    input_SetStatus( p_input, INPUT_STATUS_PAUSE );
                }
                else
                {
                    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                  FIND_ANYWHERE );
                    if( p_playlist )
                    {
                        vlc_mutex_lock( &p_playlist->object_lock );
                        if( p_playlist->i_size )
                        {
                            vlc_mutex_unlock( &p_playlist->object_lock );
                            Feedback( p_intf, _( "Play" ) );
                            playlist_Play( p_playlist );
                            vlc_object_release( p_playlist );
                        }
                    }
                }
                continue;
            }

            else if( p_input )
            {
                if( !strcmp( c, "PAUSE" ) )
                {
                    Feedback( p_intf, _( "Pause" ) );
                    input_SetStatus( p_input, INPUT_STATUS_PAUSE );
                }
                else if( !strcmp( c, "NEXT" ) )
                {
                    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                          FIND_ANYWHERE );
                    if( p_playlist )
                    {
                        playlist_Next( p_playlist );
                        vlc_object_release( p_playlist );
                    }
                }
                else if( !strcmp( c, "PREV" ) )
                {
                    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                          FIND_ANYWHERE );
                    if( p_playlist )
                    {
                        playlist_Prev( p_playlist );
                        vlc_object_release( p_playlist );
                    }
                }
                else if( !strcmp( c, "STOP" ) )
                {
                    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                          FIND_ANYWHERE );
                    if( p_playlist )
                    {
                        playlist_Stop( p_playlist );
                        vlc_object_release( p_playlist );
                    }
                }
                else if( !strcmp( c, "FAST" ) )
                {
                    input_SetStatus( p_input, INPUT_STATUS_FASTER );
                }
                else if( !strcmp( c, "SLOW" ) )
                {
                    input_SetStatus( p_input, INPUT_STATUS_SLOWER );
                }
/* beginning of modifications by stephane Thu Jun 19 15:29:49 CEST 2003 */
                else if ( !strcmp(c, "CHAPTER_N" ) ||
                          !strcmp( c, "CHAPTER_P" ) )
                {
                    unsigned int i_chapter = 0;

                    if( !strcmp( c, "CHAPTER_N" ) )
                    {
                        vlc_mutex_lock( &p_input->stream.stream_lock );
                        i_chapter = p_input->stream.p_selected_area->i_part + 1;
                        vlc_mutex_unlock( &p_input->stream.stream_lock );
                    }
                    else if( !strcmp( c, "CHAPTER_P" ) )
                    {
                        vlc_mutex_lock( &p_input->stream.stream_lock );
                        i_chapter = p_input->stream.p_selected_area->i_part - 1;
                        vlc_mutex_unlock( &p_input->stream.stream_lock );
                    }

                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    if( ( i_chapter > 0 ) && ( i_chapter <
                                               p_input->stream.p_selected_area->i_part_nb ) )
                    {
                        input_area_t *p_area = p_input->stream.p_selected_area;
                        p_input->stream.p_selected_area->i_part = i_chapter;
                        vlc_mutex_unlock( &p_input->stream.stream_lock );
                        input_ChangeArea( p_input, p_area );
                        input_SetStatus( p_input, INPUT_STATUS_PLAY );
                        vlc_mutex_lock( &p_input->stream.stream_lock );
                    }
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                }
/* end of modification by stephane Thu Jun 19 15:29:49 CEST 2003 */
            }
        }

        free( code );
    }
}

static void Feedback( intf_thread_t *p_intf, char *psz_string )
{
    if ( p_intf->p_sys->p_vout )
    {
        vout_ShowTextRelative( p_intf->p_sys->p_vout, psz_string, NULL,
                                 OSD_ALIGN_TOP|OSD_ALIGN_RIGHT, 30,20,400000 );
    }
}
