
/*****************************************************************************
 * vout.m: MacOS X video output plugin
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: vout.m,v 1.59 2003/10/29 11:54:48 hartman Exp $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <QuickTime/QuickTime.h>

#include <vlc_keys.h>

#include "intf.h"
#include "vout.h"

#define QT_MAX_DIRECTBUFFERS 10
#define VL_MAX_DISPLAYS 16

struct picture_sys_t
{
    void *p_info;
    unsigned int i_size;

    /* When using I420 output */
    PlanarPixmapInfoYUV420 pixmap_i420;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );
static int  vout_Manage    ( vout_thread_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );

static int  CoSendRequest      ( vout_thread_t *, SEL );
static int  CoCreateWindow     ( vout_thread_t * );
static int  CoDestroyWindow    ( vout_thread_t * );
static int  CoToggleFullscreen ( vout_thread_t * );

static void VLCHideMouse       ( vout_thread_t *, BOOL );

static void QTScaleMatrix      ( vout_thread_t * );
static int  QTCreateSequence   ( vout_thread_t * );
static void QTDestroySequence  ( vout_thread_t * );
static int  QTNewPicture       ( vout_thread_t *, picture_t * );
static void QTFreePicture      ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * OpenVideo: allocates MacOS X video thread output method
 *****************************************************************************
 * This function allocates and initializes a MacOS X vout method.
 *****************************************************************************/
int E_(OpenVideo) ( vlc_object_t *p_this )
{   
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    OSErr err;
    int i_timeout;
    vlc_value_t value_drawable;

    var_Get( p_vout->p_vlc, "drawable", &value_drawable );

    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    /* We don't need an intf in mozilla plugin */
    if( value_drawable.i_int == 0 )
    {
        /* Wait for a MacOS X interface to appear. Timeout is 2 seconds. */
        for( i_timeout = 20 ; i_timeout-- ; )
        {
            if( NSApp == NULL )
            {
                msleep( INTF_IDLE_SLEEP );
            }
        }
    
        if( NSApp == NULL )
        {
            /* no MacOS X intf, unable to communicate with MT */
            msg_Err( p_vout, "no MacOS X interface present" );
            free( p_vout->p_sys );
            return( 1 );
        }
    
    
        if( [NSApp respondsToSelector: @selector(getIntf)] )
        {
            intf_thread_t * p_intf;
    
            for( i_timeout = 10 ; i_timeout-- ; )
            {
                if( ( p_intf = [NSApp getIntf] ) == NULL )
                {
                    msleep( INTF_IDLE_SLEEP );
                }
            }
    
            if( p_intf == NULL )
            {
                msg_Err( p_vout, "MacOS X intf has getIntf, but is NULL" );
                free( p_vout->p_sys );
                return( 1 );
            }
        }
    }

    p_vout->p_sys->h_img_descr = 
        (ImageDescriptionHandle)NewHandleClear( sizeof(ImageDescription) );
    p_vout->p_sys->p_matrix = (MatrixRecordPtr)malloc( sizeof(MatrixRecord) );
    p_vout->p_sys->p_fullscreen_state = NULL;

    p_vout->p_sys->b_mouse_pointer_visible = VLC_TRUE;
    p_vout->p_sys->b_mouse_moved = VLC_TRUE;
    p_vout->p_sys->i_time_mouse_last_moved = mdate();

    if( value_drawable.i_int != 0 )
    {
        p_vout->p_sys->mask = NewRgn();
        p_vout->p_sys->rect.left = 0 ;
        p_vout->p_sys->rect.right = 0 ;
        p_vout->p_sys->rect.top = 0 ;
        p_vout->p_sys->rect.bottom = 0 ;

        p_vout->p_sys->isplugin = VLC_TRUE ;
    
    } else
    {
        p_vout->p_sys->mask = NULL;
        p_vout->p_sys->isplugin = VLC_FALSE ;
    }

    /* set window size */
    p_vout->p_sys->s_rect.size.width = p_vout->i_window_width;
    p_vout->p_sys->s_rect.size.height = p_vout->i_window_height;

    if( ( err = EnterMovies() ) != noErr )
    {
        msg_Err( p_vout, "EnterMovies failed: %d", err );
        free( p_vout->p_sys->p_matrix );
        DisposeHandle( (Handle)p_vout->p_sys->h_img_descr );
        free( p_vout->p_sys );
        return( 1 );
    } 

    /* Damn QT isn't thread safe. so keep a lock in the p_vlc object */
    vlc_mutex_lock( &p_vout->p_vlc->quicktime_lock );

    err = FindCodec( kYUV420CodecType, bestSpeedCodec,
                        nil, &p_vout->p_sys->img_dc );
    
    vlc_mutex_unlock( &p_vout->p_vlc->quicktime_lock );
    if( err == noErr && p_vout->p_sys->img_dc != 0 )
    {
        p_vout->output.i_chroma = VLC_FOURCC('I','4','2','0');
        p_vout->p_sys->i_codec = kYUV420CodecType;
    }
    else
    {
        msg_Err( p_vout, "failed to find an appropriate codec" );
    }

    if( p_vout->p_sys->img_dc == 0 )
    {
        free( p_vout->p_sys->p_matrix );
        DisposeHandle( (Handle)p_vout->p_sys->h_img_descr );
        free( p_vout->p_sys );
        return VLC_EGENERIC;        
    }

    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    NSArray * o_screens = [NSScreen screens];
    if( [o_screens count] > 0 && var_Type( p_vout, "video-device" ) == 0 )
    {
        int i = 1;
        vlc_value_t val, text;
        NSScreen * o_screen;

        int i_option = config_GetInt( p_vout, "macosx-vdev" );

        var_Create( p_vout, "video-device", VLC_VAR_INTEGER |
                                            VLC_VAR_HASCHOICE ); 
        text.psz_string = _("Video device");
        var_Change( p_vout, "video-device", VLC_VAR_SETTEXT, &text, NULL );
        
        NSEnumerator * o_enumerator = [o_screens objectEnumerator];

        while( (o_screen = [o_enumerator nextObject]) != NULL )
        {
            char psz_temp[255];
            NSRect s_rect = [o_screen frame];

            snprintf( psz_temp, sizeof(psz_temp)/sizeof(psz_temp[0])-1, 
                      "%s %d (%dx%d)", _("Screen"), i,
                      (int)s_rect.size.width, (int)s_rect.size.height ); 

            text.psz_string = psz_temp;
            val.i_int = i;
            var_Change( p_vout, "video-device",
                        VLC_VAR_ADDCHOICE, &val, &text );

            if( ( i - 1 ) == i_option )
            {
                var_Set( p_vout, "video-device", val );
            }
            i++;
        }

        var_AddCallback( p_vout, "video-device", vout_VarCallback,
                         NULL );

        val.b_bool = VLC_TRUE;
        var_Set( p_vout, "intf-change", val );
    }
    [o_pool release];

    /* We don't need a window either in the mozilla plugin */
    if( p_vout->p_sys->isplugin == 0 )
    {
        if( CoCreateWindow( p_vout ) )
        {
            msg_Err( p_vout, "unable to create window" );
            free( p_vout->p_sys->p_matrix );
            DisposeHandle( (Handle)p_vout->p_sys->h_img_descr );
            free( p_vout->p_sys ); 
            return( 1 );
        }
    }

    p_vout->pf_init = vout_Init;
    p_vout->pf_end = vout_End;
    p_vout->pf_manage = vout_Manage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = vout_Display;

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;
    vlc_value_t val;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure; we already found a codec,
     * and the corresponding chroma we will be using. Since we can
     * arbitrary scale, stick to the coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    var_Get( p_vout->p_vlc, "drawable", &val );

    if( p_vout->p_sys->isplugin )
    {
        p_vout->p_sys->p_qdport = val.i_int;
    }

    SetPort( p_vout->p_sys->p_qdport );
    QTScaleMatrix( p_vout );

    if( QTCreateSequence( p_vout ) )
    {
        msg_Err( p_vout, "unable to create sequence" );
        return( 1 );
    }

    /* Try to initialize up to QT_MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < QT_MAX_DIRECTBUFFERS )
    {
        p_pic = NULL;

        /* Find an empty picture slot */
        for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++ )
        {
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
            {
                p_pic = p_vout->p_picture + i_index;
                break;
            }
        }

        /* Allocate the picture */
        if( p_pic == NULL || QTNewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    int i_index;

    QTDestroySequence( p_vout );

    /* Free the direct buffers we allocated */
    for( i_index = I_OUTPUTPICTURES; i_index; )
    {
        i_index--;
        QTFreePicture( p_vout, PP_OUTPUTPICTURE[ i_index ] );
    }
}

/*****************************************************************************
 * CloseVideo: destroy video thread output method
 *****************************************************************************/
void E_(CloseVideo) ( vlc_object_t *p_this )
{       
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    if ( !p_vout->p_sys->isplugin )
    {
        if( CoDestroyWindow( p_vout ) )
        {
            msg_Err( p_vout, "unable to destroy window" );
        }
    }

    if ( p_vout->p_sys->p_fullscreen_state != NULL )
        EndFullScreen ( p_vout->p_sys->p_fullscreen_state, NULL );

    ExitMovies();

    free( p_vout->p_sys->p_matrix );
    DisposeHandle( (Handle)p_vout->p_sys->h_img_descr );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    vlc_value_t val1;
    var_Get( p_vout->p_vlc, "drawableredraw", &val1 );

    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        if( CoToggleFullscreen( p_vout ) )  
        {
            return( 1 );
        }

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    if( (p_vout->i_changes & VOUT_SIZE_CHANGE) ||
            ( p_vout->p_sys->isplugin && val1.i_int == 1) )
    {
        if( p_vout->p_sys->isplugin ) 
        {
            val1.i_int = 0;
            var_Set( p_vout->p_vlc, "drawableredraw", val1 );
            QTScaleMatrix( p_vout );
            SetDSequenceMask( p_vout->p_sys->i_seq , p_vout->p_sys->mask );
        }
        else
        {
            QTScaleMatrix( p_vout );
            SetDSequenceMatrix( p_vout->p_sys->i_seq, 
                                p_vout->p_sys->p_matrix );
            p_vout->i_changes &= ~VOUT_SIZE_CHANGE;
        }
    }

    /* hide/show mouse cursor 
     * this code looks unnecessarily complicated, but is necessary like this.
     * it has to deal with multiple monitors and therefore checks a lot */
    if( !p_vout->p_sys->b_mouse_moved && p_vout->b_fullscreen )
    {
        if( mdate() - p_vout->p_sys->i_time_mouse_last_moved > 2000000 && 
                   p_vout->p_sys->b_mouse_pointer_visible )
        {
            VLCHideMouse( p_vout, YES );
        }
        else if ( !p_vout->p_sys->b_mouse_pointer_visible )
        {
            vlc_bool_t b_playing = NO;
            input_thread_t * p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT,
                                                                    FIND_ANYWHERE );

            if ( p_input != NULL )
            {
                vlc_value_t state;
                var_Get( p_input, "state", &state );
                b_playing = state.i_int != PAUSE_S;
                vlc_object_release( p_input );
            }
            if ( !b_playing )
            {
                VLCHideMouse( p_vout, NO );
            }
        }
    }
    else if ( p_vout->p_sys->b_mouse_moved && p_vout->b_fullscreen )
    {
        if( !p_vout->p_sys->b_mouse_pointer_visible )
        {
            VLCHideMouse( p_vout, NO );
        }
        else
        {
            p_vout->p_sys->b_mouse_moved = NO;
        }
    }
    
    return( 0 );
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to the display.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    OSErr err;
    CodecFlags flags;
    Rect oldrect;
    RgnHandle oldClip;

    if( p_vout->p_sys->isplugin )
    {
        oldClip = NewRgn();

        /* In mozilla plugin, mozilla browser also draws things in
         * the windows. So we have to update the port/Origin for each
         * picture. FIXME : the vout should lock something ! */
        GetPort( &p_vout->p_sys->p_qdportold );
	GetPortBounds( p_vout->p_sys->p_qdportold, &oldrect );
        GetClip( oldClip );

	LockPortBits( p_vout->p_sys->p_qdport );

        SetPort( p_vout->p_sys->p_qdport );
        SetOrigin( p_vout->p_sys->portx , p_vout->p_sys->porty );
        ClipRect( &p_vout->p_sys->rect );
   
	
        if( ( err = DecompressSequenceFrameS( 
                        p_vout->p_sys->i_seq,
                        p_pic->p_sys->p_info,
                        p_pic->p_sys->i_size,                    
                        codecFlagUseImageBuffer, &flags, nil ) != noErr ) )
        {
            msg_Warn( p_vout, "DecompressSequenceFrameS failed: %d", err );
        }
        else
        {
            QDFlushPortBuffer( p_vout->p_sys->p_qdport, p_vout->p_sys->mask );
        }


	SetOrigin( oldrect.left , oldrect.top );
        SetClip( oldClip );
        SetPort( p_vout->p_sys->p_qdportold );

	UnlockPortBits( p_vout->p_sys->p_qdport );

    }
    else
    { 
        if( ( err = DecompressSequenceFrameS(
                        p_vout->p_sys->i_seq,
                        p_pic->p_sys->p_info,
                        p_pic->p_sys->i_size,
                        codecFlagUseImageBuffer, &flags, nil ) != noErr ) )
        {
            msg_Warn( p_vout, "DecompressSequenceFrameS failed: %d", err );
        }
        else
        {
            QDFlushPortBuffer( p_vout->p_sys->p_qdport, nil );
        }
    }
}

/*****************************************************************************
 * CoSendRequest: send request to interface thread
 *****************************************************************************
 * Returns 0 on success, 1 otherwise
 *****************************************************************************/
static int CoSendRequest( vout_thread_t *p_vout, SEL sel )
{
    int i_ret = 0;

    VLCVout * o_vlv = [[VLCVout alloc] init];

    if( ( i_ret = ExecuteOnMainThread( o_vlv, sel, (void *)p_vout ) ) )
    {
        msg_Err( p_vout, "SendRequest: no way to communicate with mt" );
    }

    [o_vlv release];

    return( i_ret );
}

/*****************************************************************************
 * CoCreateWindow: create new window 
 *****************************************************************************
 * Returns 0 on success, 1 otherwise
 *****************************************************************************/
static int CoCreateWindow( vout_thread_t *p_vout )
{
    if( CoSendRequest( p_vout, @selector(createWindow:) ) )
    {
        msg_Err( p_vout, "CoSendRequest (createWindow) failed" );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * CoDestroyWindow: destroy window 
 *****************************************************************************
 * Returns 0 on success, 1 otherwise
 *****************************************************************************/
static int CoDestroyWindow( vout_thread_t *p_vout )
{
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        VLCHideMouse( p_vout, NO );
    }

    if( CoSendRequest( p_vout, @selector(destroyWindow:) ) )
    {
        msg_Err( p_vout, "CoSendRequest (destroyWindow) failed" );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * CoToggleFullscreen: toggle fullscreen 
 *****************************************************************************
 * Returns 0 on success, 1 otherwise
 *****************************************************************************/
static int CoToggleFullscreen( vout_thread_t *p_vout )
{
    QTDestroySequence( p_vout );

    if( CoDestroyWindow( p_vout ) )
    {
        msg_Err( p_vout, "unable to destroy window" );
        return( 1 );
    }
    
    p_vout->b_fullscreen = !p_vout->b_fullscreen;

    config_PutInt( p_vout, "fullscreen", p_vout->b_fullscreen );

    if( CoCreateWindow( p_vout ) )
    {
        msg_Err( p_vout, "unable to create window" );
        return( 1 );
    }

    if( QTCreateSequence( p_vout ) )
    {
        msg_Err( p_vout, "unable to create sequence" );
        return( 1 ); 
    } 

    return( 0 );
}

/*****************************************************************************
 * VLCHideMouse: if b_hide then hide the cursor
 *****************************************************************************/
static void VLCHideMouse ( vout_thread_t *p_vout, BOOL b_hide )
{
    BOOL b_inside;
    NSRect s_rect;
    NSPoint ml;
    NSWindow *o_window = p_vout->p_sys->o_window;
    NSView *o_contents = [o_window contentView];
    
    s_rect = [o_contents bounds];
    ml = [o_window convertScreenToBase:[NSEvent mouseLocation]];
    ml = [o_contents convertPoint:ml fromView:nil];
    b_inside = [o_contents mouse: ml inRect: s_rect];
    
    if ( b_hide && b_inside )
    {
        /* only hide if mouse over VLCView */
        [NSCursor hide];
        p_vout->p_sys->b_mouse_pointer_visible = 0;
    }
    else if ( !b_hide )
    {
        [NSCursor unhide];
        p_vout->p_sys->b_mouse_pointer_visible = 1;
    }
    p_vout->p_sys->b_mouse_moved = NO;
    p_vout->p_sys->i_time_mouse_last_moved = mdate();
    return;
}

/*****************************************************************************
 * QTScaleMatrix: scale matrix 
 *****************************************************************************/
static void QTScaleMatrix( vout_thread_t *p_vout )
{
    Rect s_rect;
    unsigned int i_width, i_height;
    Fixed factor_x, factor_y;
    unsigned int i_offset_x = 0;
    unsigned int i_offset_y = 0;
    vlc_value_t val;
    vlc_value_t valt;
    vlc_value_t vall;
    vlc_value_t valb;
    vlc_value_t valr;
    vlc_value_t valx;
    vlc_value_t valy;
    vlc_value_t valw;
    vlc_value_t valh;
    vlc_value_t valportx;
    vlc_value_t valporty;

    GetPortBounds( p_vout->p_sys->p_qdport, &s_rect );
    i_width = s_rect.right - s_rect.left;
    i_height = s_rect.bottom - s_rect.top;

    var_Get( p_vout->p_vlc, "drawable", &val );
    var_Get( p_vout->p_vlc, "drawablet", &valt );
    var_Get( p_vout->p_vlc, "drawablel", &vall );
    var_Get( p_vout->p_vlc, "drawableb", &valb );
    var_Get( p_vout->p_vlc, "drawabler", &valr );
    var_Get( p_vout->p_vlc, "drawablex", &valx );
    var_Get( p_vout->p_vlc, "drawabley", &valy );
    var_Get( p_vout->p_vlc, "drawablew", &valw );
    var_Get( p_vout->p_vlc, "drawableh", &valh );
    var_Get( p_vout->p_vlc, "drawableportx", &valportx );
    var_Get( p_vout->p_vlc, "drawableporty", &valporty );

    if( p_vout->p_sys->isplugin )
    {
        p_vout->p_sys->portx = valportx.i_int;
        p_vout->p_sys->porty = valporty.i_int;
        p_vout->p_sys->p_qdport = val.i_int;
        i_width = valw.i_int;
        i_height = valh.i_int;

        SetRectRgn( p_vout->p_sys->mask , vall.i_int - valx.i_int ,
                    valt.i_int - valy.i_int , valr.i_int - valx.i_int ,
                    valb.i_int - valy.i_int );
        p_vout->p_sys->rect.top = 0;
        p_vout->p_sys->rect.left = 0;
        p_vout->p_sys->rect.bottom = valb.i_int - valt.i_int;
        p_vout->p_sys->rect.right = valr.i_int - vall.i_int;
    }

    if( i_height * p_vout->output.i_aspect < i_width * VOUT_ASPECT_FACTOR )
    {
        int i_adj_width = i_height * p_vout->output.i_aspect /
                          VOUT_ASPECT_FACTOR;

        factor_x = FixDiv( Long2Fix( i_adj_width ),
                           Long2Fix( p_vout->output.i_width ) );
        factor_y = FixDiv( Long2Fix( i_height ),
                           Long2Fix( p_vout->output.i_height ) );

        i_offset_x = (i_width - i_adj_width) / 2 + i_offset_x;
    }
    else
    {
        int i_adj_height = i_width * VOUT_ASPECT_FACTOR /
                           p_vout->output.i_aspect;

        factor_x = FixDiv( Long2Fix( i_width ),
                           Long2Fix( p_vout->output.i_width ) );
        factor_y = FixDiv( Long2Fix( i_adj_height ),
                           Long2Fix( p_vout->output.i_height ) );

        i_offset_y = (i_height - i_adj_height) / 2 + i_offset_y;
    }
    
    SetIdentityMatrix( p_vout->p_sys->p_matrix );

    ScaleMatrix( p_vout->p_sys->p_matrix,
                 factor_x, factor_y,
                 Long2Fix(0), Long2Fix(0) );            

    TranslateMatrix( p_vout->p_sys->p_matrix, 
                     Long2Fix(i_offset_x), 
                     Long2Fix(i_offset_y) );

}

/*****************************************************************************
 * QTCreateSequence: create a new sequence 
 *****************************************************************************
 * Returns 0 on success, 1 otherwise
 *****************************************************************************/
static int QTCreateSequence( vout_thread_t *p_vout )
{
    OSErr err;
    ImageDescriptionPtr p_descr;

    HLock( (Handle)p_vout->p_sys->h_img_descr );
    p_descr = *p_vout->p_sys->h_img_descr;

    p_descr->idSize = sizeof(ImageDescription);
    p_descr->cType = p_vout->p_sys->i_codec;
    p_descr->version = 1;
    p_descr->revisionLevel = 0;
    p_descr->vendor = 'appl';
    p_descr->width = p_vout->output.i_width;
    p_descr->height = p_vout->output.i_height;
    p_descr->hRes = Long2Fix(72);
    p_descr->vRes = Long2Fix(72);
    p_descr->spatialQuality = codecLosslessQuality;
    p_descr->frameCount = 1;
    p_descr->clutID = -1;
    p_descr->dataSize = 0;
    p_descr->depth = 24;


    HUnlock( (Handle)p_vout->p_sys->h_img_descr );


    if( ( err = DecompressSequenceBeginS( 
                              &p_vout->p_sys->i_seq,
                              p_vout->p_sys->h_img_descr,
                              NULL, 0,
                              p_vout->p_sys->p_qdport,
                              NULL, NULL,
                              p_vout->p_sys->p_matrix,
                              0, p_vout->p_sys->mask,
                              codecFlagUseImageBuffer,
                              codecLosslessQuality,
                              p_vout->p_sys->img_dc ) ) )
    {
        msg_Err( p_vout, "DecompressSequenceBeginS failed: %d", err );
        return( 1 );
    }


    return( 0 );
}

/*****************************************************************************
 * QTDestroySequence: destroy sequence 
 *****************************************************************************/
static void QTDestroySequence( vout_thread_t *p_vout )
{
    CDSequenceEnd( p_vout->p_sys->i_seq );
}

/*****************************************************************************
 * QTNewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, 1 otherwise
 *****************************************************************************/
static int QTNewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_width  = p_vout->output.i_width;
    int i_height = p_vout->output.i_height;

    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return( -1 );
    }

    switch( p_vout->output.i_chroma )
    {
        case VLC_FOURCC('I','4','2','0'):

            p_pic->p_sys->p_info = (void *)&p_pic->p_sys->pixmap_i420;
            p_pic->p_sys->i_size = sizeof(PlanarPixmapInfoYUV420);

            /* Allocate the memory buffer */
            p_pic->p_data = vlc_memalign( &p_pic->p_data_orig,
                                          16, i_width * i_height * 3 / 2 );

            /* Y buffer */
            p_pic->Y_PIXELS = p_pic->p_data; 
            p_pic->p[Y_PLANE].i_lines = i_height;
            p_pic->p[Y_PLANE].i_pitch = i_width;
            p_pic->p[Y_PLANE].i_pixel_pitch = 1;
            p_pic->p[Y_PLANE].i_visible_pitch = i_width;

            /* U buffer */
            p_pic->U_PIXELS = p_pic->Y_PIXELS + i_height * i_width;
            p_pic->p[U_PLANE].i_lines = i_height / 2;
            p_pic->p[U_PLANE].i_pitch = i_width / 2;
            p_pic->p[U_PLANE].i_pixel_pitch = 1;
            p_pic->p[U_PLANE].i_visible_pitch = i_width / 2;

            /* V buffer */
            p_pic->V_PIXELS = p_pic->U_PIXELS + i_height * i_width / 4;
            p_pic->p[V_PLANE].i_lines = i_height / 2;
            p_pic->p[V_PLANE].i_pitch = i_width / 2;
            p_pic->p[V_PLANE].i_pixel_pitch = 1;
            p_pic->p[V_PLANE].i_visible_pitch = i_width / 2;

            /* We allocated 3 planes */
            p_pic->i_planes = 3;

#define P p_pic->p_sys->pixmap_i420
            P.componentInfoY.offset = (void *)p_pic->Y_PIXELS
                                       - p_pic->p_sys->p_info;
            P.componentInfoCb.offset = (void *)p_pic->U_PIXELS
                                        - p_pic->p_sys->p_info;
            P.componentInfoCr.offset = (void *)p_pic->V_PIXELS
                                        - p_pic->p_sys->p_info;

            P.componentInfoY.rowBytes = i_width;
            P.componentInfoCb.rowBytes = i_width / 2;
            P.componentInfoCr.rowBytes = i_width / 2;
#undef P

            break;

    default:
        /* Unknown chroma, tell the guy to get lost */
        free( p_pic->p_sys );
        msg_Err( p_vout, "never heard of chroma 0x%.8x (%4.4s)",
                 p_vout->output.i_chroma, (char*)&p_vout->output.i_chroma );
        p_pic->i_planes = 0;
        return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * QTFreePicture: destroy a picture allocated with QTNewPicture
 *****************************************************************************/
static void QTFreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    switch( p_vout->output.i_chroma )
    {
        case VLC_FOURCC('I','4','2','0'):
            free( p_pic->p_data_orig );
            break;
    }

    free( p_pic->p_sys );
}

/*****************************************************************************
 * VLCWindow implementation
 *****************************************************************************/
@implementation VLCWindow

- (void)setVout:(vout_thread_t *)_p_vout
{
    p_vout = _p_vout;
}

- (vout_thread_t *)getVout
{
    return( p_vout );
}

- (void)scaleWindowWithFactor: (float)factor
{
    NSSize newsize;
    int i_corrected_height, i_corrected_width;
    NSPoint topleftbase;
    NSPoint topleftscreen;
    
    if ( !p_vout->b_fullscreen )
    {
        topleftbase.x = 0;
        topleftbase.y = [self frame].size.height;
        topleftscreen = [self convertBaseToScreen: topleftbase];
        
        if( p_vout->output.i_height * p_vout->output.i_aspect > 
                        p_vout->output.i_width * VOUT_ASPECT_FACTOR )
        {
            i_corrected_width = p_vout->output.i_height * p_vout->output.i_aspect /
                                            VOUT_ASPECT_FACTOR;
            newsize.width = (int) ( i_corrected_width * factor );
            newsize.height = (int) ( p_vout->render.i_height * factor );
        }
        else
        {
            i_corrected_height = p_vout->output.i_width * VOUT_ASPECT_FACTOR /
                                            p_vout->output.i_aspect;
            newsize.width = (int) ( p_vout->render.i_width * factor );
            newsize.height = (int) ( i_corrected_height * factor );
        }

        [self setContentSize: newsize];
        
        [self setFrameTopLeftPoint: topleftscreen];
        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }
}

- (void)toggleFloatOnTop
{
    if( config_GetInt( p_vout, "macosx-float" ) )
    {
        config_PutInt( p_vout, "macosx-float", 0 );
        [p_vout->p_sys->o_window setLevel: NSNormalWindowLevel];
    }
    else
    {
        config_PutInt( p_vout, "macosx-float", 1 );
        [p_vout->p_sys->o_window setLevel: NSStatusWindowLevel];
    }
}

- (void)toggleFullscreen
{
    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
}

- (BOOL)isFullscreen
{
    return( p_vout->b_fullscreen );
}

- (BOOL)canBecomeKeyWindow
{
    return( YES );
}

- (void)keyDown:(NSEvent *)o_event
{
    unichar key = 0;
    vlc_value_t val;
    unsigned int i_pressed_modifiers = 0;

    i_pressed_modifiers = [o_event modifierFlags];

    if( i_pressed_modifiers & NSShiftKeyMask )
        val.i_int |= KEY_MODIFIER_SHIFT;
    if( i_pressed_modifiers & NSControlKeyMask )
        val.i_int |= KEY_MODIFIER_CTRL;
    if( i_pressed_modifiers & NSAlternateKeyMask )
        val.i_int |= KEY_MODIFIER_ALT;
    if( i_pressed_modifiers & NSCommandKeyMask )
        val.i_int |= KEY_MODIFIER_COMMAND;

    NSLog( @"detected the modifiers: %x", val.i_int );

    key = [[o_event charactersIgnoringModifiers] characterAtIndex: 0];

    if( key )
    {
        val.i_int |= CocoaConvertKey( key );
        var_Set( p_vout->p_vlc, "key-pressed", val );
        NSLog( @"detected the key: %x", key );
    }
    else
    {
        [super keyDown: o_event];
    }
}

- (void)updateTitle
{
    NSMutableString * o_title;
    playlist_t * p_playlist = vlc_object_find( p_vout, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    
    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    o_title = [NSMutableString stringWithUTF8String: 
        p_playlist->pp_items[p_playlist->i_index]->psz_name]; 
    vlc_mutex_unlock( &p_playlist->object_lock );

    vlc_object_release( p_playlist );

    if( o_title != nil )
    {
        NSRange prefix_range = [o_title rangeOfString: @"file:"];
        if( prefix_range.location != NSNotFound )
        {
            [o_title deleteCharactersInRange: prefix_range];
        }

        [self setTitleWithRepresentedFilename: o_title];
    }
    else
    {
        [self setTitle:
            [NSString stringWithCString: VOUT_TITLE " (QuickTime)"]];
    }
}

/* This is actually the same as VLCControls::stop. */
- (BOOL)windowShouldClose:(id)sender
{
    playlist_t * p_playlist = vlc_object_find( p_vout, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )      
    {
        return NO;
    }

    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );

    /* The window will be closed by the intf later. */
    return NO;
}

@end

/*****************************************************************************
 * VLCView implementation
 *****************************************************************************/
@implementation VLCView

- (void)drawRect:(NSRect)rect
{
    vout_thread_t * p_vout;
    id o_window = [self window];
    p_vout = (vout_thread_t *)[o_window getVout];
    
    [[NSColor blackColor] set];
    NSRectFill( rect );
    [super drawRect: rect];

    p_vout->i_changes |= VOUT_SIZE_CHANGE;
}

- (BOOL)acceptsFirstResponder
{
    return( YES );
}

- (BOOL)becomeFirstResponder
{
    vout_thread_t * p_vout;
    id o_window = [self window];
    p_vout = (vout_thread_t *)[o_window getVout];
    
    [o_window setAcceptsMouseMovedEvents: YES];
    return( YES );
}

- (BOOL)resignFirstResponder
{
    vout_thread_t * p_vout;
    id o_window = [self window];
    p_vout = (vout_thread_t *)[o_window getVout];
    
    [o_window setAcceptsMouseMovedEvents: NO];
    VLCHideMouse( p_vout, NO );
    return( YES );
}

- (void)mouseDown:(NSEvent *)o_event
{
    vout_thread_t * p_vout;
    id o_window = [self window];
    p_vout = (vout_thread_t *)[o_window getVout];
    vlc_value_t val;

    switch( [o_event type] )
    {        
        case NSLeftMouseDown:
        {
            var_Get( p_vout, "mouse-button-down", &val );
            val.i_int |= 1;
            var_Set( p_vout, "mouse-button-down", val );
        }
        break;
        
        default:
            [super mouseDown: o_event];
        break;
    }
}

- (void)otherMouseDown:(NSEvent *)o_event
{
    /* This is not the the wheel button. you need to poll the
     * mouseWheel event for that. other is a third, forth or fifth button */
    vout_thread_t * p_vout;
    id o_window = [self window];
    p_vout = (vout_thread_t *)[o_window getVout];
    vlc_value_t val;

    switch( [o_event type] )
    {
        case NSOtherMouseDown:
        {
            var_Get( p_vout, "mouse-button-down", &val );
            val.i_int |= 2;
            var_Set( p_vout, "mouse-button-down", val );
        }
        break;
        
        default:
            [super mouseDown: o_event];
        break;
    }
}

- (void)rightMouseDown:(NSEvent *)o_event
{
    vout_thread_t * p_vout;
    id o_window = [self window];
    p_vout = (vout_thread_t *)[o_window getVout];
    vlc_value_t val;

    switch( [o_event type] )
    {
        case NSRightMouseDown:
        {
            var_Get( p_vout, "mouse-button-down", &val );
            val.i_int |= 4;
            var_Set( p_vout, "mouse-button-down", val );
        }
        break;
        
        default:
            [super mouseDown: o_event];
        break;
    }
}

- (void)mouseUp:(NSEvent *)o_event
{
    vout_thread_t * p_vout;
    id o_window = [self window];
    p_vout = (vout_thread_t *)[o_window getVout];
    vlc_value_t val;

    switch( [o_event type] )
    {
        case NSLeftMouseUp:
        {
            vlc_value_t b_val;
            b_val.b_bool = VLC_TRUE;
            var_Set( p_vout, "mouse-clicked", b_val );
            
            var_Get( p_vout, "mouse-button-down", &val );
            val.i_int &= ~1;
            var_Set( p_vout, "mouse-button-down", val );
        }
        break;
                
        default:
            [super mouseUp: o_event];
        break;
    }
}

- (void)otherMouseUp:(NSEvent *)o_event
{
    vout_thread_t * p_vout;
    id o_window = [self window];
    p_vout = (vout_thread_t *)[o_window getVout];
    vlc_value_t val;

    switch( [o_event type] )
    {
        case NSOtherMouseUp:
        {
            var_Get( p_vout, "mouse-button-down", &val );
            val.i_int &= ~2;
            var_Set( p_vout, "mouse-button-down", val );
        }
        break;
                
        default:
            [super mouseUp: o_event];
        break;
    }
}

- (void)rightMouseUp:(NSEvent *)o_event
{
    vout_thread_t * p_vout;
    id o_window = [self window];
    p_vout = (vout_thread_t *)[o_window getVout];
    vlc_value_t val;

    switch( [o_event type] )
    {
        case NSRightMouseUp:
        {
            var_Get( p_vout, "mouse-button-down", &val );
            val.i_int &= ~4;
            var_Set( p_vout, "mouse-button-down", val );
        }
        break;
        
        default:
            [super mouseUp: o_event];
        break;
    }
}

- (void)mouseDragged:(NSEvent *)o_event
{
    [self mouseMoved:o_event];
}

- (void)otherMouseDragged:(NSEvent *)o_event
{
    [self mouseMoved:o_event];
}

- (void)rightMouseDragged:(NSEvent *)o_event
{
    [self mouseMoved:o_event];
}

- (void)mouseMoved:(NSEvent *)o_event
{
    NSPoint ml;
    NSRect s_rect;
    BOOL b_inside;

    vout_thread_t * p_vout;
    id o_window = [self window];
    p_vout = (vout_thread_t *)[o_window getVout];

    s_rect = [self bounds];
    ml = [self convertPoint: [o_event locationInWindow] fromView: nil];
    b_inside = [self mouse: ml inRect: s_rect];

    if( b_inside )
    {
        vlc_value_t val;
        int i_width, i_height, i_x, i_y;
        
        vout_PlacePicture( p_vout, (unsigned int)s_rect.size.width,
                                   (unsigned int)s_rect.size.height,
                                   &i_x, &i_y, &i_width, &i_height );

        val.i_int = ( ((int)ml.x) - i_x ) * 
                    p_vout->render.i_width / i_width;
        var_Set( p_vout, "mouse-x", val );

        val.i_int = ( ((int)ml.y) - i_y ) * 
                    p_vout->render.i_height / i_height;
        var_Set( p_vout, "mouse-y", val );
            
        val.b_bool = VLC_TRUE;
        var_Set( p_vout, "mouse-moved", val );
        p_vout->p_sys->i_time_mouse_last_moved = mdate();
        p_vout->p_sys->b_mouse_moved = YES;
    }
    else if ( !b_inside && !p_vout->p_sys->b_mouse_pointer_visible )
    {
        /* people with multiple monitors need their mouse,
         * even if VLCView in fullscreen. */
        VLCHideMouse( p_vout, NO );
    }
    
    [super mouseMoved: o_event];
}

@end

/*****************************************************************************
 * VLCVout implementation
 *****************************************************************************/
@implementation VLCVout

- (void)createWindow:(NSValue *)o_value
{
    vlc_value_t val;
    VLCView * o_view;
    NSScreen * o_screen;
    vout_thread_t * p_vout;
    vlc_bool_t b_main_screen;
    
    p_vout = (vout_thread_t *)[o_value pointerValue];

    p_vout->p_sys->o_window = [VLCWindow alloc];
    [p_vout->p_sys->o_window setVout: p_vout];
    [p_vout->p_sys->o_window setReleasedWhenClosed: YES];

    if( var_Get( p_vout, "video-device", &val ) < 0 )
    {
        o_screen = [NSScreen mainScreen];
        b_main_screen = 1;
    }
    else
    {
        NSArray *o_screens = [NSScreen screens];
        unsigned int i_index = val.i_int;
        
        if( [o_screens count] < i_index )
        {
            o_screen = [NSScreen mainScreen];
            b_main_screen = 1;
        }
        else
        {
            i_index--;
            o_screen = [o_screens objectAtIndex: i_index];
            config_PutInt( p_vout, "macosx-vdev", i_index );
            b_main_screen = (i_index == 0);
        }
    } 

    if( p_vout->b_fullscreen )
    {
        NSRect screen_rect = [o_screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0;

        if ( b_main_screen && p_vout->p_sys->p_fullscreen_state == NULL )
            BeginFullScreen( &p_vout->p_sys->p_fullscreen_state, NULL, 0, 0,
                             NULL, NULL, fullScreenAllowEvents );

        [p_vout->p_sys->o_window 
            initWithContentRect: screen_rect
            styleMask: NSBorderlessWindowMask
            backing: NSBackingStoreBuffered
            defer: NO screen: o_screen];

        //[p_vout->p_sys->o_window setLevel: NSPopUpMenuWindowLevel - 1];
        p_vout->p_sys->b_mouse_moved = YES;
        p_vout->p_sys->i_time_mouse_last_moved = mdate();
    }
    else
    {
        unsigned int i_stylemask = NSTitledWindowMask |
                                   NSMiniaturizableWindowMask |
                                   NSClosableWindowMask |
                                   NSResizableWindowMask;
        
        if ( p_vout->p_sys->p_fullscreen_state != NULL )
            EndFullScreen ( p_vout->p_sys->p_fullscreen_state, NULL );
        p_vout->p_sys->p_fullscreen_state = NULL;

        [p_vout->p_sys->o_window 
            initWithContentRect: p_vout->p_sys->s_rect
            styleMask: i_stylemask
            backing: NSBackingStoreBuffered
            defer: NO screen: o_screen];

        [p_vout->p_sys->o_window setAlphaValue: config_GetFloat( p_vout, "macosx-opaqueness" )];
        
        if( config_GetInt( p_vout, "macosx-float" ) )
        {
            [p_vout->p_sys->o_window setLevel: NSStatusWindowLevel];
        }
        
        if( !p_vout->p_sys->b_pos_saved )   
        {
            [p_vout->p_sys->o_window center];
        }
    }

    o_view = [[VLCView alloc] init];

    /* FIXME: [o_view setMenu:] */
    [p_vout->p_sys->o_window setContentView: o_view];
    [o_view autorelease];

    [o_view lockFocus];
    p_vout->p_sys->p_qdport = [o_view qdPort];

    [o_view unlockFocus];
    
    [p_vout->p_sys->o_window updateTitle];
    [p_vout->p_sys->o_window makeKeyAndOrderFront: nil];
}

- (void)destroyWindow:(NSValue *)o_value
{
    vout_thread_t * p_vout;

    p_vout = (vout_thread_t *)[o_value pointerValue];

    if( !p_vout->b_fullscreen )
    {
        NSRect s_rect;

        s_rect = [[p_vout->p_sys->o_window contentView] frame];
        p_vout->p_sys->s_rect.size = s_rect.size;

        s_rect = [p_vout->p_sys->o_window frame];
        p_vout->p_sys->s_rect.origin = s_rect.origin;

        p_vout->p_sys->b_pos_saved = YES;
    }
    
    p_vout->p_sys->p_qdport = nil;
    [p_vout->p_sys->o_window close];
    p_vout->p_sys->o_window = nil;
}

@end
