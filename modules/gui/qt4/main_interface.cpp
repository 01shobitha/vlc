/*****************************************************************************
 * main_inteface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#include "main_interface.hpp"
#include "input_manager.hpp"
#include "dialogs_provider.hpp"

MainInterface::MainInterface( intf_thread_t *p_intf ) : QWidget( NULL )
{
    fprintf( stderr, "QT Main interface\n" );

    /* Init UI */

    /* Init input manager */
}

void MainInterface::init()
{
    /* Get timer updates */
    QObject::connect( DialogsProvider::getInstance(NULL)->fixed_timer,
    	              SIGNAL( timeout() ), this, SLOT(updateOnTimer() ) );
    /* Tell input manager about the input changes */
    QObject::connect( this, SIGNAL( inputChanged( input_thread_t * ) ),
 	           main_input_manager, SLOT( setInput( input_thread_t * ) ) );
}

MainInterface::~MainInterface()
{
}

void MainInterface::updateOnTimer()
{

}
