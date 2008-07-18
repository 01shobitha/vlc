/*****************************************************************************
 * motiondetec.c : Second version of a motion detection plugin.
 *****************************************************************************
 * Copyright (C) 2000-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_vout.h>

#include "vlc_filter.h"
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static picture_t *FilterPacked( filter_t *, picture_t * );
static void GaussianConvolution( uint32_t *, uint32_t *, int, int, int );
static int FindShapes( uint32_t *, uint32_t *, int, int, int,
                       int *, int *, int *, int *, int *);

#define NUM_COLORS 5000

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define FILTER_PREFIX "motiondetect-"

vlc_module_begin();
    set_description( N_("Motion detect video filter") );
    set_shortname( N_( "Motion Detect" ));
    set_capability( "video filter2", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    add_shortcut( "motion" );
    set_callbacks( Create, Destroy );
vlc_module_end();

struct filter_sys_t
{
    uint8_t *p_oldpix;
    uint8_t *p_oldpix_u;
    uint8_t *p_oldpix_v;
    uint32_t *p_buf;
    uint32_t *p_buf2;
    vlc_mutex_t lock;
};

/*****************************************************************************
 * Create
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        CASE_PLANAR_YUV
            p_filter->pf_video_filter = Filter;
            break;

        CASE_PACKED_YUV_422
            p_filter->pf_video_filter = FilterPacked;
            break;

        default:
            msg_Err( p_filter, "Unsupported input chroma (%4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_filter->p_sys->p_oldpix = NULL;
    p_filter->p_sys->p_buf = NULL;

    vlc_mutex_init( &p_filter->p_sys->lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    free( p_filter->p_sys->p_oldpix );
    free( p_filter->p_sys->p_buf );

    vlc_mutex_destroy( &p_filter->p_sys->lock );

    free( p_filter->p_sys );
}


/*****************************************************************************
 * Filter YUV Planar
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_inpic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;

    const uint8_t *p_inpix = p_inpic->p[Y_PLANE].p_pixels;
    const int i_src_pitch = p_inpic->p[Y_PLANE].i_pitch;
    const int i_src_visible = p_inpic->p[Y_PLANE].i_visible_pitch;
    const int i_num_lines = p_inpic->p[Y_PLANE].i_visible_lines;

    const uint8_t *p_inpix_u = p_inpic->p[U_PLANE].p_pixels;
    const uint8_t *p_inpix_v = p_inpic->p[V_PLANE].p_pixels;
    const int i_src_pitch_u = p_inpic->p[U_PLANE].i_pitch;
    const int i_num_lines_u = p_inpic->p[U_PLANE].i_visible_lines;

    uint8_t *p_oldpix;
    uint8_t *p_oldpix_u;
    uint8_t *p_oldpix_v;
    uint8_t *p_outpix;
    uint32_t *p_buf;
    uint32_t *p_buf2;

    int i,j;
    int last;

    if( !p_inpic ) return NULL;

    if( !p_sys->p_oldpix || !p_sys->p_buf )
    {
        free( p_sys->p_oldpix );
        free( p_sys->p_buf );
        p_sys->p_oldpix = malloc( i_src_pitch * i_num_lines );
        p_sys->p_oldpix_u = malloc( i_src_pitch_u * i_num_lines_u );
        p_sys->p_oldpix_v = malloc( i_src_pitch_u * i_num_lines_u );
        p_sys->p_buf = malloc( sizeof( uint32_t ) * i_src_pitch * i_num_lines );
        p_sys->p_buf2 = malloc( sizeof( uint32_t ) * i_src_pitch * i_num_lines);
        vlc_memcpy( p_sys->p_oldpix, p_inpix, i_src_pitch * i_num_lines );
        vlc_memcpy( p_sys->p_oldpix_u, p_inpix_u, i_src_pitch_u * i_num_lines_u );
        vlc_memcpy( p_sys->p_oldpix_v, p_inpix_v, i_src_pitch_u * i_num_lines_u );
        return p_inpic;
    }
    p_oldpix = p_sys->p_oldpix;
    p_oldpix_u = p_sys->p_oldpix_u;
    p_oldpix_v = p_sys->p_oldpix_v;
    p_buf = p_sys->p_buf;
    p_buf2 = p_sys->p_buf2;

    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        picture_Release( p_inpic );
        return NULL;
    }

    p_outpix = p_outpic->p[Y_PLANE].p_pixels;
    vlc_memcpy( p_outpic->p[Y_PLANE].p_pixels, p_inpic->p[Y_PLANE].p_pixels,
        p_inpic->p[Y_PLANE].i_pitch * p_inpic->p[Y_PLANE].i_visible_lines );
    vlc_memcpy( p_outpic->p[U_PLANE].p_pixels, p_inpic->p[U_PLANE].p_pixels,
        p_inpic->p[U_PLANE].i_pitch * p_inpic->p[U_PLANE].i_visible_lines );
    vlc_memcpy( p_outpic->p[V_PLANE].p_pixels, p_inpic->p[V_PLANE].p_pixels,
        p_inpic->p[V_PLANE].i_pitch * p_inpic->p[V_PLANE].i_visible_lines );

    vlc_mutex_lock( &p_filter->p_sys->lock );

    /**
     * Substract Y planes
     */
    for( i = 0; i < i_src_pitch * i_num_lines; i++ )
    {
        if( p_inpix[i] > p_oldpix[i] )
        {
            p_buf2[i] = p_inpix[i] - p_oldpix[i];
        }
        else
        {
            p_buf2[i] = p_oldpix[i] - p_inpix[i];
        }
    }
    int line;
    int col;
    int format;
    switch( p_inpic->format.i_chroma )
    {
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('J','4','2','0'):
        case VLC_FOURCC('Y','V','1','2'):
            format = 1;
            break;

        case VLC_FOURCC('I','4','2','2'):
        case VLC_FOURCC('J','4','2','2'):
            format = 2;
            break;

        default:
            format = 0;
            msg_Warn( p_filter, "Not taking chroma into account" );
            break;
    }

    if( format )
    {
        for( line = 0; line < i_num_lines_u; line++ )
        {
            for( col = 0; col < i_src_pitch_u; col ++ )
            {
                int diff;
                i = line * i_src_pitch_u + col;
                if( p_inpix_u[i] > p_oldpix_u[i] )
                {
                    diff = p_inpix_u[i] - p_oldpix_u[i];
                }
                else
                {
                    diff = p_oldpix_u[i] - p_inpix_u[i];
                }
                if( p_inpix_v[i] > p_oldpix_v[i] )
                {
                    diff += p_inpix_v[i] - p_oldpix_v[i];
                }
                else
                {
                    diff += p_oldpix_v[i] - p_inpix_v[i];
                }
                switch( format )
                {
                    case 1:
                        p_buf2[2*line*i_src_pitch+2*col] += diff;
                        p_buf2[2*line*i_src_pitch+2*col+1] += diff;
                        p_buf2[(2*line+1)*i_src_pitch+2*col] += diff;
                        p_buf2[(2*line+1)*i_src_pitch+2*col+1] += diff;
                        break;

                    case 2:
                        p_buf2[line*i_src_pitch+2*col] += diff;
                        p_buf2[line*i_src_pitch+2*col+1] += diff;
                        break;
                }
            }
        }
    }

    /**
     * Get the areas where movement was detected
     */
    int colors[NUM_COLORS];
    int color_x_min[NUM_COLORS];
    int color_x_max[NUM_COLORS];
    int color_y_min[NUM_COLORS];
    int color_y_max[NUM_COLORS];

    last = FindShapes( p_buf2, p_buf, i_src_pitch, i_src_visible, i_num_lines,
                   colors, color_x_min, color_x_max, color_y_min, color_y_max );

    /**
     * Count final number of shapes
     * Draw rectangles (there can be more than 1 moving shape in 1 rectangle)
     */
    j = 0;
    for( i = 1; i < last; i++ )
    {
        if( colors[i] == i && color_x_min[i] != -1 )
        {
            if( ( color_y_max[i] - color_y_min[i] ) * ( color_x_max[i] - color_x_min[i] ) < 16 ) continue;
            j++;
            int x, y;
            y = color_y_min[i];
            for( x = color_x_min[i]; x <= color_x_max[i]; x++ )
            {
                p_outpix[y*i_src_pitch+x] = 0xff;
            }
            y = color_y_max[i];
            for( x = color_x_min[i]; x <= color_x_max[i]; x++ )
            {
                p_outpix[y*i_src_pitch+x] = 0xff;
            }
            x = color_x_min[i];
            for( y = color_y_min[i]; y <= color_y_max[i]; y++ )
            {
                p_outpix[y*i_src_pitch+x] = 0xff;
            }
            x = color_x_max[i];
            for( y = color_y_min[i]; y <= color_y_max[i]; y++ )
            {
                p_outpix[y*i_src_pitch+x] = 0xff;
            }
        }
    }
    msg_Dbg( p_filter, "Counted %d moving shapes.", j);

    /**
     * We're done. Lets keep a copy of the picture
     */
    vlc_memcpy( p_oldpix, p_inpix, i_src_pitch * i_num_lines );
    vlc_memcpy( p_oldpix_u, p_inpix_u, i_src_pitch_u * i_num_lines_u );
    vlc_memcpy( p_oldpix_v, p_inpix_v, i_src_pitch_u * i_num_lines_u );

    vlc_mutex_unlock( &p_filter->p_sys->lock );

    return CopyInfoAndRelease( p_outpic, p_inpic );
}

/*****************************************************************************
 * Filter YUV Packed
 *****************************************************************************/
static picture_t *FilterPacked( filter_t *p_filter, picture_t *p_inpic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;

    int i_y_offset, i_u_offset, i_v_offset;

    const uint8_t *p_inpix;
    const uint8_t *p_inpix_u;
    const uint8_t *p_inpix_v;

    int i_src_pitch;
    int i_src_visible;
    int i_num_lines;

    uint8_t *p_oldpix;
    uint8_t *p_oldpix_u;
    uint8_t *p_oldpix_v;
    uint8_t *p_outpix;

    uint32_t *p_buf;
    uint32_t *p_buf2;

    int i,j;
    int last;

    if( GetPackedYuvOffsets( p_inpic->format.i_chroma, &i_y_offset,
                             &i_u_offset, &i_v_offset ) != VLC_SUCCESS )
    {
        msg_Warn( p_filter, "Unsupported input chroma (%4s)",
                  (char*)&(p_inpic->format.i_chroma) );
        picture_Release( p_inpic );
        return NULL;
    }

    p_inpix = p_inpic->p->p_pixels+i_y_offset;
    p_inpix_u = p_inpic->p->p_pixels+i_u_offset;
    p_inpix_v = p_inpic->p->p_pixels+i_v_offset;

    i_src_pitch = p_inpic->p->i_pitch;
    i_src_visible = p_inpic->p->i_visible_pitch;
    i_num_lines = p_inpic->p->i_visible_lines;

    if( !p_sys->p_oldpix || !p_sys->p_buf )
    {
        free( p_sys->p_oldpix );
        free( p_sys->p_buf );
        p_sys->p_oldpix = malloc( i_src_pitch * i_num_lines );
        p_sys->p_buf = malloc( sizeof( uint32_t ) * i_src_pitch * i_num_lines / 2 );
        p_sys->p_buf2 = malloc( sizeof( uint32_t ) * i_src_pitch * i_num_lines / 2 );
        vlc_memcpy( p_sys->p_oldpix, p_inpic->p->p_pixels,
                    i_src_pitch * i_num_lines );
        return p_inpic;
    }

    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        picture_Release( p_inpic );
        return NULL;
    }

    p_outpix = p_outpic->p->p_pixels+i_y_offset;
    vlc_memcpy( p_outpic->p->p_pixels, p_inpic->p->p_pixels,
                p_inpic->p->i_pitch * p_inpic->p->i_visible_lines );

    p_oldpix = p_sys->p_oldpix+i_y_offset;
    p_oldpix_u = p_sys->p_oldpix+i_u_offset;
    p_oldpix_v = p_sys->p_oldpix+i_v_offset;
    p_buf = p_sys->p_buf;
    p_buf2 = p_sys->p_buf2;

    vlc_mutex_lock( &p_filter->p_sys->lock );

    /**
     * Substract Y planes
     */
    for( i = 0; i < i_src_pitch * i_num_lines; i+=2 )
    {
        if( p_inpix[i] > p_oldpix[i] )
        {
            p_buf2[i>>1] = p_inpix[i] - p_oldpix[i];
        }
        else
        {
            p_buf2[i>>1] = p_oldpix[i] - p_inpix[i];
        }
    }

#if 0
    for( i = 0; i < i_src_pitch * i_num_lines; i+=4 )
    {
        int diff;
        if( p_inpix_u[i] > p_oldpix_u[i] )
        {
            diff = p_inpix_u[i] - p_oldpix_u[i];
        }
        else
        {
            diff = p_oldpix_u[i] - p_inpix_u[i];
        }
        if( p_inpix_v[i] > p_oldpix_v[i] )
        {
            diff += p_inpix_v[i] - p_oldpix_v[i];
        }
        else
        {
            diff += p_oldpix_v[i] - p_inpix_v[i];
        }

        p_buf2[(i>>2)<<1] += diff;
        p_buf2[((i>>2)<<1)+1] += diff;
    }
#endif

    /**
     * Get the areas where movement was detected
     */
    int colors[5000];
    int color_x_min[5000];
    int color_x_max[5000];
    int color_y_min[5000];
    int color_y_max[5000];

    last = FindShapes( p_buf2, p_buf, i_src_pitch/2, i_src_visible/2,
                       i_num_lines, colors, color_x_min, color_x_max,
                       color_y_min, color_y_max );

    /**
     * Count final number of shapes
     * Draw rectangles (there can be more than 1 moving shape in 1 rectangle)
     */
    j = 0;
    for( i = 1; i < last; i++ )
    {
        if( colors[i] == i && color_x_min[i] != -1 )
        {
            if( ( color_y_max[i] - color_y_min[i] ) * ( color_x_max[i] - color_x_min[i] ) < 16 ) continue;
            j++;
            printf("Shape: (%d,%d) (%d,%d)\n", color_y_max[i], color_y_min[i], color_x_max[i], color_x_min[i] );
            int x, y;
            y = color_y_min[i];
            for( x = color_x_min[i]*2; x <= color_x_max[i]*2; x+=2 )
            {
                p_outpix[y*i_src_pitch+x] = 0xff;
            }
            y = color_y_max[i];
            for( x = color_x_min[i]*2; x <= color_x_max[i]*2; x+=2 )
            {
                p_outpix[y*i_src_pitch+x] = 0xff;
            }
            x = color_x_min[i]*2;
            for( y = color_y_min[i]; y <= color_y_max[i]; y++ )
            {
                p_outpix[y*i_src_pitch+x] = 0xff;
            }
            x = color_x_max[i]*2;
            for( y = color_y_min[i]; y <= color_y_max[i]; y++ )
            {
                p_outpix[y*i_src_pitch+x] = 0xff;
            }
        }
    }
    msg_Dbg( p_filter, "Counted %d moving shapes.", j);

    /**
     * We're done. Lets keep a copy of the picture
     */
    vlc_memcpy( p_sys->p_oldpix, p_inpic->p->p_pixels, i_src_pitch * i_num_lines );

    vlc_mutex_unlock( &p_filter->p_sys->lock );

    return CopyInfoAndRelease( p_outpic, p_inpic );
}


/*****************************************************************************
 * Gaussian Convolution
 *****************************************************************************
 *    Gaussian convolution ( sigma == 1.4 )
 *
 *    |  2  4  5  4  2  |   |  2  4  4  4  2 |
 *    |  4  9 12  9  4  |   |  4  8 12  8  4 |
 *    |  5 12 15 12  5  | ~ |  4 12 16 12  4 |
 *    |  4  9 12  9  4  |   |  4  8 12  8  4 |
 *    |  2  4  5  4  2  |   |  2  4  4  4  2 |
 *****************************************************************************/
static void GaussianConvolution( uint32_t *p_inpix, uint32_t *p_smooth,
                                 int i_src_pitch, int i_num_lines,
                                 int i_src_visible )
{
    int x,y;
    for( y = 2; y < i_num_lines - 2; y++ )
    {
        for( x = 2; x < i_src_visible - 2; x++ )
        {
            p_smooth[y*i_src_visible+x] = (uint32_t)(
              /* 2 rows up */
                ( p_inpix[(y-2)*i_src_pitch+x-2] )
              + ((p_inpix[(y-2)*i_src_pitch+x-1]
              +   p_inpix[(y-2)*i_src_pitch+x]
              +   p_inpix[(y-2)*i_src_pitch+x+1])<<1 )
              + ( p_inpix[(y-2)*i_src_pitch+x+2] )
              /* 1 row up */
              + ((p_inpix[(y-1)*i_src_pitch+x-2]
              + ( p_inpix[(y-1)*i_src_pitch+x-1]<<1 )
              + ( p_inpix[(y-1)*i_src_pitch+x]*3 )
              + ( p_inpix[(y-1)*i_src_pitch+x+1]<<1 )
              +   p_inpix[(y-1)*i_src_pitch+x+2]
              /* */
              +   p_inpix[y*i_src_pitch+x-2]
              + ( p_inpix[y*i_src_pitch+x-1]*3 )
              + ( p_inpix[y*i_src_pitch+x]<<2 )
              + ( p_inpix[y*i_src_pitch+x+1]*3 )
              +   p_inpix[y*i_src_pitch+x+2]
              /* 1 row down */
              +   p_inpix[(y+1)*i_src_pitch+x-2]
              + ( p_inpix[(y+1)*i_src_pitch+x-1]<<1 )
              + ( p_inpix[(y+1)*i_src_pitch+x]*3 )
              + ( p_inpix[(y+1)*i_src_pitch+x+1]<<1 )
              +   p_inpix[(y+1)*i_src_pitch+x+2] )<<1 )
              /* 2 rows down */
              + ( p_inpix[(y+2)*i_src_pitch+x-2] )
              + ((p_inpix[(y+2)*i_src_pitch+x-1]
              +   p_inpix[(y+2)*i_src_pitch+x]
              +   p_inpix[(y+2)*i_src_pitch+x+1])<<1 )
              + ( p_inpix[(y+2)*i_src_pitch+x+2] )
              ) >> 6 /* 115 */;
        }
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static int FindShapes( uint32_t *p_diff, uint32_t *p_smooth,
                       int i_pitch, int i_visible, int i_lines,
                       int *colors,
                       int *color_x_min, int *color_x_max,
                       int *color_y_min, int *color_y_max )
{
    int last = 1;
    int i, j;

    /**
     * Apply some smoothing to remove noise
     */
    GaussianConvolution( p_diff, p_smooth, i_pitch, i_visible, i_lines );

    /**
     * Label the shapes and build the labels dependencies list
     */
    for( j = 0; j < i_pitch; j++ )
    {
        p_smooth[j] = 0;
        p_smooth[(i_lines-1)*i_pitch+j] = 0;
    }
    for( i = 1; i < i_lines-1; i++ )
    {
        p_smooth[i*i_pitch] = 0;
        for( j = 1; j < i_pitch-1; j++ )
        {
            if( p_smooth[i*i_pitch+j] > 15 )
            {
                if( p_smooth[(i-1)*i_pitch+j-1] )
                {
                    p_smooth[i*i_pitch+j] = p_smooth[(i-1)*i_pitch+j-1];
                }
                else if( p_smooth[(i-1)*i_pitch+j] )
                    p_smooth[i*i_pitch+j] = p_smooth[(i-1)*i_pitch+j];
                else if( p_smooth[i*i_pitch+j-1] )
                    p_smooth[i*i_pitch+j] = p_smooth[i*i_pitch+j-1];
                else
                {
                    if( last < 5000 )
                    {
                        p_smooth[i*i_pitch+j] = last;
                        colors[last] = last;
                        last++;
                    }
                }
                #define CHECK( A ) \
                if( p_smooth[A] && p_smooth[A] != p_smooth[i*i_pitch+j] ) \
                { \
                    if( p_smooth[A] < p_smooth[i*i_pitch+j] ) \
                        colors[p_smooth[i*i_pitch+j]] = p_smooth[A]; \
                    else \
                        colors[p_smooth[A]] = p_smooth[i*i_pitch+j]; \
                }
                CHECK( i*i_pitch+j-1 );
                CHECK( (i-1)*i_pitch+j-1 );
                CHECK( (i-1)*i_pitch+j );
                CHECK( (i-1)*i_pitch+j+1 );
                #undef CHECK
            }
            else
            {
                p_smooth[i*i_pitch+j] = 0;
            }
        }
        p_smooth[i*i_pitch+j] = 0;
    }

    /**
     * Initialise empty rectangle list
     */
    for( i = 1; i < last; i++ )
    {
        color_x_min[i] = -1;
        color_x_max[i] = -1;
        color_y_min[i] = -1;
        color_y_max[i] = -1;
    }

    /**
     * Compute rectangle coordinates
     */
    for( i = 0; i < i_pitch * i_lines; i++ )
    {
        if( p_smooth[i] )
        {
            while( colors[p_smooth[i]] != (int)p_smooth[i] )
                p_smooth[i] = colors[p_smooth[i]];
            if( color_x_min[p_smooth[i]] == -1 )
            {
                color_x_min[p_smooth[i]] =
                color_x_max[p_smooth[i]] = i % i_pitch;
                color_y_min[p_smooth[i]] =
                color_y_max[p_smooth[i]] = i / i_pitch;
            }
            else
            {
                int x = i % i_pitch, y = i / i_pitch;
                if( x < color_x_min[p_smooth[i]] )
                    color_x_min[p_smooth[i]] = x;
                if( x > color_x_max[p_smooth[i]] )
                    color_x_max[p_smooth[i]] = x;
                if( y < color_y_min[p_smooth[i]] )
                    color_y_min[p_smooth[i]] = y;
                if( y > color_y_max[p_smooth[i]] )
                    color_y_max[p_smooth[i]] = y;
            }
        }
    }

    /**
     * Merge overlaping rectangles
     */
    for( i = 1; i < last; i++ )
    {
        if( colors[i] != i ) continue;
        if( color_x_min[i] == -1 ) continue;
        for( j = i+1; j < last; j++ )
        {
            if( colors[j] != j ) continue;
            if( color_x_min[j] == -1 ) continue;
            if( __MAX( color_x_min[i], color_x_min[j] ) < __MIN( color_x_max[i], color_x_max[j] ) &&
                __MAX( color_y_min[i], color_y_min[j] ) < __MIN( color_y_max[i], color_y_max[j] ) )
            {
                color_x_min[i] = __MIN( color_x_min[i], color_x_min[j] );
                color_x_max[i] = __MAX( color_x_max[i], color_x_max[j] );
                color_y_min[i] = __MIN( color_y_min[i], color_y_min[j] );
                color_y_max[i] = __MAX( color_y_max[i], color_y_max[j] );
                color_x_min[j] = -1;
                j = 0;
            }
        }
    }

    return last;
}
