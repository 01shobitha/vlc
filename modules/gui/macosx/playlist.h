/*****************************************************************************
 * playlist.h: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: playlist.h,v 1.12 2003/11/12 01:22:40 hartman Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
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

/*****************************************************************************
 * VLCPlaylistView interface 
 *****************************************************************************/
@interface VLCPlaylistView : NSTableView
{
}

@end

/*****************************************************************************
 * VLCPlaylist interface 
 *****************************************************************************/
@interface VLCPlaylist : NSObject
{
    int i_moveRow;
    
    IBOutlet id o_window;
    IBOutlet id o_table_view;
    
    IBOutlet id o_status_field;
    IBOutlet id o_tc_name;
    IBOutlet id o_tc_author;

    IBOutlet id o_ctx_menu;
    IBOutlet id o_mi_save_playlist;
    IBOutlet id o_mi_play;
    IBOutlet id o_mi_delete;
    IBOutlet id o_mi_selectall;
}

- (NSMenu *)menuForEvent:(NSEvent *)o_event;

- (IBAction)toggleWindow:(id)sender;
- (IBAction)savePlaylist:(id)sender;
- (IBAction)playItem:(id)sender;
- (IBAction)deleteItems:(id)sender;
- (IBAction)selectAll:(id)sender;

- (void)appendArray:(NSArray*)o_array atPos:(int)i_position enqueue:(BOOL)b_enqueue;

- (void)updateRowSelection;
- (void)playlistUpdated;

@end
