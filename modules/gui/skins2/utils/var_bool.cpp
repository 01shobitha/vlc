/*****************************************************************************
 * var_bool.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: var_bool.cpp,v 1.2 2004/01/11 17:12:17 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teuli�re <ipkiss@via.ecp.fr>
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

#include "var_bool.hpp"


const string VarBool::m_type = "bool";


VarBool::VarBool( intf_thread_t *pIntf ): Variable( pIntf ), m_value( false )
{
}


void VarBool::set( bool value )
{
    if( value != m_value )
    {
        m_value = value;

        notify();
    }
}

