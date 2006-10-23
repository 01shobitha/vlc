/*****************************************************************************
 * control.c : Handle control of the playlist & running through it
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id: /local/vlc/0.8.6-playlist-vlm/src/playlist/playlist.c 13741 2006-03-21T19:29:39.792444Z zorglub  $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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
#include <vlc/input.h>
#include "vlc_playlist.h"
#include "playlist_internal.h"
#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int PlaylistVAControl( playlist_t * p_playlist, int i_query, va_list args );

void PreparseEnqueueItemSub( playlist_t *, playlist_item_t * );

playlist_item_t *playlist_RecursiveFindLast(playlist_t *p_playlist,
                                            playlist_item_t *p_node );

/*****************************************************************************
 * Playlist control
 *****************************************************************************/

/**
 * Do a playlist action. Should be entered without playlist lock
 * \see playlist_Control
 */
int playlist_LockControl( playlist_t * p_playlist, int i_query, ... )
{
    va_list args;
    int i_result;
    va_start( args, i_query );
    vlc_mutex_lock( &p_playlist->object_lock );
    i_result = PlaylistVAControl( p_playlist, i_query, args );
    va_end( args );
    vlc_mutex_unlock( &p_playlist->object_lock );
    return i_result;
}

/**
 * Do a playlist action.
 * If there is something in the playlist then you can do playlist actions.
 * Should be entered with playlist lock. See include/vlc_playlist.h for
 * possible queries
 *
 * \param p_playlist the playlist to do the command on
 * \param i_query the command to do
 * \param variable number of arguments
 * \return VLC_SUCCESS or an error
 */
int playlist_Control( playlist_t * p_playlist, int i_query, ... )
{
    va_list args;
    int i_result;
    va_start( args, i_query );
    i_result = PlaylistVAControl( p_playlist, i_query, args );
    va_end( args );

    return i_result;
}

int PlaylistVAControl( playlist_t * p_playlist, int i_query, va_list args )
{
    playlist_item_t *p_item, *p_node;
    vlc_value_t val;

    if( p_playlist->items.i_size <= 0 )
        return VLC_EGENERIC;

    switch( i_query )
    {
    case PLAYLIST_STOP:
        p_playlist->request.i_status = PLAYLIST_STOPPED;
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.p_item = NULL;
        break;

    // Node can be null, it will keep the same. Use with care ...
    // Item null = take the first child of node
    case PLAYLIST_VIEWPLAY:
        p_node = (playlist_item_t *)va_arg( args, playlist_item_t * );
        p_item = (playlist_item_t *)va_arg( args, playlist_item_t * );
        if ( p_node == NULL )
        {
            p_node = p_playlist->status.p_node;
            assert( p_node );
        }
        p_playlist->request.i_status = PLAYLIST_RUNNING;
        p_playlist->request.i_skip = 0;
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.p_node = p_node;
        p_playlist->request.p_item = p_item;
        if( p_item && var_GetBool( p_playlist, "random" ) )
            p_playlist->b_reset_currently_playing = VLC_TRUE;
        break;

    case PLAYLIST_PLAY:
        if( p_playlist->p_input )
        {
            val.i_int = PLAYING_S;
            var_Set( p_playlist->p_input, "state", val );
            break;
        }
        else
        {
            p_playlist->request.i_status = PLAYLIST_RUNNING;
            p_playlist->request.b_request = VLC_TRUE;
            p_playlist->request.p_node = p_playlist->status.p_node;
            p_playlist->request.p_item = p_playlist->status.p_item;
            p_playlist->request.i_skip = 0;
        }
        break;

    case PLAYLIST_AUTOPLAY:
        // AUTOPLAY is an ugly hack for initial status.
        // Hopefully it will disappear
        p_playlist->status.i_status = PLAYLIST_RUNNING;
        p_playlist->request.p_node = p_playlist->status.p_node;
        p_playlist->request.b_request = VLC_FALSE;
        break;

    case PLAYLIST_PAUSE:
        val.i_int = 0;
        if( p_playlist->p_input )
            var_Get( p_playlist->p_input, "state", &val );

        if( val.i_int == PAUSE_S )
        {
            p_playlist->status.i_status = PLAYLIST_RUNNING;
            if( p_playlist->p_input )
            {
                val.i_int = PLAYING_S;
                var_Set( p_playlist->p_input, "state", val );
            }
        }
        else
        {
            p_playlist->status.i_status = PLAYLIST_PAUSED;
            if( p_playlist->p_input )
            {
                val.i_int = PAUSE_S;
                var_Set( p_playlist->p_input, "state", val );
            }
        }
        break;

    case PLAYLIST_SKIP:
        p_playlist->request.p_node = p_playlist->status.p_node;
        p_playlist->request.p_item = p_playlist->status.p_item;
        p_playlist->request.i_skip = (int) va_arg( args, int );
        /* if already running, keep running */
        if( p_playlist->status.i_status != PLAYLIST_STOPPED )
            p_playlist->request.i_status = p_playlist->status.i_status;
        p_playlist->request.b_request = VLC_TRUE;
        break;

    default:
        msg_Err( p_playlist, "unknown playlist query" );
        return VLC_EBADVAR;
        break;
    }
    vlc_cond_signal( &p_playlist->object_wait );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Preparse control
 *****************************************************************************/
/** Enqueue an item for preparsing */
int playlist_PreparseEnqueue( playlist_t *p_playlist,
                              input_item_t *p_item )
{
    vlc_mutex_lock( &p_playlist->p_preparse->object_lock );
    vlc_gc_incref( p_item );
    INSERT_ELEM( p_playlist->p_preparse->pp_waiting,
                 p_playlist->p_preparse->i_waiting,
                 p_playlist->p_preparse->i_waiting,
                 p_item );
    vlc_mutex_unlock( &p_playlist->p_preparse->object_lock );
    vlc_cond_signal( &p_playlist->p_preparse->object_wait );
    return VLC_SUCCESS;
}

/** Enqueue a playlist item or a node for peparsing.
 *  This function should be entered without playlist and preparser locks */
int playlist_PreparseEnqueueItem( playlist_t *p_playlist,
                                  playlist_item_t *p_item )
{
    vlc_mutex_lock( &p_playlist->object_lock );
    vlc_mutex_lock( &p_playlist->p_preparse->object_lock );
    PreparseEnqueueItemSub( p_playlist, p_item );
    vlc_mutex_unlock( &p_playlist->p_preparse->object_lock );
    vlc_mutex_unlock( &p_playlist->object_lock );
    return VLC_SUCCESS;
}

int playlist_AskForArtEnqueue( playlist_t *p_playlist,
                              input_item_t *p_item )
{
    int i;
    preparse_item_t p;
    p.p_item = p_item;
    p.b_fetch_art = VLC_TRUE;

    vlc_mutex_lock( &p_playlist->p_fetcher->object_lock );
    for( i = 0; i < p_playlist->p_fetcher->i_waiting &&
         p_playlist->p_fetcher->p_waiting->b_fetch_art == VLC_TRUE;
         i++ );
    vlc_gc_incref( p_item );
    INSERT_ELEM( p_playlist->p_fetcher->p_waiting,
                 p_playlist->p_fetcher->i_waiting,
                 i, p );
    vlc_mutex_unlock( &p_playlist->p_fetcher->object_lock );
    vlc_cond_signal( &p_playlist->p_fetcher->object_wait );
    return VLC_SUCCESS;
}

void PreparseEnqueueItemSub( playlist_t *p_playlist,
                             playlist_item_t *p_item )
{
    int i;
    if( p_item->i_children == -1 )
    {
        vlc_gc_incref( p_item );
        INSERT_ELEM( p_playlist->p_preparse->pp_waiting,
                     p_playlist->p_preparse->i_waiting,
                     p_playlist->p_preparse->i_waiting,
                     p_item->p_input );
    }
    else
    {
        for( i = 0; i < p_item->i_children; i++)
        {
            PreparseEnqueueItemSub( p_playlist, p_item->pp_children[i] );
        }
    }
}

/*****************************************************************************
 * Playback logic
 *****************************************************************************/

static void ResyncCurrentIndex(playlist_t *p_playlist, playlist_item_t *p_cur )
{
     PL_DEBUG("resyncing on %s", PLI_NAME(p_cur) );
     /* Simply resync index */
     int i;
     p_playlist->i_current_index = -1;
     for( i = 0 ; i< p_playlist->current.i_size; i++ )
     {
          if( ARRAY_VAL(p_playlist->current, i) == p_cur )
          {
              p_playlist->i_current_index = i;
              break;
          }
     }
     PL_DEBUG("%s is at %i", PLI_NAME(p_cur), p_playlist->i_current_index );
}

void ResetCurrentlyPlaying( playlist_t *p_playlist, vlc_bool_t b_random,
                            playlist_item_t *p_cur )
{
    playlist_item_t *p_next = NULL;
    stats_TimerStart( p_playlist, "Items array build",
                      STATS_TIMER_PLAYLIST_BUILD );
    PL_DEBUG("rebuilding array of current - root %s",
              PLI_NAME(p_playlist->status.p_node) );
    ARRAY_RESET(p_playlist->current);
    p_playlist->i_current_index = -1;
    while( 1 )
    {
        /** FIXME: this is *slow* */
        p_next = playlist_GetNextLeaf( p_playlist,
                                       p_playlist->status.p_node,
                                       p_next, VLC_TRUE, VLC_FALSE );
        if( p_next )
        {
            if( p_next == p_cur )
                p_playlist->i_current_index = p_playlist->current.i_size;
            ARRAY_APPEND( p_playlist->current, p_next);
        }
        else break;
    }
    PL_DEBUG("rebuild done - %i items, index %i", p_playlist->current.i_size,
                                                  p_playlist->i_current_index);
    if( b_random )
    {
        /* Shuffle the array */
        srand( (unsigned int)mdate() );
        int swap = 0;
        int j;
        for( j = p_playlist->current.i_size - 1; j > 0; j-- )
        {
            swap++;
            int i = rand() % (j+1); /* between 0 and j */
            playlist_item_t *p_tmp;
            p_tmp = ARRAY_VAL(p_playlist->current, i);
            ARRAY_VAL(p_playlist->current,i) = ARRAY_VAL(p_playlist->current,j);
            ARRAY_VAL(p_playlist->current,j) = p_tmp;
        }
    }
    p_playlist->b_reset_currently_playing = VLC_FALSE;
    stats_TimerStop( p_playlist, STATS_TIMER_PLAYLIST_BUILD );
}

/** This function calculates the next playlist item, depending
 *  on the playlist course mode (forward, backward, random, view,...). */
playlist_item_t * playlist_NextItem( playlist_t *p_playlist )
{
    playlist_item_t *p_new = NULL;
    int i_skip = 0, i;

    vlc_bool_t b_loop = var_GetBool( p_playlist, "loop" );
    vlc_bool_t b_random = var_GetBool( p_playlist, "random" );
    vlc_bool_t b_repeat = var_GetBool( p_playlist, "repeat" );
    vlc_bool_t b_playstop = var_GetBool( p_playlist, "play-and-stop" );

    /* Handle quickly a few special cases */
    /* No items to play */
    if( p_playlist->items.i_size == 0 )
    {
        msg_Info( p_playlist, "playlist is empty" );
        return NULL;
    }

    /* Repeat and play/stop */
    if( !p_playlist->request.b_request && b_repeat == VLC_TRUE &&
         p_playlist->status.p_item )
    {
        msg_Dbg( p_playlist,"repeating item" );
        return p_playlist->status.p_item;
    }
    if( !p_playlist->request.b_request && b_playstop == VLC_TRUE )
    {
        msg_Dbg( p_playlist,"stopping (play and stop)");
        return NULL;
    }

    if( !p_playlist->request.b_request && p_playlist->status.p_item )
    {
        playlist_item_t *p_parent = p_playlist->status.p_item;
        while( p_parent )
        {
            if( p_parent->i_flags & PLAYLIST_SKIP_FLAG )
            {
                msg_Dbg( p_playlist, "blocking item, stopping") ;
                return NULL;
            }
            p_parent = p_parent->p_parent;
        }
    }

    /* Start the real work */
    if( p_playlist->request.b_request )
    {
        p_new = p_playlist->request.p_item;
        i_skip = p_playlist->request.i_skip;
        PL_DEBUG( "processing request item %s node %s skip %i",
                        PLI_NAME( p_playlist->request.p_item ),
                        PLI_NAME( p_playlist->request.p_node ), i_skip );

        if( p_playlist->request.p_node &&
            p_playlist->request.p_node != p_playlist->status.p_node )
        {
            p_playlist->status.p_node = p_playlist->request.p_node;
            p_playlist->b_reset_currently_playing = VLC_TRUE;
        }

        /* If we are asked for a node, dont take it */
        if( i_skip == 0 && ( p_new == NULL || p_new->i_children != -1 ) )
            i_skip++;

        if( p_playlist->b_reset_currently_playing )
            /* A bit too bad to reset twice ... */
            ResetCurrentlyPlaying( p_playlist, b_random, p_new );
        else if( p_new )
            ResyncCurrentIndex( p_playlist, p_new );
        else
            p_playlist->i_current_index = -1;

        if( p_playlist->current.i_size && i_skip > 0 )
        {
            for( i = i_skip; i > 0 ; i-- )
            {
                p_playlist->i_current_index++;
                if( p_playlist->i_current_index == p_playlist->current.i_size )
                {
                    PL_DEBUG( "looping - restarting at beginning of node" );
                    p_playlist->i_current_index = 0;
                }
            }
            p_new = ARRAY_VAL( p_playlist->current,
                               p_playlist->i_current_index );
        }
        else if( p_playlist->current.i_size && i_skip < 0 )
        {
            for( i = i_skip; i < 0 ; i++ )
            {
                p_playlist->i_current_index--;
                if( p_playlist->i_current_index == -1 )
                {
                    PL_DEBUG( "looping - restarting at end of node" );
                    p_playlist->i_current_index = p_playlist->current.i_size-1;
                }
            }
            p_new = ARRAY_VAL( p_playlist->current,
                               p_playlist->i_current_index );
        }
        /* Clear the request */
        p_playlist->request.b_request = VLC_FALSE;
    }
    /* "Automatic" item change ( next ) */
    else
    {
        PL_DEBUG( "changing item without a request (current %i/%i)",
                  p_playlist->i_current_index, p_playlist->current.i_size );
        /* Cant go to next from current item */
        if( p_playlist->status.p_item &&
            p_playlist->status.p_item->i_flags & PLAYLIST_SKIP_FLAG )
            return NULL;

        if( p_playlist->b_reset_currently_playing )
            ResetCurrentlyPlaying( p_playlist, b_random,
                                   p_playlist->status.p_item );

        p_playlist->i_current_index++;
        if( p_playlist->i_current_index == p_playlist->current.i_size )
        {
            if( !b_loop || p_playlist->current.i_size == 0 ) return NULL;
            p_playlist->i_current_index = 0;
        }
        PL_DEBUG( "using item %i", p_playlist->i_current_index );
        if ( p_playlist->current.i_size == 0 ) return NULL;

        p_new = ARRAY_VAL( p_playlist->current, p_playlist->i_current_index );
        /* The new item can't be autoselected  */
        if( p_new != NULL && p_new->i_flags & PLAYLIST_SKIP_FLAG )
            return NULL;
    }
    return p_new;
}

/** Start the input for an item */
int playlist_PlayItem( playlist_t *p_playlist, playlist_item_t *p_item )
{
    vlc_value_t val;
    int i_activity = var_GetInteger( p_playlist, "activity") ;

    msg_Dbg( p_playlist, "creating new input thread" );

    p_item->p_input->i_nb_played++;
    p_playlist->status.p_item = p_item;

    p_playlist->status.i_status = PLAYLIST_RUNNING;

    var_SetInteger( p_playlist, "activity", i_activity +
                    DEFAULT_INPUT_ACTIVITY );
    p_playlist->p_input = input_CreateThread( p_playlist, p_item->p_input );

    val.i_int = p_item->p_input->i_id;
    /* unlock the playlist to set the var...mmm */
    vlc_mutex_unlock( &p_playlist->object_lock);
    var_Set( p_playlist, "playlist-current", val);
    vlc_mutex_lock( &p_playlist->object_lock);

    return VLC_SUCCESS;
}
