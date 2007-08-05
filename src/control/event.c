/*****************************************************************************
 * event.c: New libvlc event control API
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id $
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include <vlc_playlist.h>


/*
 * Private functions
 */


/*
 * Internal libvlc functions
 */

/**************************************************************************
 *       libvlc_event_init (internal) :
 *
 * initialization function.
 **************************************************************************/
void libvlc_event_init( libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
	/* Will certainly be used to install libvlc_instance event */
}

/**************************************************************************
 *       libvlc_event_fini (internal) :
 *
 * finalization function.
 **************************************************************************/
void libvlc_event_fini( libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
}

/**************************************************************************
 *       libvlc_event_manager_init (internal) :
 *
 * Init an object's event manager.
 **************************************************************************/
libvlc_event_manager_t *
libvlc_event_manager_init( void * p_obj, libvlc_instance_t * p_libvlc_inst,
                           libvlc_exception_t *p_e )
{
    libvlc_event_manager_t * p_em;

	p_em = malloc(sizeof( libvlc_event_manager_t ));
	if( !p_em )
	{
		libvlc_exception_raise( p_e, "No Memory left" );
		return NULL;
	}

	p_em->p_obj = p_obj;
	p_em->p_libvlc_instance = p_libvlc_inst;
	ARRAY_INIT( p_em->listeners_groups );

	return p_em;
}

/**************************************************************************
 *       libvlc_event_manager_release (internal) :
 *
 * Init an object's event manager.
 **************************************************************************/
void libvlc_event_manager_release( libvlc_event_manager_t * p_em )
{
	libvlc_event_listeners_group_t * listeners_group;
	libvlc_event_listener_t * listener;

	FOREACH_ARRAY( listeners_group, p_em->listeners_groups )
		FOREACH_ARRAY( listener, listeners_group->listeners )
			free( listener );
		FOREACH_END()
		free( listeners_group );
	FOREACH_END()
	free( p_em );
}

/**************************************************************************
 *       libvlc_event_manager_register_event_type (internal) :
 *
 * Init an object's event manager.
 **************************************************************************/
void libvlc_event_manager_register_event_type(
		libvlc_event_manager_t * p_em,
		libvlc_event_type_t event_type,
		libvlc_exception_t * p_e )
{
	libvlc_event_listeners_group_t * listeners_group;
	listeners_group = malloc(sizeof(libvlc_event_listeners_group_t));
	if( !listeners_group )
	{
		libvlc_exception_raise( p_e, "No Memory left" );
		return;
	}

	listeners_group->event_type = event_type;
	ARRAY_INIT( listeners_group->listeners );

	ARRAY_APPEND( p_em->listeners_groups, listeners_group );
}

/**************************************************************************
 *       libvlc_event_send (internal) :
 *
 * Send a callback.
 **************************************************************************/
void libvlc_event_send( libvlc_event_manager_t * p_em,
                        libvlc_event_t * p_event )
{
	libvlc_event_listeners_group_t * listeners_group;
	libvlc_event_listener_t * listener;
	/* Fill event with the sending object now */
	p_event->p_obj = p_em->p_obj;

	FOREACH_ARRAY( listeners_group, p_em->listeners_groups )
		if( listeners_group->event_type == p_event->type )
		{
			/* We found the group, now send every one the event */
			FOREACH_ARRAY( listener, listeners_group->listeners )
				listener->pf_callback( p_event );
			FOREACH_END()
			break;
		}
	FOREACH_END()
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       libvlc_event_attach (public) :
 *
 * Add a callback for an event.
 **************************************************************************/
void libvlc_event_attach( libvlc_event_manager_t * p_event_manager,
                          libvlc_event_type_t event_type,
                          libvlc_callback_t pf_callback,
                          void *p_user_data,
                          libvlc_exception_t *p_e )
{
	libvlc_event_listeners_group_t * listeners_group;
	libvlc_event_listener_t * listener;
	listener = malloc(sizeof(libvlc_event_listener_t));
	if( !listener )
	{
		libvlc_exception_raise( p_e, "No Memory left" );
		return;
	}
	
	listener->event_type = event_type;
	listener->p_user_data = p_user_data;
	listener->pf_callback = pf_callback;
	
	FOREACH_ARRAY( listeners_group, p_event_manager->listeners_groups )
		if( listeners_group->event_type == listener->event_type )
		{
			ARRAY_APPEND( listeners_group->listeners, listener );
			return;
		}
	FOREACH_END()
	
	free(listener);
	libvlc_exception_raise( p_e,
			"This object event manager doesn't know about '%s' events",
			libvlc_event_type_name(a));
}

/**************************************************************************
 *       libvlc_event_detach (public) :
 *
 * Remove a callback for an event.
 **************************************************************************/
void libvlc_event_detach( libvlc_event_manager_t *p_event_manager,
                          libvlc_event_type_t event_type,
                          libvlc_callback_t pf_callback,
						  void *p_user_data,
                          libvlc_exception_t *p_e )
{
	libvlc_event_listeners_group_t * listeners_group;
	libvlc_event_listener_t * listener;
	FOREACH_ARRAY( listeners_group, p_event_manager->listeners_groups )
		if( listeners_group->event_type == event_type )
		{
			FOREACH_ARRAY( listener, listeners_group->listeners )
				if( listener->event_type == event_type &&
					listener->pf_callback == pf_callback &&
					listener->p_user_data == p_user_data )
				{
					/* that's our listener */
					free( listener );
					ARRAY_REMOVE( listeners_group->listeners,
						fe_idx /* This comes from the macro (and that's why
								  I hate macro) */ );
					return;
				}
			FOREACH_END()
		}
	FOREACH_END()
	
	libvlc_exception_raise( p_e,
			"This object event manager doesn't know about '%i,%p,%p' event observer",
			event_type, pf_callback, p_user_data );
}

