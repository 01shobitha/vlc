/*****************************************************************************
 * info.m: MacOS X info panel
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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

#include "intf.h"
#include "info.h"

/*****************************************************************************
 * VLCInfo implementation 
 *****************************************************************************/
@implementation VLCInfo

- (void)awakeFromNib
{
    [o_window setExcludedFromWindowsMenu: YES];
}

- (id)init
{
    self = [super init];

    if( self != nil )
    {
        o_strings = [[NSMutableDictionary alloc] init];
    }

    return( self );
}

- (void)dealloc
{
    [o_strings release];
    [super dealloc];
}

- (IBAction)toggleInfoPanel:(id)sender
{
    if( [o_window isVisible] )
    {
        [o_window orderOut: sender];
    }
    else
    {
        [o_window orderFront: sender];
        [self updateInfo];
    }
}

- (IBAction)showCategory:(id)sender
{
    NSString * o_title = [o_selector titleOfSelectedItem];
    [o_view setString: [o_strings objectForKey: o_title]];
    [o_view setNeedsDisplay: YES];
}

- (void)updateInfo
{
    NSString *o_selectedPane;
    
    if( ![o_window isVisible] )
    {
        return;
    }
    
    o_selectedPane = [[o_selector selectedItem] title];

    intf_thread_t * p_intf = [NSApp getIntf]; 
    input_thread_t * p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                       FIND_ANYWHERE );

    if ( p_input == NULL )
    {
        return;
    }

    [o_strings removeAllObjects];
    [o_selector removeAllItems];

    vlc_mutex_lock( &p_input->stream.stream_lock );
    input_info_category_t * p_category = p_input->stream.p_info;

    while( p_category )
    {
        [self createInfoView: p_category];
        p_category = p_category->p_next;
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );
    if( p_input ) vlc_object_release( p_input );

    int i_select = [o_selector indexOfItemWithTitle:o_selectedPane];
    if ( i_select < 0 )
    {
        i_select = 0;
    }
    [o_selector selectItemAtIndex: i_select ];
    [self showCategory: o_selector];
}

- (void)createInfoView:(input_info_category_t *)p_category
{
    NSString * o_title;
    NSMutableString * o_content;
    input_info_t * p_info;

    /* Add a category */
    o_title = [NSString stringWithUTF8String: p_category->psz_name];
    [o_selector addItemWithTitle: o_title];

    /* Create empty content string */
    o_content = [NSMutableString string];

    /* Add the fields */
    p_info = p_category->p_info;

    while( p_info )
    {
        [o_content appendFormat: @"%@: %@\n\n", [NSApp localizedString: p_info->psz_name],
                                                [NSApp localizedString: p_info->psz_value]]; 
        p_info = p_info->p_next;
    }

    [o_strings setObject: o_content forKey: o_title];
}

@end

@implementation VLCInfo (NSMenuValidation)
 
- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    BOOL bEnabled = TRUE;

    intf_thread_t * p_intf = [NSApp getIntf];
    input_thread_t * p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                       FIND_ANYWHERE );

    if( [[o_mi title] isEqualToString: _NS("Info")] )
    {
        if( p_input == NULL )
        {
            bEnabled = FALSE;
        }
    }
    if( p_input ) vlc_object_release( p_input );

    return( bEnabled );
}

@end
