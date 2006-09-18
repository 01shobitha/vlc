/*****************************************************************************
 * extended.hpp : Extended controls - Undocked
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: streaminfo.hpp 16687 2006-09-17 12:15:42Z jb $
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

#ifndef _EXTENDED_DIALOG_H_
#define _EXTENDED_DIALOG_H_

#include "util/qvlcframe.hpp"
#include <QHBoxLayout>

class ExtendedDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static ExtendedDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new ExtendedDialog( p_intf );
        return instance;
    }
    virtual ~ExtendedDialog();
private:
    ExtendedDialog( intf_thread_t * );
    static ExtendedDialog *instance;
public slots:
};

#endif
