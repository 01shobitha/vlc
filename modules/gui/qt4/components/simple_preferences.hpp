/*****************************************************************************
 * simple_preferences.hpp : Simple prefs
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: preferences.hpp 16348 2006-08-25 21:10:10Z zorglub $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _SIMPLEPREFS_H_
#define _SIMPLEPREFS_H_

#include <QListWidget>
#include <vlc/vlc.h>
#include <vlc_interface.h>

enum {
    SPrefsVideo = 0,
    SPrefsAudio,
    SPrefsInputAndCodecs,
//    SPrefsPlaylist,
    SPrefsInterface,
    SPrefsSubtitles,
    SPrefsHotkeys,
    SPrefsMax
};
#define SPrefsDefaultCat SPrefsInterface

class ConfigControl;

class SPrefsCatList : public QListWidget
{
    Q_OBJECT;
public:
    SPrefsCatList( intf_thread_t *, QWidget *);
    virtual ~SPrefsCatList() {};
private:
    intf_thread_t *p_intf;
};

class SPrefsPanel : public QWidget
{
    Q_OBJECT
public:
    SPrefsPanel( intf_thread_t *, QWidget *, int );
    virtual ~SPrefsPanel() {};
    void apply();
    void clean();
private:
    intf_thread_t *p_intf;
    QList<ConfigControl *> controls;
};

#endif
