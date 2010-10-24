/*****************************************************************************
 * panels.hpp : Panels for the playlist
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
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

#ifndef _PLPANELS_H_
#define _PLPANELS_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"
#include "components/playlist/playlist.hpp"

#include <QModelIndex>
#include <QWidget>
#include <QString>
#include <QToolBar>

#include <vlc_playlist.h>

class QSignalMapper;
class QTreeView;
class QListView;
class PLModel;
class QPushButton;
class QKeyEvent;
class QWheelEvent;
class QStackedLayout;
class PlIconView;
class PlListView;
class LocationBar;
class PLSelector;
class QAbstractItemView;
class PlaylistWidget;

class StandardPLPanel: public QWidget
{
    Q_OBJECT

public:
    StandardPLPanel( PlaylistWidget *, intf_thread_t *,
                     playlist_t *, playlist_item_t *, PLSelector * );
    virtual ~StandardPLPanel();
protected:
    friend class PlaylistWidget;

    PLModel *model;
private:
    enum {
      TREE_VIEW = 0,
      ICON_VIEW,
      LIST_VIEW,

      VIEW_COUNT
    };

    intf_thread_t *p_intf;

    QWidget     *parent;
    QGridLayout *layout;
    PLSelector  *p_selector;

    QTreeView   *treeView;
    PlIconView  *iconView;
    PlListView  *listView;
    QAbstractItemView *currentView;
    QStackedLayout *viewStack;

    int currentRootId;
    QSignalMapper *selectColumnsSigMapper;

    int lastActivatedId;
    int currentRootIndexId;

    void createTreeView();
    void createIconView();
    void createListView();
    void wheelEvent( QWheelEvent *e );
    bool eventFilter ( QObject * watched, QEvent * event );

public slots:
    void setRoot( playlist_item_t * );
    void browseInto( const QModelIndex& );
    void browseInto( );

private slots:
    void deleteSelection();
    void handleExpansion( const QModelIndex& );
    void handleRootChange();
    void activate( const QModelIndex & );
    void browseInto( input_item_t * );

    void gotoPlayingItem();

    void search( const QString& searchText );
    void searchDelayed( const QString& searchText );

    void popupPlView( const QPoint & );
    void popupSelectColumn( QPoint );
    void toggleColumnShown( int );

    void showView( int );
    void cycleViews();

signals:
    void viewChanged( const QModelIndex& );
};

#endif
