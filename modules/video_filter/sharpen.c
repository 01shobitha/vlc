/*****************************************************************************
 * sharpen.c: Sharpen video filter
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id: sharpen.c 18062 2006-11-26 14:20:34Z zorglub $
 *
 * Author: Jérémy DEMEULE <dj_mulder at djduron dot no-ip dot org>
 *         Jean-Baptiste Kempf <jb at videolan dot org>
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

/* The sharpen filter. */
/*
 * static int filter[] = { -1, -1, -1,
 *                         -1,  8, -1,
 *                         -1, -1, -1 };
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc_vout.h>

#include "vlc_filter.h"

#define SIG_TEXT N_("Sharpen strength (0-2)")
#define SIG_LONGTEXT N_("Set the Sharpen strength, between 0 and 2. Defaults to 0.05.")

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Augment contrast between contours.") );
    set_shortname( _("Sharpen video filter") );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );
    set_capability( "video filter2", 0 );
    add_float_with_range( "sharpen-sigma", 0.05, 0.0, 2.0, NULL,
        SIG_TEXT, SIG_LONGTEXT, VLC_FALSE );
    add_shortcut( "sharpen" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * filter_sys_t: Sharpen video filter descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Sharpen specific properties of an output thread.
 *****************************************************************************/

struct filter_sys_t
{
    float f_sigma;
};

/*****************************************************************************
 * clip: avoid negative value and value > 255
 *****************************************************************************/
inline static int32_t clip( int32_t a )
{
    return (a > 255) ? 255 : (a < 0) ? 0 : a;
}

/*****************************************************************************
 * Create: allocates Sharpen video thread output method
 *****************************************************************************
 * This function allocates and initializes a Sharpen vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    p_filter->pf_video_filter = Filter;

    p_filter->p_sys->f_sigma = var_GetFloat(p_this, "sharpen-sigma");

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Destroy: destroy Sharpen video thread output method
 *****************************************************************************
 * Terminate an output method created by SharpenCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    free( p_filter->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Invert image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    int i, j;
    vlc_value_t val;
    uint8_t *p_src = NULL;
    uint8_t *p_out = NULL;
    int i_src_pitch;
    int pix;
    const int v1 = -1;
    const int v2 = 3; /* 2^3 = 8 */

    if( !p_pic ) return NULL;
    if( !p_filter ) return NULL;
    if( !p_filter->p_sys ) return NULL;

    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }

    /* process the Y plane */
    p_src = p_pic->p[Y_PLANE].p_pixels;
    p_out = p_outpic->p[Y_PLANE].p_pixels;
    if( !p_src || !p_out )
    {
        msg_Warn( p_filter, "can't get Y plane" );
        if( p_pic->pf_release ) 
            p_pic->pf_release( p_pic );
        return NULL;
    }

    i_src_pitch = p_pic->p[Y_PLANE].i_visible_pitch;

    /* perform convolution only on Y plane. Avoid border line. */
    for( i = 0; i < p_pic->p[Y_PLANE].i_visible_lines; i++ )
    {
        if( (i == 0) || (i == p_pic->p[Y_PLANE].i_visible_lines - 1) )
        {
            for( j = 0; j < p_pic->p[Y_PLANE].i_visible_pitch; j++ )
                p_out[i * i_src_pitch + j] = clip( p_src[i * i_src_pitch + j] );
            continue ;
        }
        for( j = 0; j < p_pic->p[Y_PLANE].i_visible_pitch; j++ )
        {
            if( (j == 0) || (j == p_pic->p[Y_PLANE].i_visible_pitch - 1) )
            {
                p_out[i * i_src_pitch + j] = p_src[i * i_src_pitch + j];
                continue ;
            }

            pix = (p_src[(i - 1) * i_src_pitch + j - 1] * v1) +
                  (p_src[(i - 1) * i_src_pitch + j    ] * v1) +
                  (p_src[(i - 1) * i_src_pitch + j + 1] * v1) +
                  (p_src[(i    ) * i_src_pitch + j - 1] * v1) +
                  (p_src[(i    ) * i_src_pitch + j    ] << v2) +
                  (p_src[(i    ) * i_src_pitch + j + 1] * v1) +
                  (p_src[(i + 1) * i_src_pitch + j - 1] * v1) +
                  (p_src[(i + 1) * i_src_pitch + j    ] * v1) +
                  (p_src[(i + 1) * i_src_pitch + j + 1] * v1);

            p_out[i * i_src_pitch + j] =  clip( p_src[i * i_src_pitch + j] +
                        (p_filter->p_sys->f_sigma * pix) );
        }
    }


    p_filter->p_libvlc->pf_memcpy( p_outpic->p[U_PLANE].p_pixels,
            p_pic->p[U_PLANE].p_pixels,
            p_outpic->p[U_PLANE].i_lines * p_outpic->p[U_PLANE].i_pitch );

    p_filter->p_libvlc->pf_memcpy( p_outpic->p[V_PLANE].p_pixels,
            p_pic->p[V_PLANE].p_pixels,
            p_outpic->p[V_PLANE].i_lines * p_outpic->p[V_PLANE].i_pitch );


    p_outpic->date = p_pic->date;
    p_outpic->b_force = p_pic->b_force;
    p_outpic->i_nb_fields = p_pic->i_nb_fields;
    p_outpic->b_progressive = p_pic->b_progressive;
    p_outpic->b_top_field_first = p_pic->b_top_field_first;

    if( p_pic->pf_release )
        p_pic->pf_release( p_pic );

    return p_outpic;
}
