/*****************************************************************************
 * var_manager.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: var_manager.cpp,v 1.2 2004/01/11 17:12:17 asmax Exp $
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

#include "var_manager.hpp"


VarManager::VarManager( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_tooltipText( pIntf ), m_helpText( pIntf )
{
}


VarManager *VarManager::instance( intf_thread_t *pIntf )
{
    if( ! pIntf->p_sys->p_varManager )
    {
        VarManager *pVarManager;
        pVarManager = new VarManager( pIntf );
        if( pVarManager )
        {
            pIntf->p_sys->p_varManager = pVarManager;
        }
    }
    return pIntf->p_sys->p_varManager;
}


void VarManager::destroy( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_varManager )
    {
        delete pIntf->p_sys->p_varManager;
        pIntf->p_sys->p_varManager = NULL;
    }
}


void VarManager::registerVar( const VariablePtr &rcVar, const string &rName )
{
    m_varMap[rName] = rcVar;
}


Variable *VarManager::getVar( const string &rName )
{
    if( m_varMap.find( rName ) != m_varMap.end() )
    {
        return m_varMap[rName].get();
    }
    else
    {
        return NULL;
    }
}


Variable *VarManager::getVar( const string &rName, const string &rType )
{
    if( m_varMap.find( rName ) != m_varMap.end() )
    {
        Variable *pVar = m_varMap[rName].get();
        // Check the variable type
        if( pVar->getType() != rType )
        {
            msg_Warn( getIntf(), "Variable %s has incorrect type (%s instead"
                      " of (%s)", rName.c_str(), pVar->getType().c_str(),
                      rType.c_str() );
            return NULL;
        }
        else
        {
            return pVar;
        }
    }
    else
    {
        return NULL;
    }
}

