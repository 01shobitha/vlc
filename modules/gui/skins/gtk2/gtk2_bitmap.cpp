/*****************************************************************************
 * gtk2_bitmap.cpp: GTK2 implementation of the Bitmap class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_bitmap.cpp,v 1.8 2003/04/15 01:19:11 ipkiss Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

#if !defined WIN32

//--- GTK2 -----------------------------------------------------------------
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "os_api.h"
#include "graphics.h"
#include "gtk2_graphics.h"
#include "bitmap.h"
#include "gtk2_bitmap.h"
#include "skin_common.h"


//---------------------------------------------------------------------------
//   GTK2Bitmap
//---------------------------------------------------------------------------
GTK2Bitmap::GTK2Bitmap( intf_thread_t *p_intf, string FileName, int AColor )
    : Bitmap( p_intf, FileName, AColor )
{
/*    HBITMAP HBitmap;
    HBITMAP HBuf;
    BITMAP  Bmp;
    HDC     bufDC;
    AlphaColor = AColor;

    // Create image from file if it exists
    HBitmap = (HBITMAP) LoadImage( NULL, FileName.c_str(), IMAGE_BITMAP,
                                   0, 0, LR_LOADFROMFILE );
    if( HBitmap == NULL )
    {
        if( FileName != "" )
            msg_Warn( p_intf, "Couldn't load bitmap: %s", FileName.c_str() );

        HBitmap = CreateBitmap( 0, 0, 1, 32, NULL );
    }

    // Create device context
    bmpDC   = CreateCompatibleDC( NULL );
    SelectObject( bmpDC, HBitmap );

    // Get size of image
    GetObject( HBitmap, sizeof( Bmp ), &Bmp );
    Width  = Bmp.bmWidth;
    Height = Bmp.bmHeight;

    // If alpha color is not 0, then change 0 colors to non black color to avoid
    // window transparency
    if( (int)AlphaColor != OSAPI_GetNonTransparentColor( 0 ) )
    {
        bufDC = CreateCompatibleDC( bmpDC );
        HBuf = CreateCompatibleBitmap( bmpDC, Width, Height );
        SelectObject( bufDC, HBuf );

        LPRECT r = new RECT;
        HBRUSH Brush = CreateSolidBrush( OSAPI_GetNonTransparentColor( 0 ) );
        r->left   = 0;
        r->top    = 0;
        r->right  = Width;
        r->bottom = Height;
        FillRect( bufDC, r, Brush );
        DeleteObject( Brush );
        delete r;

        TransparentBlt( bufDC, 0, 0, Width, Height, bmpDC, 0, 0, Width, Height, 0 );
        BitBlt( bmpDC, 0, 0, Width, Height, bufDC, 0, 0, SRCCOPY );
        DeleteDC( bufDC );
        DeleteObject( HBuf );
    }

    // Delete objects
    DeleteObject( HBitmap );*/

    // Load the bitmap image
    Bmp = gdk_pixbuf_new_from_file( FileName.c_str(), NULL );
    if( Bmp == NULL )
    {
        if( FileName != "" )
            msg_Warn( p_intf, "Couldn't load bitmap: %s", FileName.c_str() );

     //   Bmp = gdk_pixbuf_new( GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
        Bmp = NULL;
    }

    Width = gdk_pixbuf_get_width( Bmp );
    Height = gdk_pixbuf_get_height( Bmp );
}
//---------------------------------------------------------------------------
GTK2Bitmap::GTK2Bitmap( intf_thread_t *p_intf, Graphics *from, int x, int y,
    int w, int h, int AColor ) : Bitmap( p_intf, from, x, y, w, h, AColor )
{
/*    Width  = w;
    Height = h;
    AlphaColor = AColor;
    HBITMAP HBmp;
    HDC fromDC = ( (GTK2Graphics *)from )->GetImageHandle();

    // Create image
    bmpDC = CreateCompatibleDC( fromDC );
    HBmp  = CreateCompatibleBitmap( fromDC, Width, Height );
    SelectObject( bmpDC, HBmp );
    DeleteObject( HBmp );
    BitBlt( bmpDC, 0, 0, Width, Height, fromDC, x, y, SRCCOPY );*/
}
//---------------------------------------------------------------------------
GTK2Bitmap::GTK2Bitmap( intf_thread_t *p_intf, Bitmap *c )
    : Bitmap( p_intf, c )
{
/*    HBITMAP HBuf;

    // Copy attibutes
    c->GetSize( Width, Height );
    AlphaColor = c->GetAlphaColor();

    // Copy bmpDC
    bmpDC = CreateCompatibleDC( NULL );
    HBuf  = CreateCompatibleBitmap( bmpDC, Width, Height );
    SelectObject( bmpDC, HBuf );

    BitBlt( bmpDC, 0, 0, Width, Height, ( (GTK2Bitmap *)c )->GetBmpDC(),
            0, 0, SRCCOPY );
    DeleteObject( HBuf );*/
}
//---------------------------------------------------------------------------
GTK2Bitmap::~GTK2Bitmap()
{
/*    DeleteDC( bmpDC );*/
}
//---------------------------------------------------------------------------
void GTK2Bitmap::DrawBitmap( int x, int y, int w, int h, int xRef, int yRef,
                              Graphics *dest )
{
/*    HDC destDC = ( (GTK2Graphics *)dest )->GetImageHandle();

    // New method, not available in win95
    TransparentBlt( destDC, xRef, yRef, w, h, bmpDC, x, y, w, h, AlphaColor );
*/
    GdkDrawable *destImg = ( (GTK2Graphics *)dest )->GetImage();
    GdkGC *destGC = ( (GTK2Graphics *)dest )->GetGC();

    gdk_pixbuf_render_to_drawable( Bmp, destImg, destGC, x, y, xRef, yRef, 
            w, h, GDK_RGB_DITHER_NONE, 0, 0);
}
//---------------------------------------------------------------------------
bool GTK2Bitmap::Hit( int x, int y)
{
/*    unsigned int c = GetPixel( bmpDC, x, y );
    if( c == AlphaColor || c == CLR_INVALID )
        return false;
    else
        return true;
*/
}
//---------------------------------------------------------------------------
int GTK2Bitmap::GetBmpPixel( int x, int y )
{
//    return GetPixel( bmpDC, x, y );
}
//---------------------------------------------------------------------------
void GTK2Bitmap::SetBmpPixel( int x, int y, int color )
{
//    SetPixelV( bmpDC, x, y, color );
}
//---------------------------------------------------------------------------

#endif
