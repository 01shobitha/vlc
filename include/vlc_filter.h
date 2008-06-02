/*****************************************************************************
 * vlc_filter.h: filter related structures
 *****************************************************************************
 * Copyright (C) 1999-2003 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _VLC_FILTER_H
#define _VLC_FILTER_H 1

#include <vlc_es.h>

/**
 * \file
 * This file defines the structure and types used by video and audio filters
 */

typedef struct filter_owner_sys_t filter_owner_sys_t;

/** Structure describing a filter
 * @warning BIG FAT WARNING : the code relies in the first 4 members of
 * filter_t and decoder_t to be the same, so if you have anything to add,
 * do it at the end of the structure.
 */
struct filter_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t *          p_module;
    filter_sys_t *      p_sys;

    /* Input format */
    es_format_t         fmt_in;

    /* Output format of filter */
    es_format_t         fmt_out;

    /* Filter configuration */
    config_chain_t *    p_cfg;

    picture_t *         ( * pf_video_filter ) ( filter_t *, picture_t * );
    block_t *           ( * pf_audio_filter ) ( filter_t *, block_t * );
    void                ( * pf_video_blend )  ( filter_t *, picture_t *,
                                                picture_t *, picture_t *,
                                                int, int, int );

    subpicture_t *      ( *pf_sub_filter ) ( filter_t *, mtime_t );
    int                 ( *pf_render_text ) ( filter_t *, subpicture_region_t *,
                                              subpicture_region_t * );
    int                 ( *pf_render_html ) ( filter_t *, subpicture_region_t *,
                                              subpicture_region_t * );

    /*
     * Buffers allocation
     */

    /* Audio output callbacks */
    block_t *       ( * pf_audio_buffer_new) ( filter_t *, int );

    /* Video output callbacks */
    picture_t     * ( * pf_vout_buffer_new) ( filter_t * );
    void            ( * pf_vout_buffer_del) ( filter_t *, picture_t * );
    void            ( * pf_picture_link)    ( picture_t * );
    void            ( * pf_picture_unlink)  ( picture_t * );

    /* SPU output callbacks */
    subpicture_t *  ( * pf_sub_buffer_new) ( filter_t * );
    void            ( * pf_sub_buffer_del) ( filter_t *, subpicture_t * );

    /* Private structure for the owner of the decoder */
    filter_owner_sys_t *p_owner;
};


/**
 * Create a picture_t *(*)( filter_t *, picture_t * ) compatible wrapper
 * using a void (*)( filter_t *, picture_t *, picture_t * ) function
 *
 * Currently used by the chroma video filters
 */
#define VIDEO_FILTER_WRAPPER( name )                                    \
    static picture_t *name ## _Filter ( filter_t *p_filter,             \
                                        picture_t *p_pic )              \
    {                                                                   \
        picture_t *p_outpic = p_filter->pf_vout_buffer_new( p_filter ); \
        if( !p_outpic )                                                 \
        {                                                               \
            msg_Warn( p_filter, "can't get output picture" );           \
            if( p_pic->pf_release )                                     \
                p_pic->pf_release( p_pic );                             \
            return NULL;                                                \
        }                                                               \
                                                                        \
        name( p_filter, p_pic, p_outpic );                              \
                                                                        \
        p_outpic->date = p_pic->date;                                   \
        p_outpic->b_force = p_pic->b_force;                             \
        p_outpic->i_nb_fields = p_pic->i_nb_fields;                     \
        p_outpic->b_progressive = p_pic->b_progressive;                 \
        p_outpic->b_top_field_first = p_pic->b_top_field_first;         \
                                                                        \
        if( p_pic->pf_release )                                         \
            p_pic->pf_release( p_pic );                                 \
        return p_outpic;                                                \
    }

#endif /* _VLC_FILTER_H */
