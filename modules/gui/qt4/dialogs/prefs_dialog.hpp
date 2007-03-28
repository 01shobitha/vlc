/*****************************************************************************
 * prefs_dialog.hpp : Preferences
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
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

#ifndef _PREFS_DIALOG_H_
#define _PREFS_DIALOG_H_

#include "util/qvlcframe.hpp"
#include "components/simple_preferences.hpp"

class PrefsTree;
class SPrefsCatList;
class PrefsPanel;
class SPrefsPanel;
class QTreeWidgetItem;
class QTreeWidget;
class QHBoxLayout;
class QVBoxLayout;
class QGroupBox;
class QRadioButton;
class QWidget;
class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QLabel;

class PrefsDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static PrefsDialog * getInstance( intf_thread_t *_p_intf )
    {
        if( !instance )
            instance = new PrefsDialog( _p_intf );
        return instance;
    }
    virtual ~PrefsDialog() {};

    void showModulePrefs( char* );
private:
    PrefsDialog( intf_thread_t * );

    QWidget *main_panel;
    QHBoxLayout *main_panel_l;
    PrefsPanel *advanced_panel;
    SPrefsPanel *simple_panel;
    SPrefsPanel *simple_panels[SPrefsMax];

    QWidget *tree_panel;
    QHBoxLayout *tree_panel_l;
    SPrefsCatList *simple_tree;
    PrefsTree *advanced_tree;

    QGroupBox *types;
    QRadioButton *small,*all;

    QGridLayout *main_layout;

//    QLabel *panel_label;

    static PrefsDialog *instance;
private slots:
     void changePanel( QTreeWidgetItem * );
     void changeSimplePanel( QListWidgetItem * );
     void setAll();
     void setSmall();
     void save();
     void cancel();
     void reset();
};

#endif
