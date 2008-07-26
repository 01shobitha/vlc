/*****************************************************************************
* simple_prefs.m: Simple Preferences for Mac OS X
*****************************************************************************
* Copyright (C) 2008 the VideoLAN team
* $Id$
*
* Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "simple_prefs.h"
#import "prefs.h"
#import <vlc_keys.h>
#import <vlc_interface.h>
#import "misc.h"

static NSString* VLCSPrefsToolbarIdentifier = @"Our Simple Preferences Toolbar Identifier";
static NSString* VLCIntfSettingToolbarIdentifier = @"Intf Settings Item Identifier";
static NSString* VLCAudioSettingToolbarIdentifier = @"Audio Settings Item Identifier";
static NSString* VLCVideoSettingToolbarIdentifier = @"Video Settings Item Identifier";
static NSString* VLCOSDSettingToolbarIdentifier = @"Subtitles Settings Item Identifier";
static NSString* VLCInputSettingToolbarIdentifier = @"Input Settings Item Identifier";
static NSString* VLCHotkeysSettingToolbarIdentifier = @"Hotkeys Settings Item Identifier";

@implementation VLCSimplePrefs

static VLCSimplePrefs *_o_sharedInstance = nil;

+ (VLCSimplePrefs *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
        p_intf = VLCIntf;
    }

    return _o_sharedInstance;
}

- (void)dealloc
{
    [o_currentlyShownCategoryView release];
    [o_sprefs_toolbar release];

    [o_hotkeySettings release];
    [o_hotkeyDescriptions release];
    [o_hotkeysNonUseableKeys release];

    if( o_keyInTransition )
        [o_keyInTransition release];

    [super dealloc];
}


- (NSString *)OSXKeyToString:(int)val
{
    NSMutableString *o_temp_str = [[[NSMutableString alloc] init] autorelease];
    if( val & KEY_MODIFIER_CTRL )
        [o_temp_str appendString: [NSString stringWithUTF8String: "\xE2\x8C\x83"]];
    if( val & KEY_MODIFIER_ALT )
        [o_temp_str appendString: [NSString stringWithUTF8String: "\xE2\x8C\xA5"]];
    if( val & KEY_MODIFIER_SHIFT )
        [o_temp_str appendString: [NSString stringWithUTF8String: "\xE2\x87\xA7"]];
    if( val & KEY_MODIFIER_COMMAND )
        [o_temp_str appendString: [NSString stringWithUTF8String: "\xE2\x8C\x98"]];

    const char *base = KeyToString( val & ~KEY_MODIFIER );
    if( base )
        [o_temp_str appendString: [NSString stringWithUTF8String: base]];
    else
        o_temp_str = @"Unset";
    return o_temp_str;
}

- (void)awakeFromNib
{
    [self initStrings];
    
    /* setup the toolbar */
    o_sprefs_toolbar = [[[NSToolbar alloc] initWithIdentifier: VLCSPrefsToolbarIdentifier] autorelease];
    [o_sprefs_toolbar setAllowsUserCustomization: NO];
    [o_sprefs_toolbar setAutosavesConfiguration: NO];
    [o_sprefs_toolbar setDisplayMode: NSToolbarDisplayModeIconAndLabel];
    [o_sprefs_toolbar setSizeMode: NSToolbarSizeModeRegular];
    [o_sprefs_toolbar setDelegate: self];
    [o_sprefs_win setToolbar: o_sprefs_toolbar];
    
    /* setup useful stuff */
    o_hotkeysNonUseableKeys = [[NSArray arrayWithObjects:
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'c'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'x'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'v'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'a'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|','],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'h'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_ALT|'h'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'o'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'o'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'d'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'n'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'s'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'z'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'l'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'r'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'0'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'1'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'2'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'3'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'m'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'w'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'w'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'c'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'p'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'i'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'e'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'e'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'b'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'m'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_CTRL|'m'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|'?'],
                                [NSNumber numberWithInt: KEY_MODIFIER_COMMAND|KEY_MODIFIER_ALT|'?'],
                                nil] retain];
}

- (NSToolbarItem *) toolbar: (NSToolbar *)o_sprefs_toolbar 
      itemForItemIdentifier: (NSString *)o_itemIdent 
  willBeInsertedIntoToolbar: (BOOL)b_willBeInserted
{
    NSToolbarItem *o_toolbarItem = nil;
    
    #define CreateToolbarItem( o_name, o_desc, o_img, sel ) \
    o_toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: o_itemIdent] autorelease]; \
    \
    [o_toolbarItem setLabel: o_name]; \
    [o_toolbarItem setPaletteLabel: o_desc]; \
    \
    [o_toolbarItem setToolTip: o_desc]; \
    [o_toolbarItem setImage: [NSImage imageNamed: o_img]]; \
    \
    [o_toolbarItem setTarget: self]; \
    [o_toolbarItem setAction: @selector( sel )]; \
    \
    [o_toolbarItem setEnabled: YES]; \
    [o_toolbarItem setAutovalidates: YES]
    
    if( [o_itemIdent isEqual: VLCIntfSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Interface"), _NS("Interface Settings"), @"spref_cone_Interface_64", showInterfaceSettings );
    }
    else if( [o_itemIdent isEqual: VLCAudioSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Audio"), _NS("General Audio Settings"), @"spref_cone_Audio_64", showAudioSettings );
    }
    else if( [o_itemIdent isEqual: VLCVideoSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Video"), _NS("General Video Settings"), @"spref_cone_Video_64", showVideoSettings );
    }
    else if( [o_itemIdent isEqual: VLCOSDSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Subtitles & OSD"), _NS("Subtitles & On Screen Display Settings"), @"spref_cone_Subtitles_64", showOSDSettings );
    }
    else if( [o_itemIdent isEqual: VLCInputSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Input & Codecs"), _NS("Input & Codec settings"), @"spref_cone_Input_64", showInputSettings );
    }
    else if( [o_itemIdent isEqual: VLCHotkeysSettingToolbarIdentifier] )
    {
        CreateToolbarItem( _NS("Hotkeys"), _NS("Hotkeys settings"), @"spref_cone_Hotkeys_64", showHotkeySettings );
    }

    return o_toolbarItem;
}

- (NSArray *)toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier, 
        VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, VLCHotkeysSettingToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers: (NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier, 
        VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, VLCHotkeysSettingToolbarIdentifier, NSToolbarFlexibleSpaceItemIdentifier, nil];
}

- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects: VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier, 
        VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, VLCHotkeysSettingToolbarIdentifier, nil];
}

- (void)initStrings
{
    /* audio */
    [o_audio_dolby_txt setStringValue: _NS("Force detection of Dolby Surround")];
    [o_audio_effects_box setTitle: _NS("Effects")];
    [o_audio_enable_ckb setTitle: _NS("Enable Audio")];
    [o_audio_general_box setTitle: _NS("General Audio")];
    [o_audio_headphone_ckb setTitle: _NS("Headphone surround effect")];
    [o_audio_lang_txt setStringValue: _NS("Preferred Audio language")];
    [o_audio_last_ckb setTitle: _NS("Enable Last.fm submissions")];
    [o_audio_lastpwd_txt setStringValue: _NS("Password")];
    [o_audio_lastuser_txt setStringValue: _NS("User name")];
    [o_audio_norm_ckb setTitle: _NS("Volume normalizer")];
    [o_audio_spdif_ckb setTitle: _NS("Use S/PDIF when available")];
    [o_audio_visual_txt setStringValue: _NS("Visualization")];
    [o_audio_vol_txt setStringValue: _NS("Default Volume")];

    /* hotkeys */
    [o_hotkeys_change_btn setTitle: _NS("Change")];
    [o_hotkeys_change_win setTitle: _NS("Change Hotkey")];
    [o_hotkeys_change_cancel_btn setTitle: _NS("Cancel")];
    [o_hotkeys_change_ok_btn setTitle: _NS("OK")];
    [o_hotkeys_clear_btn setTitle: _NS("Clear")];
    [o_hotkeys_lbl setStringValue: _NS("Select an action to change the associated hotkey:")];
    [[[o_hotkeys_listbox tableColumnWithIdentifier: @"action"] headerCell] setStringValue: _NS("Action")];
    [[[o_hotkeys_listbox tableColumnWithIdentifier: @"shortcut"] headerCell] setStringValue: _NS("Shortcut")];

    /* input */
    [o_input_access_box setTitle: _NS("Access Filter")];
    [o_input_avi_txt setStringValue: _NS("Repair AVI Files")];
    [o_input_bandwidth_ckb setTitle: _NS("Bandwidth limiter")];
    [o_input_cachelevel_txt setStringValue: _NS("Default Caching Level")];
    [o_input_caching_box setTitle: _NS("Caching")];
    [o_input_cachelevel_custom_txt setStringValue: _NS("Use the complete preferences to configure custom caching values for each access module.")];
    [o_input_dump_ckb setTitle: _NS("Dump")];
    [o_input_httpproxy_txt setStringValue: _NS("HTTP Proxy")];
    [o_input_httpproxypwd_txt setStringValue: _NS("Password for HTTP Proxy")];
    [o_input_mux_box setTitle: _NS("Codecs / Muxers")];
    [o_input_net_box setTitle: _NS("Network")];
    [o_input_postproc_txt setStringValue: _NS("Post-Processing Quality")];
    [o_input_record_ckb setTitle: _NS("Record")];
    [o_input_rtsp_ckb setTitle: _NS("Use RTP over RTSP (TCP)")];
    [o_input_serverport_txt setStringValue: _NS("Default Server Port")];
    [o_input_timeshift_ckb setTitle: _NS("Timeshift")];

    /* interface */
    [o_intf_art_txt setStringValue: _NS("Album art download policy")];
    [o_intf_embedded_ckb setTitle: _NS("Add controls to the video window")];
    [o_intf_fspanel_ckb setTitle: _NS("Show Fullscreen Controller")];
    [o_intf_lang_txt setStringValue: _NS("Language")];
    [o_intf_network_box setTitle: _NS("Privacy / Network Interaction")];
    
    /* Subtitles and OSD */
    [o_osd_encoding_txt setStringValue: _NS("Default Encoding")];
    [o_osd_font_box setTitle: _NS("Display Settings")];
    [o_osd_font_btn setTitle: _NS("Browse...")];
    [o_osd_font_color_txt setStringValue: _NS("Font Color")];
    [o_osd_font_effect_txt setStringValue: _NS("Effect")];
    [o_osd_font_size_txt setStringValue: _NS("Font Size")];
    [o_osd_font_txt setStringValue: _NS("Font")];
    [o_osd_lang_box setTitle: _NS("Subtitle Languages")];
    [o_osd_lang_txt setStringValue: _NS("Preferred Subtitle Language")];
    [o_osd_osd_box setTitle: _NS("On Screen Display")];
    [o_osd_osd_ckb setTitle: _NS("Enable OSD")];

    /* video */
    [o_video_black_ckb setTitle: _NS("Black screens in Fullscreen mode")];
    [o_video_device_txt setStringValue: _NS("Fullscreen Video Device")];
    [o_video_display_box setTitle: _NS("Display")];
    [o_video_enable_ckb setTitle: _NS("Enable Video")];
    [o_video_fullscreen_ckb setTitle: _NS("Fullscreen")];
    [o_video_onTop_ckb setTitle: _NS("Always on top")];
    [o_video_output_txt setStringValue: _NS("Output module")];
    [o_video_skipFrames_ckb setTitle: _NS("Skip frames")];
    [o_video_snap_box setTitle: _NS("Video snapshots")];
    [o_video_snap_folder_btn setTitle: _NS("Browse...")];
    [o_video_snap_folder_txt setStringValue: _NS("Folder")];
    [o_video_snap_format_txt setStringValue: _NS("Format")];
    [o_video_snap_prefix_txt setStringValue: _NS("Prefix")];
    [o_video_snap_seqnum_ckb setTitle: _NS("Sequential numbering")];
    
    /* generic stuff */
    [[o_sprefs_basicFull_matrix cellAtRow: 0 column: 0] setStringValue: _NS("Basic")];
    [[o_sprefs_basicFull_matrix cellAtRow: 0 column: 1] setStringValue: _NS("All")];
    [o_sprefs_cancel_btn setTitle: _NS("Cancel")];
    [o_sprefs_reset_btn setTitle: _NS("Reset All")];
    [o_sprefs_save_btn setTitle: _NS("Save")];
    [o_sprefs_win setTitle: _NS("Preferences")];
}

- (void)resetControls
{
    module_config_t *p_item;
    vlc_list_t *p_list;
    module_t *p_parser;
    int i, y = 0;
    char *psz_tmp;

#define SetupIntList( object, name ) \
    [object removeAllItems]; \
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name ); \
    for( i = 0; i < p_item->i_list; i++ ) \
    { \
        if( p_item->ppsz_list_text[i] != NULL) \
            [object addItemWithTitle: _NS( p_item->ppsz_list_text[i] )]; \
        else \
            [object addItemWithTitle: [NSString stringWithUTF8String: p_item->ppsz_list[i] ?: ""]]; \
    } \
    if( p_item->value.i < [object numberOfItems] ) \
        [object selectItemAtIndex: p_item->value.i]; \
    else \
        [object selectItemAtIndex: 0]; \
    [object setToolTip: _NS( p_item->psz_longtext )]

#define SetupStringList( object, name ) \
    [object removeAllItems]; \
    y = 0; \
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name ); \
    for( i = 0; p_item->ppsz_list[i] != nil; i++ ) \
    { \
        [object addItemWithTitle: _NS( p_item->ppsz_list_text[i] )]; \
        if( p_item->value.psz && !strcmp( p_item->value.psz, p_item->ppsz_list[i] ) ) \
            y = i; \
    } \
    [object selectItemAtIndex: y]; \
    [object setToolTip: _NS( p_item->psz_longtext )]
    
#define SetupModuleList( object, name ) \
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name ); \
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE ); \
    [object removeAllItems]; \
    [object addItemWithTitle: _NS("Default")]; \
    for( int i_index = 0; i_index < p_list->i_count; i_index++ ) \
    { \
        p_parser = (module_t *)p_list->p_values[i_index].p_object ; \
        \
        if( module_IsCapable( p_parser, p_item->psz_type ) ) \
        { \
            [object addItemWithTitle: [NSString stringWithUTF8String: module_GetLongName( p_parser ) ?: ""]]; \
            \
            if( p_item->value.psz && !strcmp( p_item->value.psz, module_GetObjName( p_parser ) ) ) \
                [object selectItem: [object lastItem]]; \
        } \
    } \
    vlc_list_release( p_list ); \
    [object setToolTip: _NS(p_item->psz_longtext)]

    [[o_sprefs_basicFull_matrix cellAtRow:0 column:0] setState: NSOnState];
    [[o_sprefs_basicFull_matrix cellAtRow:0 column:1] setState: NSOffState];
    
    /**********************
     * interface settings *
     **********************/
    SetupStringList( o_intf_lang_pop, "language" );
    SetupIntList( o_intf_art_pop, "album-art" );

    [o_intf_fspanel_ckb setState: config_GetInt( p_intf, "macosx-fspanel" )];
    [o_intf_embedded_ckb setState: config_GetInt( p_intf, "embedded-video" )];

    /******************
     * audio settings *
     ******************/
    [o_audio_enable_ckb setState: config_GetInt( p_intf, "audio" )];
    [o_audio_vol_fld setIntValue: config_GetInt( p_intf, "volume" )];
    [o_audio_vol_sld setIntValue: config_GetInt( p_intf, "volume" )];

    [o_audio_spdif_ckb setState: config_GetInt( p_intf, "spdif" )];

    SetupIntList( o_audio_dolby_pop, "force-dolby-surround" );

    [o_audio_lang_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "audio-language" ) ?: ""]];

    [o_audio_headphone_ckb setState: config_GetInt( p_intf, "headphone-dolby" )];
    
    psz_tmp = config_GetPsz( p_intf, "audio-filter" );
    if( psz_tmp )
        [o_audio_norm_ckb setState: (int)strstr( psz_tmp, "normvol" )];
    [o_audio_norm_fld setFloatValue: config_GetFloat( p_intf, "norm-max-level" )];

    SetupModuleList( o_audio_visual_pop, "audio-visual" );

    /* Last.FM is optional */
    if( module_Exists( p_intf, "audioscrobbler" ) )
    {
        [o_audio_lastuser_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "lastfm-username" ) ?: ""]];
        [o_audio_lastpwd_sfld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "lastfm-password" ) ?: ""]];

        if( config_ExistIntf( VLC_OBJECT( p_intf ), "audioscrobbler" ) )
        {
            [o_audio_last_ckb setState: NSOnState];
            [o_audio_lastuser_fld setEnabled: YES];
            [o_audio_lastpwd_sfld setEnabled: YES];
        }
        else
        {
            [o_audio_last_ckb setState: NSOffState];
            [o_audio_lastuser_fld setEnabled: NO];
            [o_audio_lastpwd_sfld setEnabled: NO];
        }
    }
    else
        [o_audio_last_ckb setEnabled: NO];

    /******************
     * video settings *
     ******************/
    [o_video_enable_ckb setState: config_GetInt( p_intf, "video" )];
    [o_video_fullscreen_ckb setState: config_GetInt( p_intf, "fullscreen" )];
    [o_video_onTop_ckb setState: config_GetInt( p_intf, "video-on-top" )];
    [o_video_skipFrames_ckb setState: config_GetInt( p_intf, "skip-frames" )];
    [o_video_black_ckb setState: config_GetInt( p_intf, "macosx-black" )];

    SetupModuleList( o_video_output_pop, "vout" );

    [o_video_device_pop removeAllItems];
    i = 0;
    y = [[NSScreen screens] count];
    [o_video_device_pop addItemWithTitle: _NS("Default")];
    [[o_video_device_pop lastItem] setTag: 0];
    while( i < y )
    {
        NSRect s_rect = [[[NSScreen screens] objectAtIndex: i] frame];
        [o_video_device_pop addItemWithTitle: 
         [NSString stringWithFormat: @"%@ %i (%ix%i)", _NS("Screen"), i+1,
                   (int)s_rect.size.width, (int)s_rect.size.height]];
        [[o_video_device_pop lastItem] setTag: (int)[[[NSScreen screens] objectAtIndex: i] displayID]];
        i++;
    }
    [o_video_device_pop selectItemAtIndex: 0];
    [o_video_device_pop selectItemWithTag: config_GetInt( p_intf, "macosx-vdev" )];

    [o_video_snap_folder_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "snapshot-path" ) ?: ""]];
    [o_video_snap_prefix_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "snapshot-prefix" ) ?: ""]];
    [o_video_snap_seqnum_ckb setState: config_GetInt( p_intf, "snapshot-sequential" )];
    
    p_item = config_FindConfig( VLC_OBJECT(p_intf), "snapshot-format" );
    for( i = 0; p_item->ppsz_list[i] != nil; i++ )
    {
        [o_video_snap_format_pop addItemWithTitle: [NSString stringWithUTF8String: p_item->ppsz_list[i] ?: ""]];
        if( p_item->value.psz && !strcmp( p_item->value.psz, p_item->ppsz_list[i] ) )
            y = i;
    }
    [o_video_snap_format_pop selectItemAtIndex: y];

    /***************************
     * input & codecs settings *
     ***************************/
    [o_input_serverport_fld setIntValue: config_GetInt( p_intf, "server-port" )];
    if( config_GetPsz( p_intf, "http-proxy" ) != NULL )
        [o_input_httpproxy_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "http-proxy" ) ?: ""]];
    if( config_GetPsz( p_intf, "http-proxy" ) != NULL )
        [o_input_httpproxypwd_sfld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "http-proxy-pwd" ) ?: ""]];
    [o_input_postproc_fld setIntValue: config_GetInt( p_intf, "ffmpeg-pp-q" )];

    SetupIntList( o_input_avi_pop, "avi-index" );

    [o_input_rtsp_ckb setState: config_GetInt( p_intf, "rtsp-tcp" )];

    psz_tmp = config_GetPsz( p_intf, "access-filter" );
    if( psz_tmp )
    {
        [o_input_record_ckb setState: (int)strstr( psz_tmp, "record" )];
        [o_input_dump_ckb setState: (int)strstr( psz_tmp, "dump" )];
        [o_input_bandwidth_ckb setState: (int)strstr( psz_tmp, "bandwidth" )];
        [o_input_timeshift_ckb setState: (int)strstr( psz_tmp, "timeshift" )];
    }

    [o_input_cachelevel_pop removeAllItems];
    [o_input_cachelevel_pop addItemsWithTitles: 
        [NSArray arrayWithObjects: _NS("Custom"), _NS("Lowest latency"), _NS("Low latency"), _NS("Normal"),
            _NS("High latency"), _NS("Higher latency"), nil]];
    [[o_input_cachelevel_pop itemAtIndex: 0] setTag: 0];
    [[o_input_cachelevel_pop itemAtIndex: 1] setTag: 100];
    [[o_input_cachelevel_pop itemAtIndex: 2] setTag: 200];
    [[o_input_cachelevel_pop itemAtIndex: 3] setTag: 300];
    [[o_input_cachelevel_pop itemAtIndex: 4] setTag: 400];
    [[o_input_cachelevel_pop itemAtIndex: 5] setTag: 500];
    
#define TestCaC( name ) \
    b_cache_equal =  b_cache_equal && \
        ( i_cache == config_GetInt( p_intf, name ) )

#define TestCaCi( name, int ) \
        b_cache_equal = b_cache_equal &&  \
        ( ( i_cache * int ) == config_GetInt( p_intf, name ) )

    /* Select the accurate value of the PopupButton */
    bool b_cache_equal = true;
    int i_cache = config_GetInt( p_intf, "file-caching");
    
    TestCaC( "udp-caching" );
    if( module_Exists (p_intf, "dvdread") )
        TestCaC( "dvdread-caching" );
    if( module_Exists (p_intf, "dvdnav") )
        TestCaC( "dvdnav-caching" );
    TestCaC( "tcp-caching" );
    TestCaC( "fake-caching" );
    TestCaC( "cdda-caching" );
    TestCaC( "screen-caching" );
    TestCaC( "vcd-caching" );
    TestCaCi( "rtsp-caching", 4 );
    TestCaCi( "ftp-caching", 2 );
    TestCaCi( "http-caching", 4 );
    if(module_Exists (p_intf, "access_realrtsp"))
        TestCaCi( "realrtsp-caching", 10 );
    TestCaCi( "mms-caching", 19 );
    if( b_cache_equal )
    {
        [o_input_cachelevel_pop selectItemWithTag: i_cache];
        [o_input_cachelevel_custom_txt setHidden: YES];
    }
    else
    {
        [o_input_cachelevel_pop selectItemWithTitle: _NS("Custom")];
        [o_input_cachelevel_custom_txt setHidden: NO];
    }

    /*********************
     * subtitle settings *
     *********************/
    [o_osd_osd_ckb setState: config_GetInt( p_intf, "osd" )];
    
    [o_osd_encoding_pop removeAllItems];
    y = 0;
    p_item = config_FindConfig( VLC_OBJECT(p_intf), "subsdec-encoding" );
    for( i = 0; p_item->ppsz_list[i] != nil; i++ )
    {
        if( p_item->ppsz_list[i] != "" )
            [o_osd_encoding_pop addItemWithTitle: _NS( p_item->ppsz_list[i] )];
        else
            [o_osd_encoding_pop addItemWithTitle: @" "];

        if( p_item->value.psz && !strcmp( p_item->value.psz, p_item->ppsz_list[i] ) )
            y = i;
    }
    [o_osd_encoding_pop selectItemAtIndex: y];
    
    [o_osd_lang_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "sub-language" ) ?: ""]];
    if( config_GetPsz( p_intf, "quartztext-font" ) != NULL )
        [o_osd_font_fld setStringValue: [NSString stringWithUTF8String: config_GetPsz( p_intf, "quartztext-font" ) ?: ""]];

    SetupIntList( o_osd_font_color_pop, "quartztext-color" );
    SetupIntList( o_osd_font_size_pop, "quartztext-rel-fontsize" );
    //SetupIntList( o_osd_font_effect_pop, "quartztext-effect" );

    /********************
     * hotkeys settings *
     ********************/
    struct hotkey *p_hotkeys = p_intf->p_libvlc->p_hotkeys;
    o_hotkeySettings = [[NSMutableArray alloc] init];
    NSMutableArray *o_tempArray_desc = [[NSMutableArray alloc] init];
    i = 1;

    while( i < 100 )
    {
        p_item = config_FindConfig( VLC_OBJECT(p_intf), p_hotkeys[i].psz_action );
        if( !p_item )
            break;

        [o_tempArray_desc addObject: _NS( p_item->psz_text )];
        [o_hotkeySettings addObject: [NSNumber numberWithInt: p_item->value.i]];

        i++;
    }
    o_hotkeyDescriptions = [[NSArray alloc] initWithArray: o_tempArray_desc copyItems: YES];
    [o_tempArray_desc release];
    [o_hotkeys_listbox reloadData];
}

- (void)showSimplePrefs
{
    /* we want to show the interface settings, if no category was chosen */
    if( [o_sprefs_toolbar selectedItemIdentifier] == nil )
    {
        [o_sprefs_toolbar setSelectedItemIdentifier: VLCIntfSettingToolbarIdentifier];
        [self showInterfaceSettings];
    }
    
    [self resetControls];

    [o_sprefs_win center];
    [o_sprefs_win makeKeyAndOrderFront: self];
}

- (IBAction)buttonAction:(id)sender
{
    if( sender == o_sprefs_cancel_btn )
        [o_sprefs_win orderOut: sender];
    else if( sender == o_sprefs_save_btn )
    {
        [self saveChangedSettings];
        [o_sprefs_win orderOut: sender];
    }
    else if( sender == o_sprefs_reset_btn )
        NSBeginInformationalAlertSheet( _NS("Reset Preferences"), _NS("Cancel"),
                                        _NS("Continue"), nil, o_sprefs_win, self,
                                        @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil,
                                        _NS("Beware this will reset the VLC media player preferences.\n"
                                            "Are you sure you want to continue?") );
    else if( sender == o_sprefs_basicFull_matrix )
    {
        [o_sprefs_win orderOut: self];
        [[o_sprefs_basicFull_matrix cellAtRow:0 column:0] setState: NSOffState];
        [[o_sprefs_basicFull_matrix cellAtRow:0 column:1] setState: NSOnState];
        [[[VLCMain sharedInstance] getPreferences] showPrefs];
    }
    else
        msg_Warn( p_intf, "unknown buttonAction sender" );
}

- (void)sheetDidEnd:(NSWindow *)o_sheet 
         returnCode:(int)i_return
        contextInfo:(void *)o_context
{
    if( i_return == NSAlertAlternateReturn )
    {
        config_ResetAll( p_intf );
        b_intfSettingChanged = b_videoSettingChanged = b_audioSettingChanged = YES;
        [self resetControls];
    }
}

- (void)saveChangedSettings
{
    module_config_t *p_item;
    vlc_list_t *p_list;
    module_t *p_parser;
    char *psz_tmp;
    int i;
    
#define SaveIntList( object, name ) \
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name ); \
    if( [object indexOfSelectedItem] >= 0 ) \
        config_PutInt( p_intf, name, p_item->pi_list[[object indexOfSelectedItem]] ); \
    else \
        config_PutInt( p_intf, name, [object intValue] ) \
                    
#define SaveStringList( object, name ) \
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name ); \
    if( [object indexOfSelectedItem] >= 0 ) \
        config_PutPsz( p_intf, name, strdup( p_item->ppsz_list[[object indexOfSelectedItem]] ) ); \
    else \
        config_PutPsz( p_intf, name, strdup( [[VLCMain sharedInstance] delocalizeString: [object stringValue]] ) )

#define SaveModuleList( object, name ) \
    p_item = config_FindConfig( VLC_OBJECT(p_intf), name ); \
    \
    p_list = vlc_list_find( VLCIntf, VLC_OBJECT_MODULE, FIND_ANYWHERE ); \
    for( int i_module_index = 0; i_module_index < p_list->i_count; i_module_index++ ) \
    { \
        p_parser = (module_t *)p_list->p_values[i_module_index].p_object; \
        \
        if( p_item->i_type == CONFIG_ITEM_MODULE && module_IsCapable( p_parser, p_item->psz_type ) ) \
        { \
            if( [[[object selectedItem] title] isEqualToString: _NS( module_GetLongName( p_parser ) )] ) \
            { \
                config_PutPsz( p_intf, name, strdup( module_GetObjName( p_parser ))); \
                break; \
            } \
        } \
    } \
    vlc_list_release( p_list ); \
    if( [[[object selectedItem] title] isEqualToString: _NS( "Default" )] ) \
        config_PutPsz( p_intf, name, "" )

    /**********************
     * interface settings *
     **********************/
    if( b_intfSettingChanged )
    {
        SaveStringList( o_intf_lang_pop, "language" );
        SaveIntList( o_intf_art_pop, "album-art" );

        config_PutInt( p_intf, "macosx-fspanel", [o_intf_fspanel_ckb state] );
        config_PutInt( p_intf, "embedded-video", [o_intf_embedded_ckb state] );

        /* okay, let's save our changes to vlcrc */
        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "macosx" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the Interface settings using SimplePrefs (%i)", i );
            intf_UserFatal( p_intf, false, _("Interface Settings not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            i = 0;
        }

        b_intfSettingChanged = NO;
    }
    
    /******************
     * audio settings *
     ******************/
    if( b_audioSettingChanged )
    {
        config_PutInt( p_intf, "audio", [o_audio_enable_ckb state] );
        config_PutInt( p_intf, "volume", [o_audio_vol_sld intValue] );
        config_PutInt( p_intf, "spdif", [o_audio_spdif_ckb state] );

        SaveIntList( o_audio_dolby_pop, "force-dolby-surround" );

        config_PutPsz( p_intf, "audio-language", [[o_audio_lang_fld stringValue] UTF8String] );
        config_PutInt( p_intf, "headphone-dolby", [o_audio_headphone_ckb state] );

        psz_tmp = config_GetPsz( p_intf, "audio-filter" );
        if(! psz_tmp)
            config_PutPsz( p_intf, "audio-filter", "volnorm" );
        else if( (int)strstr( psz_tmp, "normvol" ) == NO )
        {
            /* work-around a GCC 4.0.1 bug */
            psz_tmp = (char *)[[NSString stringWithFormat: @"%s:volnorm", psz_tmp] UTF8String];
            config_PutPsz( p_intf, "audio-filter", psz_tmp );
        }
        else
        {
            psz_tmp = (char *)[[[NSString stringWithUTF8String: psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@":volnorm"]] UTF8String];
            psz_tmp = (char *)[[[NSString stringWithUTF8String: psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@"volnorm:"]] UTF8String];
            psz_tmp = (char *)[[[NSString stringWithUTF8String: psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@"volnorm"]] UTF8String];
            config_PutPsz( p_intf, "audio-filter", psz_tmp );
        }
        config_PutFloat( p_intf, "norm-max-level", [o_audio_norm_fld floatValue] );

        SaveModuleList( o_audio_visual_pop, "audio-visual" );

        /* Last.FM is optional */
        if( module_Exists( p_intf, "audioscrobbler" ) )
        {   
            [o_audio_last_ckb setEnabled: YES];
            if( [o_audio_last_ckb state] == NSOnState )
                config_AddIntf( p_intf, "audioscrobbler" );
            else
                config_RemoveIntf( p_intf, "audioscrobbler" );

            config_PutPsz( p_intf, "lastfm-username", [[o_audio_lastuser_fld stringValue] UTF8String] );
            config_PutPsz( p_intf, "lastfm-password", [[o_audio_lastpwd_sfld stringValue] UTF8String] );
        }
        else
            [o_audio_last_ckb setEnabled: NO];

        /* okay, let's save our changes to vlcrc */
        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "audioscrobbler" );
        i = i + config_SaveConfigFile( p_intf, "volnorm" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the Audio settings using SimplePrefs (%i)", i );
            intf_UserFatal( p_intf, false, _("Audio Settings not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            
            i = 0;
        }
        b_audioSettingChanged = NO;
    }
    
    /******************
     * video settings *
     ******************/
    if( b_videoSettingChanged )
    {
        config_PutInt( p_intf, "video", [o_video_enable_ckb state] );
        config_PutInt( p_intf, "fullscreen", [o_video_fullscreen_ckb state] );
        config_PutInt( p_intf, "video-on-top", [o_video_onTop_ckb state] );
        config_PutInt( p_intf, "skip-frames", [o_video_skipFrames_ckb state] );
        config_PutInt( p_intf, "macosx-black", [o_video_black_ckb state] );

        SaveModuleList( o_video_output_pop, "vout" );
        config_PutInt( p_intf, "macosx-vdev", [[o_video_device_pop selectedItem] tag] );

        config_PutPsz( p_intf, "snapshot-path", [[o_video_snap_folder_fld stringValue] UTF8String] );
        config_PutPsz( p_intf, "snapshot-prefix", [[o_video_snap_prefix_fld stringValue] UTF8String] );
        config_PutInt( p_intf, "snapshot-sequential", [o_video_snap_seqnum_ckb state] );

        if( [o_video_snap_format_pop indexOfSelectedItem] >= 0 )
            config_PutPsz( p_intf, "snapshot-format", [[[o_video_snap_format_pop selectedItem] title] UTF8String] );

        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "macosx" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the Video settings using SimplePrefs (%i)", i );
            intf_UserFatal( p_intf, false, _("Video Settings not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            i = 0;
        }
        b_videoSettingChanged = NO;
    }
    
    /***************************
     * input & codecs settings *
     ***************************/
    if( b_inputSettingChanged )
    {
        config_PutInt( p_intf, "server-port", [o_input_serverport_fld intValue] );
        config_PutPsz( p_intf, "http-proxy", [[o_input_httpproxy_fld stringValue] UTF8String] );
        config_PutPsz( p_intf, "http-proxy-pwd", [[o_input_httpproxypwd_sfld stringValue] UTF8String] );
        config_PutInt( p_intf, "ffmpeg-pp-q", [o_input_postproc_fld intValue] );

        SaveIntList( o_input_avi_pop, "avi-index" );

        config_PutInt( p_intf, "rtsp-tcp", [o_input_rtsp_ckb state] );

        #define CaCi( name, int ) config_PutInt( p_intf, name, int * [[o_input_cachelevel_pop selectedItem] tag] )
        #define CaC( name ) CaCi( name, 1 )
        msg_Dbg( p_intf, "Adjusting all cache values to: %i", [[o_input_cachelevel_pop selectedItem] tag] );
        CaC( "udp-caching" );
        if( module_Exists (p_intf, "dvdread" ) )
        {
            CaC( "dvdread-caching" );
            i = i + config_SaveConfigFile( p_intf, "dvdread" );
        }
        if( module_Exists (p_intf, "dvdnav" ) )
        {
            CaC( "dvdnav-caching" );
            i = i + config_SaveConfigFile( p_intf, "dvdnav" );
        }
        CaC( "tcp-caching" ); CaC( "vcd-caching" );
        CaC( "fake-caching" ); CaC( "cdda-caching" ); CaC( "file-caching" );
        CaC( "screen-caching" );
        CaCi( "rtsp-caching", 4 ); CaCi( "ftp-caching", 2 );
        CaCi( "http-caching", 4 );
        if( module_Exists (p_intf, "access_realrtsp" ) )
        {
            CaCi( "realrtsp-caching", 10 );
            i = i + config_SaveConfigFile( p_intf, "access_realrtsp" );
        }
        CaCi( "mms-caching", 19 );

        #define SaveAccessFilter( object, name ) \
        if( [object state] == NSOnState ) \
        { \
            if( b_first ) \
            { \
                [o_temp appendString: name]; \
                b_first = NO; \
            } \
            else \
                [o_temp appendFormat: @":%@", name]; \
        }

        BOOL b_first = YES;
        NSMutableString *o_temp = [[NSMutableString alloc] init];
        SaveAccessFilter( o_input_record_ckb, @"record" );
        SaveAccessFilter( o_input_dump_ckb, @"dump" );
        SaveAccessFilter( o_input_bandwidth_ckb, @"bandwidth" );
        SaveAccessFilter( o_input_timeshift_ckb, @"timeshift" );
        config_PutPsz( p_intf, "access-filter", [o_temp UTF8String] );
        [o_temp release];

        i = config_SaveConfigFile( p_intf, "main" );
        i = i + config_SaveConfigFile( p_intf, "ffmpeg" );
        i = i + config_SaveConfigFile( p_intf, "access_http" );
        i = i + config_SaveConfigFile( p_intf, "access_file" );
        i = i + config_SaveConfigFile( p_intf, "access_tcp" );
        i = i + config_SaveConfigFile( p_intf, "access_fake" );
        i = i + config_SaveConfigFile( p_intf, "cdda" );
        i = i + config_SaveConfigFile( p_intf, "screen" );
        i = i + config_SaveConfigFile( p_intf, "vcd" );
        i = i + config_SaveConfigFile( p_intf, "access_ftp" );
        i = i + config_SaveConfigFile( p_intf, "access_mms" );
        i = i + config_SaveConfigFile( p_intf, "live555" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the Input settings using SimplePrefs (%i)", i );
            intf_UserFatal( p_intf, false, _("Input Settings not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            i = 0;
        }
        b_inputSettingChanged = NO;
    }
    
    /**********************
     * subtitles settings *
     **********************/
    if( b_osdSettingChanged )
    {
        config_PutInt( p_intf, "osd", [o_osd_osd_ckb state] );

        if( [o_osd_encoding_pop indexOfSelectedItem] >= 0 )
            config_PutPsz( p_intf, "subsdec-encoding", [[[o_osd_encoding_pop selectedItem] title] UTF8String] );

        config_PutPsz( p_intf, "sub-language", [[o_osd_lang_fld stringValue] UTF8String] );
        config_PutPsz( p_intf, "quartztext-font", [[o_osd_font_fld stringValue] UTF8String] );

        SaveIntList( o_osd_font_color_pop, "quartztext-color" );
        SaveIntList( o_osd_font_size_pop, "quartztext-rel-fontsize" );
        //SaveIntList( o_osd_font_effect_pop, "freetype-effect" );

        i = config_SaveConfigFile( p_intf, NULL );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the OSD/Subtitle settings using SimplePrefs (%i)", i );
            intf_UserFatal( p_intf, false, _("On Screen Display/Subtitle Settings not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            i = 0;
        }
        b_osdSettingChanged = NO;
    }
    
    /********************
     * hotkeys settings *
     ********************/
    if( b_hotkeyChanged )
    {
        struct hotkey *p_hotkeys = p_intf->p_libvlc->p_hotkeys;
        i = 1;
        while( i < [o_hotkeySettings count] )
        {
            config_PutInt( p_intf, p_hotkeys[i].psz_action, [[o_hotkeySettings objectAtIndex: i-1] intValue] );
            i++;
        }        

        i = config_SaveConfigFile( p_intf, "main" );

        if( i != 0 )
        {
            msg_Err( p_intf, "An error occurred while saving the Hotkey settings using SimplePrefs (%i)", i );
            intf_UserFatal( p_intf, false, _("Hotkeys not saved"),
                        _("An error occured while saving your settings via SimplePrefs (%i)."), i );
            i = 0;
        }
        b_hotkeyChanged = NO;
    }
}

- (void)showSettingsForCategory: (id)o_new_category_view
{
    NSRect o_win_rect, o_view_rect, o_old_view_rect;
    o_win_rect = [o_sprefs_win frame];
    o_view_rect = [o_new_category_view frame];
    
    if( o_currentlyShownCategoryView != nil )
    {
        /* restore our window's height, if we've shown another category previously */
        o_old_view_rect = [o_currentlyShownCategoryView frame];
        o_win_rect.size.height = o_win_rect.size.height - o_old_view_rect.size.height;
        o_win_rect.origin.y = ( o_win_rect.origin.y + o_old_view_rect.size.height ) - o_view_rect.size.height;

        /* remove our previous category view */
        [o_currentlyShownCategoryView removeFromSuperviewWithoutNeedingDisplay];
    }
    
    o_win_rect.size.height = o_win_rect.size.height + o_view_rect.size.height;
    
    [o_sprefs_win displayIfNeeded];
    [o_sprefs_win setFrame: o_win_rect display:YES animate: YES];
    
    [o_new_category_view setFrame: NSMakeRect( 0, 
                                               [o_sprefs_controls_box frame].size.height, 
                                               o_view_rect.size.width, 
                                               o_view_rect.size.height )];
    [o_new_category_view setNeedsDisplay: YES];
    [o_new_category_view setAutoresizesSubviews: YES];
    [[o_sprefs_win contentView] addSubview: o_new_category_view];
    
    /* keep our current category for further reference */
    [o_currentlyShownCategoryView release];
    o_currentlyShownCategoryView = o_new_category_view;
    [o_currentlyShownCategoryView retain];
}

- (IBAction)interfaceSettingChanged:(id)sender
{
    b_intfSettingChanged = YES;
}

- (void)showInterfaceSettings
{
    [self showSettingsForCategory: o_intf_view];
}

- (IBAction)audioSettingChanged:(id)sender
{
    if( sender == o_audio_vol_sld )
        [o_audio_vol_fld setIntValue: [o_audio_vol_sld intValue]];

    if( sender == o_audio_vol_fld )
        [o_audio_vol_sld setIntValue: [o_audio_vol_fld intValue]];

    if( sender == o_audio_last_ckb )
    {
        if( [o_audio_last_ckb state] == NSOnState )
        {
            [o_audio_lastpwd_sfld setEnabled: YES];
            [o_audio_lastuser_fld setEnabled: YES];
        }
        else
        {
            [o_audio_lastpwd_sfld setEnabled: NO];
            [o_audio_lastuser_fld setEnabled: NO];
        }
    }

    b_audioSettingChanged = YES;
}

- (void)showAudioSettings
{
    [self showSettingsForCategory: o_audio_view];
}

- (IBAction)videoSettingChanged:(id)sender
{
    if( sender == o_video_snap_folder_btn )
    {
        o_selectFolderPanel = [[NSOpenPanel alloc] init];
        [o_selectFolderPanel setCanChooseDirectories: YES];
        [o_selectFolderPanel setCanChooseFiles: NO];
        [o_selectFolderPanel setResolvesAliases: YES];
        [o_selectFolderPanel setAllowsMultipleSelection: NO];
        [o_selectFolderPanel setMessage: _NS("Choose the folder to save your video snapshots to.")];
        [o_selectFolderPanel setCanCreateDirectories: YES];
        [o_selectFolderPanel setPrompt: _NS("Choose")];
        [o_selectFolderPanel beginSheetForDirectory: nil file: nil modalForWindow: o_sprefs_win 
                                      modalDelegate: self 
                                     didEndSelector: @selector(savePanelDidEnd:returnCode:contextInfo:)
                                        contextInfo: o_video_snap_folder_btn];
    }
    else
        b_videoSettingChanged = YES;
}

- (void)savePanelDidEnd:(NSOpenPanel * )panel returnCode: (int)returnCode contextInfo: (void *)contextInfo
{
    if( returnCode == NSOKButton )
    {
        if( contextInfo == o_video_snap_folder_btn )
        {
            [o_video_snap_folder_fld setStringValue: [o_selectFolderPanel filename]];
            b_videoSettingChanged = YES;
        }
        else if( contextInfo == o_osd_font_btn )
        {
            [o_osd_font_fld setStringValue: [o_selectFolderPanel filename]];
            b_osdSettingChanged = YES;
        }
    }

    [o_selectFolderPanel release];
}

- (void)showVideoSettings
{
    [self showSettingsForCategory: o_video_view];
}

- (IBAction)osdSettingChanged:(id)sender
{
    if( sender == o_osd_font_btn )
    {
        o_selectFolderPanel = [[NSOpenPanel alloc] init];
        [o_selectFolderPanel setCanChooseDirectories: NO];
        [o_selectFolderPanel setCanChooseFiles: YES];
        [o_selectFolderPanel setResolvesAliases: YES];
        [o_selectFolderPanel setAllowsMultipleSelection: NO];
        [o_selectFolderPanel setMessage: _NS("Choose the font to display your Subtitles with.")];
        [o_selectFolderPanel setCanCreateDirectories: NO];
        [o_selectFolderPanel setPrompt: _NS("Choose")];
        [o_selectFolderPanel setAllowedFileTypes: [NSArray arrayWithObjects: @"dfont", @"ttf", @"otf", @"FFIL", nil]];
        [o_selectFolderPanel beginSheetForDirectory: @"/System/Library/Fonts/" file: nil modalForWindow: o_sprefs_win 
                                      modalDelegate: self 
                                     didEndSelector: @selector(savePanelDidEnd:returnCode:contextInfo:)
                                        contextInfo: o_osd_font_btn];
    }
    else
        b_osdSettingChanged = YES;
}

- (void)showOSDSettings
{
    [self showSettingsForCategory: o_osd_view];
}

- (IBAction)inputSettingChanged:(id)sender
{
    if( sender == o_input_cachelevel_pop )
    {
        if( [[[o_input_cachelevel_pop selectedItem] title] isEqualToString: _NS("Custom")] )
            [o_input_cachelevel_custom_txt setHidden: NO];
        else
            [o_input_cachelevel_custom_txt setHidden: YES];
    }

    b_inputSettingChanged = YES;
}

- (void)showInputSettings
{
    [self showSettingsForCategory: o_input_view];
}

- (IBAction)hotkeySettingChanged:(id)sender
{
    if( sender == o_hotkeys_change_btn || sender == o_hotkeys_listbox )
    {
        [o_hotkeys_change_lbl setStringValue: [NSString stringWithFormat: _NS("Press new keys for\n\"%@\""), 
                                               [o_hotkeyDescriptions objectAtIndex: [o_hotkeys_listbox selectedRow]]]];
        [o_hotkeys_change_keys_lbl setStringValue: [self OSXKeyToString:[[o_hotkeySettings objectAtIndex: [o_hotkeys_listbox selectedRow]] intValue]]];
        [o_hotkeys_change_taken_lbl setStringValue: @""];
        [o_hotkeys_change_win setInitialFirstResponder: [o_hotkeys_change_win contentView]];
        [o_hotkeys_change_win makeFirstResponder: [o_hotkeys_change_win contentView]];
        [NSApp runModalForWindow: o_hotkeys_change_win];
    }
    else if( sender == o_hotkeys_change_cancel_btn )
    {
        [NSApp stopModal];
        [o_hotkeys_change_win close];
    }
    else if( sender == o_hotkeys_change_ok_btn )
    {
        int i_returnValue;
        if(! o_keyInTransition )
        {
            [NSApp stopModal];
            [o_hotkeys_change_win close];
            msg_Err( p_intf, "internal error prevented the hotkey switch" );
            return;
        }

        b_hotkeyChanged = YES;

        i_returnValue = [o_hotkeySettings indexOfObject: o_keyInTransition];
        if( i_returnValue != NSNotFound )
            [o_hotkeySettings replaceObjectAtIndex: i_returnValue withObject: [[NSNumber numberWithInt: 0] retain]];

        [o_hotkeySettings replaceObjectAtIndex: [o_hotkeys_listbox selectedRow] withObject: [o_keyInTransition retain]];

        [NSApp stopModal];
        [o_hotkeys_change_win close];

        [o_hotkeys_listbox reloadData];
    }
    else if( sender == o_hotkeys_clear_btn )
    {
        [o_hotkeySettings replaceObjectAtIndex: [o_hotkeys_listbox selectedRow] withObject: [NSNumber numberWithInt: 0]];
        [o_hotkeys_listbox reloadData];
        b_hotkeyChanged = YES;
    }
}

- (void)showHotkeySettings
{
    [self showSettingsForCategory: o_hotkeys_view];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [o_hotkeySettings count];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    if( [[aTableColumn identifier] isEqualToString: @"action"] )
        return [o_hotkeyDescriptions objectAtIndex: rowIndex];
    else if( [[aTableColumn identifier] isEqualToString: @"shortcut"] )
        return [self OSXKeyToString: [[o_hotkeySettings objectAtIndex: rowIndex] intValue]];
    else
    {
        msg_Err( p_intf, "unknown TableColumn identifier (%s)!", [[aTableColumn identifier] UTF8String] );
        return NULL;
    }
}

- (void)changeHotkeyTo: (int)i_theNewKey
{
    int i_returnValue;
    i_returnValue = [o_hotkeysNonUseableKeys indexOfObject: [NSNumber numberWithInt: i_theNewKey]];
    if( i_returnValue != NSNotFound || i_theNewKey == 0 )
    {
        [o_hotkeys_change_keys_lbl setStringValue: _NS("Invalid combination")];
        [o_hotkeys_change_taken_lbl setStringValue: _NS("Regrettably, these keys cannot be assigned as hotkey shortcuts.")];
        [o_hotkeys_change_ok_btn setEnabled: NO];
    }
    else
    {
        NSString *o_temp;
        if( o_keyInTransition )
            [o_keyInTransition release];
        o_keyInTransition = [[NSNumber numberWithInt: i_theNewKey] retain];

        o_temp = [self OSXKeyToString: i_theNewKey];

        [o_hotkeys_change_keys_lbl setStringValue: o_temp];

        i_returnValue = [o_hotkeySettings indexOfObject: o_keyInTransition];
        if( i_returnValue != NSNotFound )
            [o_hotkeys_change_taken_lbl setStringValue: [NSString stringWithFormat:
                                                         _NS("This combination is already taken by \"%@\"."),
                                                         [o_hotkeyDescriptions objectAtIndex: i_returnValue]]];
        else
            [o_hotkeys_change_taken_lbl setStringValue: @""];

        [o_hotkeys_change_ok_btn setEnabled: YES];
    }
}
    
@end

/********************
 * hotkeys settings *
 ********************/

@implementation VLCHotkeyChangeWindow

- (BOOL)resignFirstResponder
{
    /* We need to stay the first responder or we'll miss the user's input */
    return NO;
}

- (void)keyDown:(NSEvent *)o_theEvent
{
    int i_nonReadableKey = 0;

    if( [o_theEvent modifierFlags] & NSControlKeyMask )
        i_nonReadableKey = i_nonReadableKey | KEY_MODIFIER_CTRL;

    if( [o_theEvent modifierFlags] & NSAlternateKeyMask  )
        i_nonReadableKey = i_nonReadableKey | KEY_MODIFIER_ALT;

    if( [o_theEvent modifierFlags] & NSShiftKeyMask )
        i_nonReadableKey = i_nonReadableKey | KEY_MODIFIER_SHIFT;

    if( [o_theEvent modifierFlags] & NSCommandKeyMask )
        i_nonReadableKey = i_nonReadableKey | KEY_MODIFIER_COMMAND;

    if( [o_theEvent modifierFlags] & NSFunctionKeyMask  )
    {
        unichar key = 0;
        key = [[o_theEvent charactersIgnoringModifiers] characterAtIndex: 0];
        
        switch( key )
        {
            case 0x1b:
                i_nonReadableKey = i_nonReadableKey | KEY_ESC;
                break;
            case NSF1FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F1;
                break;
            case NSF2FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F2;
                break;
            case NSF3FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F3;
                break;
            case NSF4FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F4;
                break;
            case NSF5FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F5;
                break;
            case NSF6FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F6;
                break;
            case NSF7FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F7;
                break;
            case NSF8FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F8;
                break;
            case NSF9FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F9;
                break;
            case NSF10FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F10;
                break;
            case NSF11FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F11;
                break;
            case NSF12FunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_F12;
                break;
            case NSInsertFunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_INSERT;
                break;
            case NSHomeFunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_HOME;
                break;
            case NSEndFunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_END;
                break;
            case NSPageUpFunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_PAGEUP;
                break;
            case NSPageDownFunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_PAGEDOWN;
                break;
            case NSMenuFunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_MENU;
                break;
            case NSTabCharacter:
                i_nonReadableKey = i_nonReadableKey | KEY_TAB;
                break;
            case NSDeleteCharacter:
                i_nonReadableKey = i_nonReadableKey | KEY_DELETE;
                break;
            case NSBackspaceCharacter:
                i_nonReadableKey = i_nonReadableKey | KEY_BACKSPACE;
                break;
            case NSUpArrowFunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_UP;
                break;
            case NSDownArrowFunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_DOWN;
                break;
            case NSRightArrowFunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_RIGHT;
                break;
            case NSLeftArrowFunctionKey:
                i_nonReadableKey = i_nonReadableKey | KEY_LEFT;
                break;
            case NSEnterCharacter:
                i_nonReadableKey = i_nonReadableKey | KEY_ENTER;
                break;
            default:
            {
                msg_Warn( VLCIntf, "user pressed unknown function key" );
                i_nonReadableKey = 0;
                break;
            }
        }
    }
    else
    {
        if( [[o_theEvent charactersIgnoringModifiers] isEqualToString: @" "] )
            i_nonReadableKey = i_nonReadableKey | KEY_SPACE;
        else
            i_nonReadableKey = i_nonReadableKey | StringToKey( (char *)[[[o_theEvent charactersIgnoringModifiers] lowercaseString] UTF8String] );
    }

    [[[VLCMain sharedInstance] getSimplePreferences] changeHotkeyTo: i_nonReadableKey];
}

@end
