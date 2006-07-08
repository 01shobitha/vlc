/*****************************************************************************
 * interaction.h: Mac OS X interaction dialogs
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix Kühne <fkuehne at videolan dot org>
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

#import "intf.h"
#import "interaction.h"

/*****************************************************************************
 * VLCInteractionList implementation
 *****************************************************************************/
@implementation VLCInteractionList

-(id)init
{
    [super init];
    o_interaction_list = [[NSMutableArray alloc] initWithCapacity:1];
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(newInteractionEvent:)
        name: @"VLCNewInteractionEventNotification"
        object:self];

    return self;
}

-(void)newInteractionEvent: (NSNotification *)o_notification
{
    VLCInteraction *o_interaction;
    NSValue *o_value = [[o_notification userInfo] objectForKey:@"VLCDialogPointer"];
    interaction_dialog_t *p_dialog = [o_value pointerValue];

    switch( p_dialog->i_action )
    {
    case INTERACT_NEW:
        [self addInteraction: p_dialog];
        break;
    case INTERACT_UPDATE:
        o_interaction = (VLCInteraction *)p_dialog->p_private;
        [o_interaction updateDialog];
        break;
    case INTERACT_HIDE:
        o_interaction = (VLCInteraction *)p_dialog->p_private;
        [o_interaction hideDialog];
        break;
    case INTERACT_DESTROY:
        o_interaction = (VLCInteraction *)p_dialog->p_private;
        [o_interaction destroyDialog];
        [self removeInteraction:o_interaction];
        p_dialog->i_status = DESTROYED_DIALOG;
        break;
    }
}

-(void)addInteraction: (interaction_dialog_t *)p_dialog
{

    VLCInteraction *o_interaction = [[VLCInteraction alloc] initDialog: p_dialog];
    
    p_dialog->p_private = (void *)o_interaction;
    [o_interaction_list addObject:[o_interaction autorelease]];
    [o_interaction runDialog];
}

-(void)removeInteraction: (VLCInteraction *)o_interaction
{
    [o_interaction_list removeObject:o_interaction];
}

-(void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [o_interaction_list removeAllObjects];
    [o_interaction_list release];
    [super dealloc];
}

@end

/*****************************************************************************
 * VLCInteraction implementation
 *****************************************************************************/
@implementation VLCInteraction

-(id)initDialog: (interaction_dialog_t *)_p_dialog
{
    p_intf = VLCIntf;
    [super init];
    p_dialog = _p_dialog;
    return self;
}

-(void)runDialog
{
    id o_window = NULL;
    if( !p_dialog )
        msg_Err( p_intf, "no available interaction framework" );

    if( !nib_interact_loaded )
    {
        nib_interact_loaded = [NSBundle loadNibNamed:@"Interaction" owner:self];
        [o_prog_cancel_btn setTitle: _NS("Cancel")];
        [o_prog_bar setUsesThreadedAnimation: YES];
        [o_auth_login_txt setStringValue: _NS("Login:")];
        [o_auth_pw_txt setStringValue: _NS("Password:")];
        [o_auth_cancel_btn setTitle: _NS("Cancel")];
        [o_auth_ok_btn setTitle: _NS("OK")];
        [o_input_ok_btn setTitle: _NS("OK")];
        [o_input_cancel_btn setTitle: _NS("Cancel")];
        o_mainIntfPgbar = [[VLCMain sharedInstance] getMainIntfPgbar];
    }

    NSString *o_title = [NSString stringWithUTF8String:p_dialog->psz_title ? p_dialog->psz_title : "title"];
    NSString *o_description = [NSString stringWithUTF8String:p_dialog->psz_description ? p_dialog->psz_description : ""];
    
    vout_thread_t *p_vout = vlc_object_find( VLCIntf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout != NULL )
    {
        NSEnumerator * o_enum = [[NSApp orderedWindows] objectEnumerator];

        while( ( o_window = [o_enum nextObject] ) )
        {
            if( [[o_window className] isEqualToString: @"VLCWindow"] )
            {
                vlc_object_release( (vlc_object_t *)p_vout );
                break;
            }
        }
        vlc_object_release( (vlc_object_t *)p_vout );
    }
    else
    {
        o_window = [NSApp mainWindow];
    }

    #if 0
    msg_Dbg( p_intf, "Title: %s", [o_title UTF8String] );
    msg_Dbg( p_intf, "Description: %s", [o_description UTF8String] );
    #endif

    if( p_dialog->i_id == DIALOG_ERRORS )
    {
        msg_Err( p_intf, "Error: %s", p_dialog->psz_description );
    }
    else
    {
        if( p_dialog->i_flags & DIALOG_OK_CANCEL )
        {
            msg_Dbg( p_intf, "OK-Cancel-dialog requested" );
            NSBeginInformationalAlertSheet( o_title, _NS("OK") , _NS("Cancel"),
                nil, o_window, self,
                @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil, 
                o_description );
        }
        else if( p_dialog->i_flags & DIALOG_YES_NO_CANCEL )
        {
            msg_Dbg( p_intf, "yes-no-cancel-dialog requested" );
            NSBeginInformationalAlertSheet( o_title, _NS("Yes"), _NS("No"),
                _NS("Cancel"), o_window, self,
                @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil, 
                o_description );
        }
        else if( p_dialog->i_flags & DIALOG_LOGIN_PW_OK_CANCEL )
        {
            msg_Dbg( p_intf, "dialog for login and pw requested" );
            [o_auth_title setStringValue: o_title];
            [o_auth_description setStringValue: o_description];
            [o_auth_login_fld setStringValue: @""];
            [o_auth_pw_fld setStringValue: @""];
            [NSApp beginSheet: o_auth_win modalForWindow: o_window
                modalDelegate: self didEndSelector: nil contextInfo: nil];
            [o_auth_win makeKeyWindow];
        }
        else if( p_dialog->i_flags & DIALOG_USER_PROGRESS )
        {
            msg_Dbg( p_intf, "user progress dialog requested" );
            [o_prog_title setStringValue: o_title];
            [o_prog_description setStringValue: o_description];
            [o_prog_bar setDoubleValue: (double)p_dialog->val.f_float];
            [NSApp beginSheet: o_prog_win modalForWindow: o_window
                modalDelegate: self didEndSelector: nil contextInfo: nil];
            [o_prog_win makeKeyWindow];
        }
        else if( p_dialog->i_flags & DIALOG_PSZ_INPUT_OK_CANCEL )
        {
            msg_Dbg( p_intf, "text input from user requested" );
            [o_input_title setStringValue: o_title];
            [o_input_description setStringValue: o_description];
            [o_input_fld setStringValue: @""];
            [NSApp beginSheet: o_input_win modalForWindow: o_window
                modalDelegate: self didEndSelector: nil contextInfo: nil];
            [o_input_win makeKeyWindow];
        }
        else if( p_dialog->i_flags & DIALOG_INTF_PROGRESS )
        {
            msg_Dbg( p_intf, "progress-bar in main intf requested" );
            [[VLCMain sharedInstance] setScrollField: o_description stopAfter: -1];
            [o_mainIntfPgbar setDoubleValue: (double)p_dialog->val.f_float];
            [o_mainIntfPgbar setHidden: NO];
            [[[VLCMain sharedInstance] getControllerWindow] makeKeyWindow];
            [o_mainIntfPgbar setIndeterminate: NO];
        }
        else
            msg_Err( p_intf, "requested dialog type unknown (%i)", 
                p_dialog->i_flags );
    }
}

- (void)sheetDidEnd:(NSWindow *)o_sheet returnCode:(int)i_return
    contextInfo:(void *)o_context
{
    vlc_mutex_lock( &p_dialog->p_interaction->object_lock );
    if( i_return == NSAlertDefaultReturn )
    {
        p_dialog->i_return = DIALOG_OK_YES;
    }
    else if( i_return == NSAlertAlternateReturn && ( p_dialog->i_flags & DIALOG_OK_CANCEL ) )
    {
        p_dialog->i_return = DIALOG_CANCELLED;
    }
    else if( i_return == NSAlertAlternateReturn )
    {
        p_dialog->i_return = DIALOG_NO;
    }
    else if( i_return == NSAlertOtherReturn )
    {
        p_dialog->i_return = DIALOG_CANCELLED;
    }
    p_dialog->i_status = ANSWERED_DIALOG;
    vlc_mutex_unlock( &p_dialog->p_interaction->object_lock );
}

-(void)updateDialog
{
    if( p_dialog->i_flags & DIALOG_USER_PROGRESS )
    {
        [o_prog_description setStringValue: \
            [NSString stringWithUTF8String: p_dialog->psz_description]];
        [o_prog_bar setDoubleValue: (double)p_dialog->val.f_float];

        if( [o_prog_bar doubleValue] == 100.0 )
        {
            /* we are done, let's hide */
            [self hideDialog];
        }
        return;
    }
    if( p_dialog->i_flags & DIALOG_INTF_PROGRESS )
    {
        [[VLCMain sharedInstance] setScrollField:
            [NSString stringWithUTF8String: p_dialog->psz_description]
            stopAfter: -1];
        [o_mainIntfPgbar setDoubleValue: (double)p_dialog->val.f_float];

        if( [o_mainIntfPgbar doubleValue] == 100.0 )
        {
            /* we are done, let's hide */
            [self hideDialog];
        }
        return;
    }
}

-(void)hideDialog
{
    msg_Dbg( p_intf, "hide event" );
    if( p_dialog->i_flags & DIALOG_USER_PROGRESS )
    {
        [NSApp endSheet: o_prog_win];
        [o_prog_win close];
    }
    if( p_dialog->i_flags & DIALOG_LOGIN_PW_OK_CANCEL )
    {
        [NSApp endSheet: o_auth_win];
        [o_auth_win close];
    }
    if( p_dialog->i_flags & DIALOG_PSZ_INPUT_OK_CANCEL )
    {
        [NSApp endSheet: o_input_win];
        [o_input_win close];
    }
    if( p_dialog->i_flags & DIALOG_INTF_PROGRESS )
    {
        [o_mainIntfPgbar setIndeterminate: YES];
        [o_mainIntfPgbar setHidden: YES];
        [[VLCMain sharedInstance] resetScrollField];
    }
}

-(void)destroyDialog
{
    msg_Dbg( p_intf, "destroy event" );
    if( o_mainIntfPgbar )
        [o_mainIntfPgbar release];
}

- (IBAction)cancelAndClose:(id)sender
{
    /* tell the core that the dialog was cancelled */
    vlc_mutex_lock( &p_dialog->p_interaction->object_lock );
    p_dialog->i_return = DIALOG_CANCELLED;
    p_dialog->i_status = ANSWERED_DIALOG;
    vlc_mutex_unlock( &p_dialog->p_interaction->object_lock );
    msg_Dbg( p_intf, "dialog cancelled" );
}

- (IBAction)okayAndClose:(id)sender
{
    vlc_mutex_lock( &p_dialog->p_interaction->object_lock );
    if( p_dialog->i_flags == DIALOG_LOGIN_PW_OK_CANCEL )
    {
        p_dialog->psz_returned[0] = strdup( [[o_auth_login_fld stringValue] UTF8String] );
        p_dialog->psz_returned[1] = strdup( [[o_auth_pw_fld stringValue] UTF8String] );
    }
    else if( p_dialog->i_flags == DIALOG_PSZ_INPUT_OK_CANCEL )
        p_dialog->psz_returned[0] = strdup( [[o_input_fld stringValue] UTF8String] );
    p_dialog->i_return = DIALOG_OK_YES;
    p_dialog->i_status = ANSWERED_DIALOG;
    vlc_mutex_unlock( &p_dialog->p_interaction->object_lock );
    msg_Dbg( p_intf, "dialog acknowledged" );
}

@end
