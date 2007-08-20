/*****************************************************************************
 * media_list.c: libvlc new API media list functions
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#include "libvlc_internal.h"
#include <vlc/libvlc.h>
#include <assert.h>
#include "vlc_arrays.h"

/*
 * Private functions
 */



/**************************************************************************
 *       notify_item_addition (private)
 *
 * Do the appropriate action when an item is deleted.
 **************************************************************************/
static void
notify_item_addition( libvlc_media_list_t * p_mlist,
                      libvlc_media_descriptor_t * p_md,
                      int index )
{
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaListItemAdded;
    event.u.media_list_item_added.item = p_md;
    event.u.media_list_item_added.index = index;

    /* Send the event */
    libvlc_event_send( p_mlist->p_event_manager, &event );
}

/**************************************************************************
 *       notify_item_deletion (private)
 *
 * Do the appropriate action when an item is added.
 **************************************************************************/
static void
notify_item_deletion( libvlc_media_list_t * p_mlist,
                      libvlc_media_descriptor_t * p_md,
                      int index )
{
    libvlc_event_t event;

    /* Construct the event */
    event.type = libvlc_MediaListItemDeleted;
    event.u.media_list_item_deleted.item = p_md;
    event.u.media_list_item_deleted.index = index;

    /* Send the event */
    libvlc_event_send( p_mlist->p_event_manager, &event );
}

/**************************************************************************
 *       media_descriptor_changed (private) (libvlc Event Callback )
 *
 * An item has changed.
 **************************************************************************/
static void
media_descriptor_changed( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_list_t * p_mlist = user_data;
    libvlc_media_descriptor_t * p_md = p_event->p_obj;
    libvlc_event_t event;
    
    /* Construct the new media list event */
    event.type = libvlc_MediaListItemChanged;
    event.u.media_list_item_changed.item = p_md;

    /* XXX: this is not good, but there is a solution in the pipeline */
    event.u.media_list_item_changed.index =
        libvlc_media_list_index_of_item( p_mlist, p_md, NULL );

    /* Send the event */
    libvlc_event_send( p_mlist->p_event_manager, &event );
}

/**************************************************************************
 *       media_descriptor_subitem_added (private) (libvlc Event Callback )
 *
 * An item (which is a playlist) has gained sub child.
 **************************************************************************/
static void
media_descriptor_subitem_added( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_list_t * p_mlist = user_data;
    libvlc_media_descriptor_t * p_new_md;
    libvlc_media_descriptor_t * p_parent_md = p_event->p_obj;
    int count;
    char * psz_parent_tag = NULL;
    char * psz_parent_name, *psz_tag;

    p_new_md = p_event->u.media_descriptor_subitem_added.new_child;

    /* Here we need something way clever */
    /* We just set the  */
    count = libvlc_media_descriptor_tags_count_for_key( p_parent_md,
                                                        "VLCNode",
                                                        NULL );
    psz_parent_name = libvlc_media_descriptor_get_meta(
                                p_parent_md, libvlc_meta_Title, NULL );
    if( count > 0 )
    {
        psz_parent_tag = libvlc_media_descriptor_tag_at_index_for_key(
                                            p_parent_md, 0, "VLCNode", NULL );
        asprintf( &psz_tag, "%s/%s", psz_parent_tag, psz_parent_name );
        free( psz_parent_tag );
        free( psz_parent_name );
    }
    else
        psz_tag = psz_parent_name;

    libvlc_media_descriptor_add_tag( p_new_md, "VLCNode", psz_tag, NULL );
    libvlc_media_list_add_media_descriptor( p_mlist, p_new_md, NULL );
}

/**************************************************************************
 *       install_media_descriptor_observer (private)
 *
 * Do the appropriate action when an item is deleted.
 **************************************************************************/
static void
install_media_descriptor_observer( libvlc_media_list_t * p_mlist,
                                   libvlc_media_descriptor_t * p_md )
{
    libvlc_event_attach( p_md->p_event_manager,
                         libvlc_MediaDescriptorMetaChanged,
                         media_descriptor_changed,
                         p_mlist, NULL );
    libvlc_event_attach( p_md->p_event_manager,
                         libvlc_MediaDescriptorSubItemAdded,
                         media_descriptor_subitem_added,
                         p_mlist, NULL );
}

/**************************************************************************
 *       uninstall_media_descriptor_observer (private)
 *
 * Do the appropriate action when an item is deleted.
 **************************************************************************/
static void
uninstall_media_descriptor_observer( libvlc_media_list_t * p_mlist,
                                     libvlc_media_descriptor_t * p_md )
{
    libvlc_event_detach( p_md->p_event_manager,
                         libvlc_MediaDescriptorMetaChanged,
                         media_descriptor_changed,
                         p_mlist, NULL );
    libvlc_event_detach( p_md->p_event_manager,
                         libvlc_MediaDescriptorSubItemAdded,
                         media_descriptor_subitem_added,
                         p_mlist, NULL );
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       libvlc_media_list_new (Public)
 *
 * Init an object.
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_list_new( libvlc_instance_t * p_inst,
                       libvlc_exception_t * p_e )
{
    libvlc_media_list_t * p_mlist;

    p_mlist = malloc(sizeof(libvlc_media_list_t));

    if( !p_mlist )
        return NULL;
    
    p_mlist->p_libvlc_instance = p_inst;
    p_mlist->p_event_manager = libvlc_event_manager_new( p_mlist, p_inst, p_e );

    libvlc_event_manager_register_event_type( p_mlist->p_event_manager,
            libvlc_MediaListItemAdded, p_e );
    libvlc_event_manager_register_event_type( p_mlist->p_event_manager,
            libvlc_MediaListItemChanged, p_e );
    libvlc_event_manager_register_event_type( p_mlist->p_event_manager,
            libvlc_MediaListItemDeleted, p_e );

    if( libvlc_exception_raised( p_e ) )
    {
        libvlc_event_manager_release( p_mlist->p_event_manager );
        free( p_mlist );
        return NULL;
    }

    vlc_mutex_init( p_inst->p_libvlc_int, &p_mlist->object_lock );
    
    ARRAY_INIT(p_mlist->items);
    p_mlist->i_refcount = 1;
    p_mlist->psz_name = NULL;

    return p_mlist;
}

/**************************************************************************
 *       libvlc_media_list_release (Public)
 *
 * Release an object.
 **************************************************************************/
void libvlc_media_list_release( libvlc_media_list_t * p_mlist )
{
    libvlc_media_descriptor_t * p_md;

    vlc_mutex_lock( &p_mlist->object_lock );
    p_mlist->i_refcount--;
    if( p_mlist->i_refcount > 0 )
    {
        vlc_mutex_unlock( &p_mlist->object_lock );        
        return;
    }
    vlc_mutex_unlock( &p_mlist->object_lock );        

    /* Refcount null, time to free */

    libvlc_event_manager_release( p_mlist->p_event_manager );

    FOREACH_ARRAY( p_md, p_mlist->items )
        uninstall_media_descriptor_observer( p_mlist, p_md );
        libvlc_media_descriptor_release( p_md );
    FOREACH_END()
 
    free( p_mlist );
}

/**************************************************************************
 *       libvlc_media_list_retain (Public)
 *
 * Increase an object refcount.
 **************************************************************************/
void libvlc_media_list_retain( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_lock( &p_mlist->object_lock );
    p_mlist->i_refcount++;
    vlc_mutex_unlock( &p_mlist->object_lock );
}


/**************************************************************************
 *       add_file_content (Public)
 **************************************************************************/
void
libvlc_media_list_add_file_content( libvlc_media_list_t * p_mlist,
                                    const char * psz_uri,
                                    libvlc_exception_t * p_e )
{
    input_item_t * p_input_item;
    libvlc_media_descriptor_t * p_md;

    p_input_item = input_ItemNewExt( p_mlist->p_libvlc_instance->p_libvlc_int, psz_uri,
                                _("Media Library"), 0, NULL, -1 );

    if( !p_input_item )
    {
        libvlc_exception_raise( p_e, "Can't create an input item" );
        return;
    }

    p_md = libvlc_media_descriptor_new_from_input_item(
            p_mlist->p_libvlc_instance,
            p_input_item, p_e );

    if( !p_md )
    {
        vlc_gc_decref( p_input_item );
        return;
    }

    libvlc_media_list_add_media_descriptor( p_mlist, p_md, p_e );
    if( libvlc_exception_raised( p_e ) )
        return;

    input_Read( p_mlist->p_libvlc_instance->p_libvlc_int, p_input_item, VLC_TRUE );

    return;
}

/**************************************************************************
 *       set_name (Public)
 **************************************************************************/
void libvlc_media_list_set_name( libvlc_media_list_t * p_mlist,
                                 const char * psz_name,
                                 libvlc_exception_t * p_e)

{
    (void)p_e;
    vlc_mutex_lock( &p_mlist->object_lock );
    free( p_mlist->psz_name );
    p_mlist->psz_name = psz_name ? strdup( psz_name ) : NULL;
    vlc_mutex_unlock( &p_mlist->object_lock );
}

/**************************************************************************
 *       name (Public)
 **************************************************************************/
char * libvlc_media_list_name( libvlc_media_list_t * p_mlist,
                               libvlc_exception_t * p_e)
{
    char *ret;
    (void)p_e;

    vlc_mutex_lock( &p_mlist->object_lock );
    ret = p_mlist->psz_name ? strdup( p_mlist->psz_name ) : NULL;
    vlc_mutex_unlock( &p_mlist->object_lock );

    return ret;
}

/**************************************************************************
 *       libvlc_media_list_count (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
int libvlc_media_list_count( libvlc_media_list_t * p_mlist,
                             libvlc_exception_t * p_e )
{
    (void)p_e;
    return p_mlist->items.i_size;
}

/**************************************************************************
 *       libvlc_media_list_add_media_descriptor (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
void libvlc_media_list_add_media_descriptor( 
                                   libvlc_media_list_t * p_mlist,
                                   libvlc_media_descriptor_t * p_md,
                                   libvlc_exception_t * p_e )
{
    (void)p_e;
    libvlc_media_descriptor_retain( p_md );
    ARRAY_APPEND( p_mlist->items, p_md );
    notify_item_addition( p_mlist, p_md, p_mlist->items.i_size-1 );
    install_media_descriptor_observer( p_mlist, p_md );
}

/**************************************************************************
 *       libvlc_media_list_insert_media_descriptor (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
void libvlc_media_list_insert_media_descriptor( 
                                   libvlc_media_list_t * p_mlist,
                                   libvlc_media_descriptor_t * p_md,
                                   int index,
                                   libvlc_exception_t * p_e )
{
    (void)p_e;
    libvlc_media_descriptor_retain( p_md );

    ARRAY_INSERT( p_mlist->items, p_md, index);
    notify_item_addition( p_mlist, p_md, index );
    install_media_descriptor_observer( p_mlist, p_md );
}

/**************************************************************************
 *       libvlc_media_list_remove_index (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
void libvlc_media_list_remove_index( libvlc_media_list_t * p_mlist,
                                     int index,
                                     libvlc_exception_t * p_e )
{
    libvlc_media_descriptor_t * p_md;

    p_md = ARRAY_VAL( p_mlist->items, index );

    uninstall_media_descriptor_observer( p_mlist, p_md );

    ARRAY_REMOVE( p_mlist->items, index )
    notify_item_deletion( p_mlist, p_md, index );

    libvlc_media_descriptor_release( p_md );
}

/**************************************************************************
 *       libvlc_media_list_item_at_index (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
libvlc_media_descriptor_t *
libvlc_media_list_item_at_index( libvlc_media_list_t * p_mlist,
                                 int index,
                                 libvlc_exception_t * p_e )
{
    libvlc_media_descriptor_t * p_md =  ARRAY_VAL( p_mlist->items, index );
    libvlc_media_descriptor_retain( p_md );
    return p_md;
}

/**************************************************************************
 *       libvlc_media_list_index_of_item (Public)
 *
 * Lock should be hold when entering.
 * Warning: this function would return the first matching item
 **************************************************************************/
int libvlc_media_list_index_of_item( libvlc_media_list_t * p_mlist,
                                     libvlc_media_descriptor_t * p_searched_md,
                                     libvlc_exception_t * p_e )
{
    libvlc_media_descriptor_t * p_md;
    FOREACH_ARRAY( p_md, p_mlist->items )
        if( p_searched_md == p_md )
            return fe_idx; /* Once more, we hate macro for that */
    FOREACH_END()
    return -1;
}

/**************************************************************************
 *       libvlc_media_list_lock (Public)
 *
 * The lock must be held in access operations. It is never used in the
 * Public method.
 **************************************************************************/
void libvlc_media_list_lock( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_lock( &p_mlist->object_lock );
}


/**************************************************************************
 *       libvlc_media_list_unlock (Public)
 *
 * The lock must be held in access operations
 **************************************************************************/
void libvlc_media_list_unlock( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_unlock( &p_mlist->object_lock );
}



/**************************************************************************
 *       libvlc_media_list_p_event_manager (Public)
 *
 * The p_event_manager is immutable, so you don't have to hold the lock
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_list_event_manager( libvlc_media_list_t * p_mlist,
                                    libvlc_exception_t * p_e )
{
    (void)p_e;
    return p_mlist->p_event_manager;
}
