/*****************************************************************************
 * item-ext.c : Playlist item management functions (act on the playlist)
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Cl�ment Stenac <zorglub@videolan.org>
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
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_playlist.h"

/***************************************************************************
 * Item creation/addition functions
 ***************************************************************************/

/**
 * Add a MRL into the playlist, duration and options given
 *
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \param i_duration length of the item in milliseconds.
 * \param ppsz_options an array of options
 * \param i_options the number of options
 * \return The id of the playlist item
*/
int playlist_AddExt( playlist_t *p_playlist, const char * psz_uri,
                     const char *psz_name, int i_mode, int i_pos,
                     mtime_t i_duration, const char **ppsz_options,
                     int i_options )
{
    playlist_item_t *p_item;
    p_item = playlist_ItemNew( p_playlist , psz_uri, psz_name );

    if( p_item == NULL )
    {
        msg_Err( p_playlist, "unable to add item to playlist" );
        return -1;
    }

    p_item->input.i_duration = i_duration;
    p_item->input.i_options = i_options;
    p_item->input.ppsz_options = NULL;

    for( p_item->input.i_options = 0; p_item->input.i_options < i_options;
         p_item->input.i_options++ )
    {
        if( !p_item->input.i_options )
        {
            p_item->input.ppsz_options = malloc( i_options * sizeof(char *) );
            if( !p_item->input.ppsz_options ) break;
        }

        p_item->input.ppsz_options[p_item->input.i_options] =
            strdup( ppsz_options[p_item->input.i_options] );
    }

    return playlist_AddItem( p_playlist, p_item, i_mode, i_pos );
}

/**
 * Add a MRL into the playlist.
 *
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \return The id of the playlist item
*/
int playlist_Add( playlist_t *p_playlist, const char *psz_uri,
                  const char *psz_name, int i_mode, int i_pos )
{
    return playlist_AddExt( p_playlist, psz_uri, psz_name, i_mode, i_pos,
                            -1, NULL, 0 );
}

/**
 * Add a playlist item into a playlist
 *
 * \param p_playlist the playlist to insert into
 * \param p_item the playlist item to insert
 * \param i_mode the mode used when adding
 * \param i_pos the possition in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \return The id of the playlist item
 */
int playlist_AddItem( playlist_t *p_playlist, playlist_item_t *p_item,
                      int i_mode, int i_pos)
{
    vlc_value_t val;
    vlc_bool_t b_end = VLC_FALSE;
    playlist_view_t *p_view;

    vlc_mutex_lock( &p_playlist->object_lock );

    /*
     * CHECK_INSERT : checks if the item is already enqued before
     * enqueing it
     */

    /* That should not change */
    if ( i_mode & PLAYLIST_CHECK_INSERT )
    {
         int j;

        if ( p_playlist->pp_items )
        {
            for ( j = 0; j < p_playlist->i_size; j++ )
            {
                if ( !strcmp( p_playlist->pp_items[j]->input.psz_uri,
                               p_item->input.psz_uri ) )
                {
                    playlist_ItemDelete( p_item );
                    vlc_mutex_unlock( &p_playlist->object_lock );
                    return -1;
                }
             }
         }
         i_mode &= ~PLAYLIST_CHECK_INSERT;
         i_mode |= PLAYLIST_APPEND;
    }

    msg_Dbg( p_playlist, "adding playlist item `%s' ( %s )",
             p_item->input.psz_name, p_item->input.psz_uri );

    p_item->input.i_id = ++p_playlist->i_last_id;

    /* Do a few boundary checks and allocate space for the item */
    if( i_pos == PLAYLIST_END )
    {
        b_end = VLC_TRUE;
        if( i_mode & PLAYLIST_INSERT )
        {
            i_mode &= ~PLAYLIST_INSERT;
            i_mode |= PLAYLIST_APPEND;
        }

        i_pos = p_playlist->i_size - 1;
    }

    if( !(i_mode & PLAYLIST_REPLACE)
         || i_pos < 0 || i_pos >= p_playlist->i_size )
    {
        /* Additional boundary checks */
        if( i_mode & PLAYLIST_APPEND )
        {
            i_pos++;
        }

        if( i_pos < 0 )
        {
            i_pos = 0;
        }
        else if( i_pos > p_playlist->i_size )
        {
            i_pos = p_playlist->i_size;
        }

        INSERT_ELEM( p_playlist->pp_items, p_playlist->i_size, i_pos, p_item );
        p_playlist->i_enabled ++;

        /* We update the ALL view directly */
        playlist_ViewUpdate( p_playlist, VIEW_ALL );

        /* Add the item to the General category */
        if( b_end == VLC_TRUE )
        {
            playlist_NodeAppend( p_playlist, VIEW_CATEGORY, p_item,
                                 p_playlist->p_general );
        }
        else
        {
            playlist_NodeInsert( p_playlist, VIEW_CATEGORY, p_item,
                                 p_playlist->p_general, i_pos );
        }
        p_view = playlist_ViewFind( p_playlist, VIEW_ALL );
        playlist_ItemAddParent( p_item, VIEW_ALL, p_view->p_root );

        /* Also add the item to the "simple" view */
        p_view = playlist_ViewFind( p_playlist, VIEW_SIMPLE );

        if( b_end == VLC_TRUE )
        {
            playlist_NodeAppend( p_playlist, VIEW_SIMPLE,p_item,
                                  p_view->p_root );
        }
        else
        {
            playlist_NodeInsert( p_playlist, VIEW_SIMPLE,p_item,
                                  p_view->p_root, i_pos );
        }

        /* FIXME : Update sorted views */

        if( p_playlist->i_index >= i_pos )
        {
            p_playlist->i_index++;
        }
    }
    else
    {
        msg_Err( p_playlist, "Insert mode not implemented" );
    }

    if( i_mode & PLAYLIST_GO )
    {
        p_playlist->request.b_request = VLC_TRUE;
        /* FIXME ... */
        p_playlist->request.i_view = VIEW_SIMPLE;
        p_playlist->request.p_node = p_view->p_root;
        p_playlist->request.p_item = p_item;

        if( p_playlist->p_input )
        {
            input_StopThread( p_playlist->p_input );
        }
        p_playlist->status.i_status = PLAYLIST_RUNNING;
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return p_item->input.i_id;
}


/**
 * Add a playlist item to a given node (in the category view )
 *
 * \param p_playlist the playlist to insert into
 * \param p_item the playlist item to insert
 * \param i_view the view for which to add or TODO: ALL_VIEWS
 * \param p_parent the parent node
 * \param i_mode the mode used when adding
 * \param i_pos the possition in the node where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the node
 ** \return The id of the playlist item
 */
int playlist_NodeAddItem( playlist_t *p_playlist, playlist_item_t *p_item,
                          int i_view,playlist_item_t *p_parent,
                          int i_mode, int i_pos)
{
    vlc_value_t val;
    int i_position;
    playlist_view_t *p_view;

    vlc_mutex_lock( &p_playlist->object_lock );

    /* Sanity checks */
    if( !p_parent || p_parent->i_children == -1 )
    {
        msg_Err( p_playlist, "invalid node" );
    }

    /*
     * CHECK_INSERT : checks if the item is already enqued before
     * enqueing it
     */
    if ( i_mode & PLAYLIST_CHECK_INSERT )
    {
         int j;

        if ( p_playlist->pp_items )
        {
            for ( j = 0; j < p_playlist->i_size; j++ )
            {
                if ( !strcmp( p_playlist->pp_items[j]->input.psz_uri,
                              p_item->input.psz_uri ) )
                {
                    playlist_ItemDelete( p_item );
                    vlc_mutex_unlock( &p_playlist->object_lock );
                    return -1;
                }
            }
        }
        i_mode &= ~PLAYLIST_CHECK_INSERT;
        i_mode |= PLAYLIST_APPEND;
    }

    msg_Dbg( p_playlist, "adding playlist item `%s' ( %s )",
             p_item->input.psz_name, p_item->input.psz_uri );

    p_item->input.i_id = ++p_playlist->i_last_id;

    /* First, add the item at the right position in the item bank */
    /* WHY THAT ? */
     //i_position = p_playlist->i_index == -1 ? 0 : p_playlist->i_index;
    i_position = p_playlist->i_size ;

    INSERT_ELEM( p_playlist->pp_items,
                 p_playlist->i_size,
                 i_position,
                 p_item );
    p_playlist->i_enabled ++;

    /* TODO: Handle modes */
    playlist_NodeAppend( p_playlist, i_view, p_item, p_parent );

    /* We update the ALL view directly */
    p_view = playlist_ViewFind( p_playlist, VIEW_ALL );
    playlist_ItemAddParent( p_item, VIEW_ALL, p_view->p_root );
    playlist_ViewUpdate( p_playlist, VIEW_ALL );

    /* TODO : Update sorted views*/

    if( i_mode & PLAYLIST_GO )
    {
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.i_view = VIEW_CATEGORY;
        p_playlist->request.p_node = p_parent;
        p_playlist->request.p_item = p_item;
        if( p_playlist->p_input )
        {
            input_StopThread( p_playlist->p_input );
        }
        p_playlist->status.i_status = PLAYLIST_RUNNING;
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return p_item->input.i_id;
}

/***************************************************************************
 * Item search functions
 ***************************************************************************/

/**
 * Search the position of an item by its id
 * This function must be entered with the playlist lock
 *
 * \param p_playlist the playlist
 * \param i_id the id to find
 * \return the position, or VLC_EGENERIC on failure
 */
int playlist_GetPositionById( playlist_t * p_playlist , int i_id )
{
    int i;
    for( i =  0 ; i < p_playlist->i_size ; i++ )
    {
        if( p_playlist->pp_items[i]->input.i_id == i_id )
        {
            return i;
        }
    }
    return VLC_EGENERIC;
}


/**
 * Search an item by its position
 * This function must be entered with the playlist lock
 *
 * \param p_playlist the playlist
 * \param i_pos the position of the item to find
 * \return the item, or NULL on failure
 */
playlist_item_t * playlist_ItemGetByPos( playlist_t * p_playlist , int i_pos )
{
    if( i_pos >= 0 && i_pos < p_playlist->i_size)
    {
        return p_playlist->pp_items[i_pos];
    }
    else if( p_playlist->i_size > 0)
    {
        return p_playlist->pp_items[p_playlist->i_index];
    }
    else
    {
        return NULL;
    }
}

/**
 * Search an item by its id
 *
 * \param p_playlist the playlist
 * \param i_id the id to find
 * \return the item, or NULL on failure
 */
playlist_item_t * playlist_ItemGetById( playlist_t * p_playlist , int i_id )
{
    int i;
    for( i =  0 ; i < p_playlist->i_size ; i++ )
    {
        if( p_playlist->pp_items[i]->input.i_id == i_id )
        {
            return p_playlist->pp_items[i];
        }
    }
    return NULL;
}




/***********************************************************************
 * Misc functions
 ***********************************************************************/

/**
 * Transform an item to a node
 *
 * \param p_playlist the playlist object
 * \param p_item the item to transform
 * \return nothing
 */
void playlist_ItemToNode( playlist_t *p_playlist,playlist_item_t *p_item )
{
    int i = 0;
    if( p_item->i_children == -1 )
    {
        p_item->i_children = 0;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    /* Remove it from the array of available items */
    for( i = 0 ; i < p_playlist->i_size ; i++ )
    {
        if( p_item == p_playlist->pp_items[i] )
        {
            REMOVE_ELEM( p_playlist->pp_items, p_playlist->i_size, i );
        }
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    /* Handle the parents
     * Nothing to do ! */
}

/**
 * delete an item from a playlist.
 *
 * \param p_playlist the playlist to remove from.
 * \param i_id the identifier of the item to delete
 * \return returns VLC_SUCCESS or an error
 */
int playlist_Delete( playlist_t * p_playlist, int i_id )
{
    vlc_value_t     val;
    int             i;

    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_id );

    if( p_item == NULL ) return VLC_EGENERIC;

    /* Check if it is the current item */
    if( p_playlist->status.p_item == p_item )
    {
        playlist_Control( p_playlist, PLAYLIST_STOP );
        p_playlist->status.p_item = NULL;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    msg_Dbg( p_playlist, "deleting playlist item `%s'",
                          p_item->input.psz_name );

    /* Remove the item from all its parent nodes */
    for ( i= 0 ; i < p_item->i_parents ; i++ )
    {
        playlist_NodeRemoveItem( p_playlist, p_item,
                                 p_item->pp_parents[i]->p_parent );
        if( p_item->pp_parents[i]->i_view == VIEW_ALL )
        {
            p_playlist->i_size--;
        }
    }

    /* TODO : Update views */

    playlist_ItemDelete( p_item );

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return VLC_SUCCESS;
}

/**
 * Clear all playlist items
 *
 * \param p_playlist the playlist to be cleared.
 * \return returns 0
 */
int playlist_Clear( playlist_t * p_playlist )
{
    int i;
    for( i = p_playlist->i_size; i > 0 ; i-- )
    {
        playlist_Delete( p_playlist, p_playlist->pp_items[0]->input.i_id );
    }
    return VLC_SUCCESS;
}


/**
 * Disables a playlist item
 *
 * \param p_playlist the playlist to disable from.
 * \param i_pos the position of the item to disable
 * \return returns 0
 */
int playlist_Disable( playlist_t * p_playlist, playlist_item_t *p_item )
{
    vlc_value_t     val;

    if( !p_item ) return VLC_EGENERIC;

    msg_Dbg( p_playlist, "disabling playlist item `%s'",
                   p_item->input.psz_name );

    if( p_item->i_flags & PLAYLIST_ENA_FLAG )
    {
        p_playlist->i_enabled--;
    }
    p_item->i_flags &= ~PLAYLIST_ENA_FLAG;

    var_SetInteger( p_playlist, "item-change", p_item->input.i_id );
    return VLC_SUCCESS;
}

/**
 * Enables a playlist item
 *
 * \param p_playlist the playlist to enable from.
 * \param i_pos the position of the item to enable
 * \return returns 0
 */
int playlist_Enable( playlist_t * p_playlist, playlist_item_t *p_item )
{
    vlc_value_t     val;

    if( !p_item ) return VLC_EGENERIC;

    msg_Dbg( p_playlist, "enabling playlist item `%s'",
                   p_item->input.psz_name );

    if( p_item->i_flags & ~PLAYLIST_ENA_FLAG )
    {
        p_playlist->i_enabled++;
    }
    p_item->i_flags |= PLAYLIST_ENA_FLAG;

    var_SetInteger( p_playlist, "item-change", p_item->input.i_id );
    return VLC_SUCCESS;
}

/**
 * Move an item in a playlist
 *
 * Move the item in the playlist with position i_pos before the current item
 * at position i_newpos.
 * \param p_playlist the playlist to move items in
 * \param i_pos the position of the item to move
 * \param i_newpos the position of the item that will be behind the moved item
 *        after the move
 * \return returns VLC_SUCCESS
 */
int playlist_Move( playlist_t * p_playlist, int i_pos, int i_newpos )
{
    vlc_value_t val;
    vlc_mutex_lock( &p_playlist->object_lock );

    /* take into account that our own row disappears. */
    if( i_pos < i_newpos ) i_newpos--;

    if( i_pos >= 0 && i_newpos >=0 && i_pos <= p_playlist->i_size &&
        i_newpos <= p_playlist->i_size )
    {
        playlist_item_t * temp;

        msg_Dbg( p_playlist, "moving playlist item `%s' (%i -> %i)",
                 p_playlist->pp_items[i_pos]->input.psz_name, i_pos, i_newpos);

        if( i_pos == p_playlist->i_index )
        {
            p_playlist->i_index = i_newpos;
        }
        else if( i_pos > p_playlist->i_index &&
                 i_newpos <= p_playlist->i_index )
        {
            p_playlist->i_index++;
        }
        else if( i_pos < p_playlist->i_index &&
                 i_newpos >= p_playlist->i_index )
        {
            p_playlist->i_index--;
        }

        if ( i_pos < i_newpos )
        {
            temp = p_playlist->pp_items[i_pos];
            while ( i_pos < i_newpos )
            {
                p_playlist->pp_items[i_pos] = p_playlist->pp_items[i_pos+1];
                i_pos++;
            }
            p_playlist->pp_items[i_newpos] = temp;
        }
        else if ( i_pos > i_newpos )
        {
            temp = p_playlist->pp_items[i_pos];
            while ( i_pos > i_newpos )
            {
                p_playlist->pp_items[i_pos] = p_playlist->pp_items[i_pos-1];
                i_pos--;
            }
            p_playlist->pp_items[i_newpos] = temp;
        }
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return VLC_SUCCESS;
}
