/*****************************************************************************
 * zvbi.c : VBI and Teletext PES demux and decoder using libzvbi
 *****************************************************************************
 * Copyright (C) 2007, M2X
 * $Id$
 *
 * Authors: Derk-Jan Hartman <djhartman at m2x dot nl> for M2X
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
 *
 * information on teletext format can be found here :
 * http://pdc.ro.nu/teletext.html
 *
 *****************************************************************************/

/* This module implements:
 * ETSI EN 301 775: VBI data in PES
 * ETSI EN 300 472: EBU Teletext data in PES
 * ETSI EN 300 706: Enhanced Teletext (libzvbi)
 * ETSI EN 300 231: Video Programme System [VPS] (libzvbi)
 * ETSI EN 300 294: 625-line Wide Screen Signaling [WSS] (libzvbi)
 * EIA-608 Revision A: Closed Captioning [CC] (libzvbi)
 */

#include <vlc/vlc.h>
#include <assert.h>
#include <stdint.h>
#include <libzvbi.h>

#include "vlc_vout.h"
#include "vlc_bits.h"
#include "vlc_codec.h"

typedef enum {
    DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE     = 0x02,
    DATA_UNIT_EBU_TELETEXT_SUBTITLE         = 0x03,
    DATA_UNIT_EBU_TELETEXT_INVERTED         = 0x0C,

    DATA_UNIT_ZVBI_WSS_CPR1204              = 0xB4,
    DATA_UNIT_ZVBI_CLOSED_CAPTION_525       = 0xB5,
    DATA_UNIT_ZVBI_MONOCHROME_SAMPLES_525   = 0xB6,

    DATA_UNIT_VPS                           = 0xC3,
    DATA_UNIT_WSS                           = 0xC4,
    DATA_UNIT_CLOSED_CAPTION                = 0xC5,
    DATA_UNIT_MONOCHROME_SAMPLES            = 0xC6,

    DATA_UNIT_STUFFING                      = 0xFF,
} data_unit_id;

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
static subpicture_t *Decode( decoder_t *, block_t ** );

#define PAGE_TEXT N_("Teletext page")
#define PAGE_LONGTEXT N_("Open the indicated Teletext page." \
        "Default page is index 100")

#define OPAQUE_TEXT N_("Text is always opaque")
#define OPAQUE_LONGTEXT N_("Setting vbi-opaque to false " \
        "makes the boxed text transparent." )

vlc_module_begin();
    set_description( _("VBI and Teletext decoder") );
    set_shortname( "VBI & Teletext" );
    set_capability( "decoder", 51 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_SCODEC );
    set_callbacks( Open, Close );

    add_integer( "vbi-page", 100, NULL,
                 PAGE_TEXT, PAGE_LONGTEXT, VLC_FALSE );
    add_bool( "vbi-opaque", VLC_TRUE, NULL,
                 OPAQUE_TEXT, OPAQUE_LONGTEXT, VLC_FALSE );
vlc_module_end();

/****************************************************************************
 * Local structures
 ****************************************************************************/

struct decoder_sys_t
{
   vbi_decoder *           p_vbi_dec;
   vbi_dvb_demux *         p_dvb_demux;
   unsigned int            i_wanted_page;
   unsigned int            i_last_page;
   vlc_bool_t              b_update;
   vlc_bool_t              b_opaque;
};

static void event_handler( vbi_event *ev, void *user_data );
static int RequestPage( vlc_object_t *p_this, char const *psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data );
static int Opaque( vlc_object_t *p_this, char const *psz_cmd,
                   vlc_value_t oldval, vlc_value_t newval, void *p_data );

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t *) p_this;
    decoder_sys_t *p_sys = NULL;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('t','e','l','x') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_decode_sub = Decode;
    p_sys = p_dec->p_sys = malloc( sizeof(decoder_sys_t) );
    if( p_sys == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_ENOMEM;

    }
    memset( p_sys, 0, sizeof(decoder_sys_t) );

    p_sys->p_vbi_dec = vbi_decoder_new();
    p_sys->p_dvb_demux = vbi_dvb_pes_demux_new( NULL, NULL );

    if( (p_sys->p_vbi_dec == NULL) || (p_sys->p_dvb_demux == NULL) )
    {
        msg_Err( p_dec, "VBI decoder/demux could not be created." );
        free( p_sys );
        return VLC_ENOMEM;
    }

    vbi_event_handler_register( p_sys->p_vbi_dec, VBI_EVENT_TTX_PAGE |
                                VBI_EVENT_CAPTION | VBI_EVENT_NETWORK |
                                VBI_EVENT_ASPECT | VBI_EVENT_PROG_INFO,
                                event_handler, p_dec );

    /* Create the var on vlc_global. */
    p_sys->i_wanted_page = var_CreateGetInteger( p_dec->p_libvlc_global,
                                                 "vbi-page" );
    var_AddCallback( p_dec->p_libvlc_global, "vbi-page",
                     RequestPage, p_sys );

    p_sys->b_opaque = var_CreateGetBool( p_dec, "vbi-opaque" );
    var_AddCallback( p_dec, "vbi-opaque", Opaque, p_sys );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*) p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_vbi_dec )
        vbi_decoder_delete( p_sys->p_vbi_dec );
    if( p_sys->p_dvb_demux )
        vbi_dvb_demux_delete( p_sys->p_dvb_demux );
    free( p_sys );
}

#define MAX_SLICES 32

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static subpicture_t *Decode( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t   *p_sys = p_dec->p_sys;
    block_t         *p_block;
    subpicture_t    *p_spu = NULL;
    video_format_t  fmt;
    vlc_bool_t      b_cached = VLC_FALSE;
    vbi_page        p_page;
    uint8_t         *p_pos;
    unsigned int    i_left;

    /* part of kludge */
    uint32_t        *p_begin, *p_end;
    unsigned        int x = 0, y = 0;
    vbi_opacity opacity;
    /* end part of kludge */

    if( (pp_block == NULL) || (*pp_block == NULL) )
        return NULL;
    p_block = *pp_block;
    *pp_block = NULL;

    p_pos = p_block->p_buffer;
    i_left = p_block->i_buffer;

    while( i_left > 0 )
    {
        vbi_sliced      p_sliced[MAX_SLICES];
        unsigned int    i_lines = 0;
        int64_t         i_pts;

        i_lines = vbi_dvb_demux_cor( p_sys->p_dvb_demux, p_sliced,
                                     MAX_SLICES, &i_pts, &p_pos, &i_left );

        if( i_lines > 0 )
            vbi_decode( p_sys->p_vbi_dec, p_sliced, i_lines, i_pts / 90000.0 );
    }

    /* Try to see if the page we want is in the cache yet */
    b_cached = vbi_fetch_vt_page( p_sys->p_vbi_dec, &p_page,
                                  vbi_dec2bcd( p_sys->i_wanted_page ),
                                  VBI_ANY_SUBNO, VBI_WST_LEVEL_3p5,
                                  25, FALSE );

    if( !b_cached )
        goto error;

    if( (p_sys->i_wanted_page == p_sys->i_last_page) &&
         (p_sys->b_update != VLC_TRUE) )
        goto error;

    p_sys->i_last_page = p_sys->i_wanted_page;
    p_sys->b_update = VLC_FALSE;
    msg_Dbg( p_dec, "we now have page: %d ready for display",
             p_sys->i_wanted_page );

    /* If there is a page or sub to render, then we do that here */
    /* Create the subpicture unit */
    p_spu = p_dec->pf_spu_buffer_new( p_dec );
    if( !p_spu )
    {
        msg_Warn( p_dec, "can't get spu buffer" );
        goto error;
    }

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('R','G','B','A');
    fmt.i_aspect = VOUT_ASPECT_FACTOR;
    fmt.i_sar_num = fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_visible_width = p_page.columns * 12;
    fmt.i_height = fmt.i_visible_height = p_page.rows * 10;
    fmt.i_bits_per_pixel = 32;
    fmt.i_x_offset = fmt.i_y_offset = 0;

    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_dec), &fmt );
    if( p_spu->p_region == NULL )
    {
        msg_Err( p_dec, "cannot allocate SPU region" );
        goto error;
    }

    p_spu->p_region->i_x = 0;
    p_spu->p_region->i_y = 0;

    /* Normal text subs, easy markup */
    p_spu->i_flags = SUBPICTURE_ALIGN_TOP;

    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = 0;
    p_spu->b_ephemer = VLC_TRUE;
    p_spu->b_absolute = VLC_FALSE;
    p_spu->b_pausable = VLC_TRUE;
    p_spu->i_original_picture_width = p_page.columns * 12;
    p_spu->i_original_picture_height = p_page.rows * 10;

    vbi_draw_vt_page( &p_page, VBI_PIXFMT_RGBA32_LE,
                      p_spu->p_region->picture.p->p_pixels, 1, 1 );
    p_spu->p_region->picture.p->i_lines = p_page.rows * 10;
    p_spu->p_region->picture.p->i_pitch = p_page.columns * 12 * 4;

    /* Kludge since zvbi doesn't provide an option to specify opacity. */
    p_begin = (uint32_t *)p_spu->p_region->picture.p->p_pixels;
    p_end   = (uint32_t *)p_spu->p_region->picture.p->p_pixels+(fmt.i_width * fmt.i_height);

    for( ; p_begin < p_end; p_begin++ )
    {
        opacity = p_page.text[ y / 10 * p_page.columns + x / 12 ].opacity;
        switch( opacity )
        {
            /* Show video instead of this character */
            case VBI_TRANSPARENT_SPACE:
                *p_begin = 0;
                break;
            /* To make the boxed text "closed captioning" transparent
             * change VLC_TRUE to VLC_FALSE.
             */
            case VBI_OPAQUE:
                if( p_sys->b_opaque )
                    break;
            /* Full text transparency. only foreground color is show */
            case VBI_TRANSPARENT_FULL:
            /* Transparency for boxed text */
            case VBI_SEMI_TRANSPARENT:
                if( (*p_begin & 0xffffff00) == 0xff  )
                    *p_begin = 0;
                break;
            default:
                break;
        }
        x++;
        if( x >= fmt.i_width )
        {
            x = 0;
            y++;
        }
    }
    /* end of kludge */

#if 0
    unsigned int i_total, i_textsize = 7000;
    char p_text[7000];

    i_total =  vbi_print_page_region( &p_page, p_text, i_textsize,
                        "ISO-8859-1", 0, 0, 0, 0, p_page.columns,
                        p_page.rows );
    p_text[i_total] = '\0';

    msg_Dbg( p_dec, "page %x-%x\n%s", p_page.pgno, p_page.subno, p_text );
#endif

    vbi_unref_page( &p_page );
    block_Release( p_block );
    return p_spu;

error:
    vbi_unref_page( &p_page );
    if( p_spu != NULL )
    {
        p_dec->pf_spu_buffer_del( p_dec, p_spu );
        p_spu = NULL;
    }

    block_Release( p_block );
    return NULL;
}

static void event_handler( vbi_event *ev, void *user_data)
{
    decoder_t *p_dec        = (decoder_t *)user_data;
    decoder_sys_t *p_sys    = p_dec->p_sys;

    if( ev->type == VBI_EVENT_TTX_PAGE )
    {
        /* msg_Dbg( p_dec, "Page %03x.%02x ",
                    ev->ev.ttx_page.pgno,
                    ev->ev.ttx_page.subno & 0xFF);
        */
        if( p_sys->i_last_page == vbi_bcd2dec( ev->ev.ttx_page.pgno ) )
            p_sys->b_update = VLC_TRUE;

        if( ev->ev.ttx_page.clock_update )
            msg_Dbg( p_dec, "clock" );
        if( ev->ev.ttx_page.header_update )
            msg_Dbg( p_dec, "header" );
    }
    else if( ev->type == VBI_EVENT_CAPTION )
        msg_Dbg( p_dec, "Caption line: %x", ev->ev.caption.pgno );
    else if( ev->type == VBI_EVENT_NETWORK )
        msg_Dbg( p_dec, "Network change" );
    else if( ev->type == VBI_EVENT_ASPECT )
        msg_Dbg( p_dec, "Aspect update" );
    else if( ev->type == VBI_EVENT_NETWORK )
        msg_Dbg( p_dec, "Program info received" );
}

static int RequestPage( vlc_object_t *p_this, char const *psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    decoder_sys_t   *p_sys = p_data;

    if( (newval.i_int > 0) && (newval.i_int < 999) )
        p_sys->i_wanted_page = newval.i_int;

    return VLC_SUCCESS;
}

static int Opaque( vlc_object_t *p_this, char const *psz_cmd,
                   vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    decoder_sys_t *p_sys = p_data;

    if( p_sys )
        p_sys->b_opaque = newval.b_bool;
    return VLC_SUCCESS;
}
