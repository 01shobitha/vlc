/*****************************************************************************
 * video.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: video.c,v 1.38 2003/08/09 19:49:13 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include <string.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "ffmpeg.h"

#ifdef LIBAVCODEC_PP
#   ifdef HAVE_POSTPROC_POSTPROCESS_H
#       include <postproc/postprocess.h>
#   else
#       include <libpostproc/postprocess.h>
#   endif
#else
#   include "postprocessing/postprocessing.h"
#endif

#include "video.h"


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void ffmpeg_CopyPicture( picture_t *, AVFrame *, vdec_thread_t * );

static int  ffmpeg_GetFrameBuf      ( struct AVCodecContext *, AVFrame * );
static void ffmpeg_ReleaseFrameBuf  ( struct AVCodecContext *, AVFrame * );

/*****************************************************************************
 * Local Functions
 *****************************************************************************/
static inline uint32_t ffmpeg_PixFmtToChroma( int i_ff_chroma )
{
    /* FIXME FIXME some of them are wrong */
    switch( i_ff_chroma )
    {
        case PIX_FMT_YUV420P:
        case PIX_FMT_YUV422:
            return( VLC_FOURCC('I','4','2','0') );
        case PIX_FMT_RGB24:
            return( VLC_FOURCC('R','V','2','4') );

        case PIX_FMT_YUV422P:
            return( VLC_FOURCC('I','4','2','2') );
        case PIX_FMT_YUV444P:
            return( VLC_FOURCC('I','4','4','4') );
        case PIX_FMT_YUV410P:
        case PIX_FMT_YUV411P:
        case PIX_FMT_BGR24:
        default:
            return( 0 );
    }
}

/* Return a Vout */
static vout_thread_t *ffmpeg_CreateVout( vdec_thread_t  *p_vdec,
                                         AVCodecContext *p_context )
{
    vout_thread_t *p_vout;
    unsigned int   i_width = p_context->width;
    unsigned int   i_height = p_context->height;
    uint32_t       i_chroma = ffmpeg_PixFmtToChroma( p_context->pix_fmt );
    unsigned int   i_aspect;

    if( !i_width || !i_height )
    {
        return( NULL ); /* Can't create a new vout without display size */
    }

    if( !i_chroma )
    {
        /* we make conversion if possible*/
        i_chroma = VLC_FOURCC('I','4','2','0');
    }

    i_aspect = VOUT_ASPECT_FACTOR * p_context->aspect_ratio;
    if( i_aspect == 0 )
    {
        i_aspect = VOUT_ASPECT_FACTOR * i_width / i_height;
    }

    /* Spawn a video output if there is none. First we look for our children,
     * then we look for any other vout that might be available. */
    p_vout = vout_Request( p_vdec->p_fifo, p_vdec->p_vout,
                           i_width, i_height, i_chroma, i_aspect );
#ifdef LIBAVCODEC_PP
    if( p_vdec->pp_mode && !p_vdec->pp_context )
    {
        int32_t i_cpu = p_vdec->p_fifo->p_libvlc->i_cpu;
        int i_flags = 0;

        if( i_cpu & CPU_CAPABILITY_MMX )
        {
            i_flags |= PP_CPU_CAPS_MMX;
        }
        if( i_cpu & CPU_CAPABILITY_MMXEXT )
        {
            i_flags |= PP_CPU_CAPS_MMX2;
        }
        if( i_cpu & CPU_CAPABILITY_3DNOW )
        {
            i_flags |= PP_CPU_CAPS_3DNOW;
        }

        switch( p_context->pix_fmt )
        {
            case PIX_FMT_YUV444P:
                i_flags |= PP_FORMAT_444;
                break;
            case PIX_FMT_YUV422P:
                i_flags |= PP_FORMAT_422;
                break;
            case PIX_FMT_YUV411P:
                i_flags |= PP_FORMAT_411;
                break;
            default:
                i_flags |= PP_FORMAT_420;
                break;
        }

        p_vdec->pp_context = pp_get_context( i_width, i_height, i_flags );
    }
#endif

    return p_vout;
}

/*****************************************************************************
 *
 * Functions that initialize, decode and end the decoding process
 *
 * Functions exported for ffmpeg.c
 *   * E_( InitThread_Video )
 *   * E_( DecodeThread )
 *   * E_( EndThread_Video )
 *****************************************************************************/

/*****************************************************************************
 * InitThread: initialize vdec output thread
 *****************************************************************************
 * This function is called from decoder_Run and performs the second step
 * of the initialization. It returns 0 on success. Note that the thread's
 * flag are not modified inside this function.
 *
 * ffmpeg codec will be open, some memory allocated. But Vout is not yet
 * open (done after the first decoded frame)
 *****************************************************************************/
static inline void SetDWBE( void *data, uint32_t dw )
{
    uint8_t *p = data;

    p[0] = (dw >> 24 )&0xff;
    p[1] = (dw >> 16 )&0xff;
    p[2] = (dw >>  8 )&0xff;
    p[3] = (dw )&0xff;
}

int E_( InitThread_Video )( vdec_thread_t *p_vdec )
{
    int i_tmp;
    int i_truncated;

    p_vdec->p_ff_pic = avcodec_alloc_frame();

    if( ( p_vdec->p_format =
          (BITMAPINFOHEADER *)p_vdec->p_fifo->p_bitmapinfoheader ) != NULL )
    {
        /* ***** Fill p_context with init values ***** */
        p_vdec->p_context->width  = p_vdec->p_format->biWidth;
        p_vdec->p_context->height = p_vdec->p_format->biHeight;
    }
    else
    {
        msg_Warn( p_vdec->p_fifo, "display informations missing" );
        p_vdec->p_format = NULL;
    }

    /*  ***** Get configuration of ffmpeg plugin ***** */
    i_tmp = config_GetInt( p_vdec->p_fifo, "ffmpeg-workaround-bugs" );
    p_vdec->p_context->workaround_bugs  = __MAX( __MIN( i_tmp, 99 ), 0 );

    i_tmp = config_GetInt( p_vdec->p_fifo, "ffmpeg-error-resilience" );
    p_vdec->p_context->error_resilience = __MAX( __MIN( i_tmp, 99 ), -1 );

    if( config_GetInt( p_vdec->p_fifo, "grayscale" ) )
    {
        p_vdec->p_context->flags|= CODEC_FLAG_GRAY;
    }

    p_vdec->b_hurry_up = config_GetInt(p_vdec->p_fifo, "ffmpeg-hurry-up");

    /* CODEC_FLAG_TRUNCATED */

    /* FIXME search real LIBAVCODEC_BUILD */
#if LIBAVCODEC_BUILD >= 4662
    i_truncated = config_GetInt( p_vdec->p_fifo, "ffmpeg-truncated" );
    if( i_truncated == 1 )
#if 0
        ||
        ( i_truncated == -1 && ( p_vdec->p_context->width == 0 || p_vdec->p_context->height == 0 ) ) )
#endif
    {
        p_vdec->p_context->flags |= CODEC_FLAG_TRUNCATED;
    }
#endif

    /* ***** Open the codec ***** */
    if( avcodec_open(p_vdec->p_context, p_vdec->p_codec) < 0 )
    {
        msg_Err( p_vdec->p_fifo, "cannot open codec (%s)",
                                 p_vdec->psz_namecodec );
        return( VLC_EGENERIC );
    }
    else
    {
        msg_Dbg( p_vdec->p_fifo, "ffmpeg codec (%s) started",
                                 p_vdec->psz_namecodec );
    }

    p_vdec->b_direct_rendering = 0;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-dr" ) &&
        p_vdec->p_codec->capabilities & CODEC_CAP_DR1 &&
        ffmpeg_PixFmtToChroma( p_vdec->p_context->pix_fmt ) &&
        /* Apparently direct rendering doesn't work with YUV422P */
        p_vdec->p_context->pix_fmt != PIX_FMT_YUV422P &&
        !(p_vdec->p_context->width % 16) && !(p_vdec->p_context->height % 16) )
    {
        /* Some codecs set pix_fmt only after the 1st frame has been decoded,
         * so we need to do another check in ffmpeg_GetFrameBuf() */
        p_vdec->b_direct_rendering = 1;
    }

    /* ***** Load post processing ***** */
#ifdef LIBAVCODEC_PP
    p_vdec->pp_context = NULL;
    p_vdec->pp_mode    = NULL;

    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-pp-q" ) > 0 )
    {
        int  i_quality = config_GetInt( p_vdec->p_fifo, "ffmpeg-pp-q" );
        char *psz_name = config_GetPsz( p_vdec->p_fifo, "ffmpeg-pp-name" );

        if( !psz_name )
        {
            psz_name = strdup( "default" );
        }
        else if( *psz_name == '\0' )
        {
            free( psz_name );
            psz_name = strdup( "default" );
        }

        p_vdec->pp_mode =
            pp_get_mode_by_name_and_quality( psz_name, i_quality );

        if( !p_vdec->pp_mode )
        {
            msg_Err( p_vdec->p_fifo, "failed geting mode for postproc" );
        }
        else
        {
            msg_Info( p_vdec->p_fifo, "postproc activated" );
        }
        free( psz_name );

        /* for now we cannot do postproc and dr */
        p_vdec->b_direct_rendering = 0;
    }
    else
    {
        msg_Dbg( p_vdec->p_fifo, "no postproc" );
    }
#endif

    /* ffmpeg doesn't properly release old pictures when frames are skipped */
    if( p_vdec->b_hurry_up ) p_vdec->b_direct_rendering = 0;

    /* Always use our get_buffer wrapper so we can calculate the
     * PTS correctly */
    p_vdec->p_context->get_buffer = ffmpeg_GetFrameBuf;
    p_vdec->p_context->release_buffer = ffmpeg_ReleaseFrameBuf;
    p_vdec->p_context->opaque = p_vdec;

    if( p_vdec->b_direct_rendering )
    {
        msg_Dbg( p_vdec->p_fifo, "using direct rendering" );
        p_vdec->p_context->flags |= CODEC_FLAG_EMU_EDGE;
    }

    /* ***** init this codec with special data ***** */
    if( p_vdec->p_format &&
            p_vdec->p_format->biSize > sizeof(BITMAPINFOHEADER) )
    {
        int b_gotpicture;
        int i_size = p_vdec->p_format->biSize - sizeof(BITMAPINFOHEADER);

        if( p_vdec->i_codec_id == CODEC_ID_MPEG4 )
        {
            uint8_t *p_vol = malloc( i_size + FF_INPUT_BUFFER_PADDING_SIZE );

            memcpy( p_vol, &p_vdec->p_format[1], i_size );
            memset( &p_vol[i_size], 0, FF_INPUT_BUFFER_PADDING_SIZE );

            avcodec_decode_video( p_vdec->p_context, p_vdec->p_ff_pic,
                                  &b_gotpicture,
                                  p_vol,
                                  i_size );
            free( p_vol );
        }
#if LIBAVCODEC_BUILD >= 4666
        else if( p_vdec->i_codec_id == CODEC_ID_SVQ3 )
        {
            uint8_t *p;

            p_vdec->p_context->extradata_size = i_size + 12;
            p = p_vdec->p_context->extradata  =
                malloc( p_vdec->p_context->extradata_size );

            memcpy( &p[0],  "SVQ3", 4 );
            memset( &p[4], 0, 8 );
            memcpy( &p[12], &p_vdec->p_format[1], i_size );
        }
#endif
        else
        {
            p_vdec->p_context->extradata_size = i_size;
            p_vdec->p_context->extradata =
                malloc( i_size + FF_INPUT_BUFFER_PADDING_SIZE );
            memcpy( p_vdec->p_context->extradata,
                    &p_vdec->p_format[1], i_size );
            memset( &((uint8_t*)p_vdec->p_context->extradata)[i_size],
                    0, FF_INPUT_BUFFER_PADDING_SIZE );
        }
    }
    p_vdec->p_vout = NULL;

    p_vdec->input_pts_previous = 0;
    p_vdec->input_pts = 0;

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * DecodeThread: Called to decode one frame
 *****************************************************************************
 * We have to get a frame stored in a pes, give it to ffmpeg decoder and send
 * the image to the output.
 *****************************************************************************/
void  E_( DecodeThread_Video )( vdec_thread_t *p_vdec )
{
    pes_packet_t    *p_pes;
    int     i_frame_size;
    int     i_used;
    int     b_drawpicture;
    int     b_gotpicture;
    picture_t *p_pic;                                         /* vlc picture */

    /* TODO implement it in a better way */
    /* A good idea could be to decode all I pictures and see for the other */
    if( p_vdec->b_hurry_up && p_vdec->i_frame_late > 4 )
    {
        b_drawpicture = 0;
        if( p_vdec->i_frame_late < 8 )
        {
            p_vdec->p_context->hurry_up = 2;
        }
        else
        {
            /* too much late picture, won't decode
               but break picture until a new I, and for mpeg4 ...*/
            p_vdec->i_frame_late--; /* needed else it will never be decrease */
            input_ExtractPES( p_vdec->p_fifo, NULL );
            return;
        }
    }
    else
    {
        b_drawpicture = 1;
        p_vdec->p_context->hurry_up = 0;
    }

    if( p_vdec->i_frame_late > 0 &&
        mdate() - p_vdec->i_frame_late_start > (mtime_t)5000000 )
    {
        msg_Err( p_vdec->p_fifo, "more than 5 seconds of late video -> "
                 "dropping (to slow computer ?)" );
        do
        {
            input_ExtractPES( p_vdec->p_fifo, &p_pes );
            if( !p_pes )
            {
                p_vdec->p_fifo->b_error = 1;
                return;
            }

            if( p_pes->i_pts > 0 )
            {
                p_vdec->input_pts_previous = p_vdec->input_pts;
                p_vdec->input_pts = p_pes->i_pts;
            }
            input_DeletePES( p_vdec->p_fifo->p_packets_mgt, p_pes );

        } while( p_vdec->input_pts <= 0 || p_vdec->input_pts < mdate() );
    }

    if( !p_vdec->p_context->width || !p_vdec->p_context->height )
    {
        p_vdec->p_context->hurry_up = 5;
    }

    do
    {
        input_ExtractPES( p_vdec->p_fifo, &p_pes );
        if( !p_pes )
        {
            p_vdec->p_fifo->b_error = 1;
            return;
        }

        if( p_pes->i_pts > 0 )
        {
            p_vdec->input_pts_previous = p_vdec->input_pts;
            p_vdec->input_pts = p_pes->i_pts;
        }

        i_frame_size = p_pes->i_pes_size;

        if( i_frame_size > 0 )
        {
            uint8_t *p_last;
            int i_need;
            /* XXX Don't forget that ffmpeg required a little more bytes
             * that the real frame size */
            i_need = i_frame_size + FF_INPUT_BUFFER_PADDING_SIZE + p_vdec->i_buffer;
            if( p_vdec->i_buffer_size < i_need)
            {
                p_last = p_vdec->p_buffer;
                p_vdec->p_buffer = malloc( i_need );
                p_vdec->i_buffer_size = i_need;
                if( p_vdec->i_buffer > 0 )
                {
                    memcpy( p_vdec->p_buffer, p_last, p_vdec->i_buffer );
                }
                FREE( p_last );
            }
            i_frame_size =
                E_( GetPESData )( p_vdec->p_buffer + p_vdec->i_buffer,
                                  i_frame_size , p_pes );
            memset( p_vdec->p_buffer + p_vdec->i_buffer + i_frame_size,
                    0, FF_INPUT_BUFFER_PADDING_SIZE );
        }

        input_DeletePES( p_vdec->p_fifo->p_packets_mgt, p_pes );

    } while( i_frame_size <= 0 );

    i_frame_size += p_vdec->i_buffer;

usenextdata:
    i_used = avcodec_decode_video( p_vdec->p_context,
                                   p_vdec->p_ff_pic,
                                   &b_gotpicture,
                                   p_vdec->p_buffer,
                                   i_frame_size );

    if( p_vdec->p_fifo->b_die || p_vdec->p_fifo->b_error ) return;

#if 0
    msg_Dbg( p_vdec->p_fifo,
             "used:%d framesize:%d (%s picture)",
             i_used, i_frame_size, b_gotpicture ? "got":"no got" );
#endif
    if( i_used < 0 )
    {
        msg_Warn( p_vdec->p_fifo, "cannot decode one frame (%d bytes)",
                                  i_frame_size );
        p_vdec->i_frame_error++;
        p_vdec->i_buffer = 0;
        return;
    }
    else if( i_used < i_frame_size )
    {
        memmove( p_vdec->p_buffer,
                 p_vdec->p_buffer + i_used,
                 p_vdec->i_buffer_size - i_used );

        p_vdec->i_buffer = i_frame_size - i_used;
    }
    else
    {
        p_vdec->i_buffer = 0;
    }

    /* consumed bytes */
    i_frame_size -= i_used;

   /* Update frame late count*/
    if( p_vdec->pts <= mdate() )
    {
        p_vdec->i_frame_late++;
        if( p_vdec->i_frame_late == 1 )
        {
            p_vdec->i_frame_late_start = mdate();
        }
    }
    else
    {
        p_vdec->i_frame_late = 0;
    }

    if( !b_gotpicture || p_vdec->p_ff_pic->linesize[0] == 0 || !b_drawpicture )
    {
        return;
    }

    if( !p_vdec->b_direct_rendering )
    {
        p_vdec->p_vout = ffmpeg_CreateVout( p_vdec, p_vdec->p_context );
        if( !p_vdec->p_vout )
        {
            msg_Err( p_vdec->p_fifo, "cannot create vout" );
            p_vdec->p_fifo->b_error = 1; /* abort */
            return;
        }

        /* Get a new picture */
        while( !(p_pic = vout_CreatePicture( p_vdec->p_vout, 0, 0, 0 ) ) )
        {
            if( p_vdec->p_fifo->b_die || p_vdec->p_fifo->b_error )
            {
                return;
            }
            msleep( VOUT_OUTMEM_SLEEP );
        }

        /* fill p_picture_t from AVVideoFrame and do chroma conversion
         * if needed */
        ffmpeg_CopyPicture( p_pic, p_vdec->p_ff_pic, p_vdec );
    }
    else
    {
        p_pic = (picture_t *)p_vdec->p_ff_pic->opaque;
    }

    /* Set the PTS
     * There is an ugly hack here because some demuxers pass us a dts instead
     * of a pts so this screw up things for streams with B frames. */
    if( p_vdec->p_ff_pic->pts &&
        ( !p_vdec->p_context->has_b_frames ||
          p_vdec->p_ff_pic->pict_type == FF_B_TYPE ) )
    {
        p_vdec->pts = p_vdec->p_ff_pic->pts;
    }

    if( p_vdec->pts <= 0 )
    {
        p_vdec->pts = mdate() + DEFAULT_PTS_DELAY;  // FIXME
    }

    /* Send decoded frame to vout */
    vout_DatePicture( p_vdec->p_vout, p_pic, p_vdec->pts );
    vout_DisplayPicture( p_vdec->p_vout, p_pic );

    /* interpolate the next PTS */
    if( p_vdec->p_context->frame_rate > 0 )
    {
        p_vdec->pts += I64C(1000000) * (2 + p_vdec->p_ff_pic->repeat_pict) *
                       p_vdec->p_context->frame_rate_base /
                       (2 * p_vdec->p_context->frame_rate);
    }

    if( i_frame_size > 0 )
    {
        goto usenextdata; /* try to use all data */
    }
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
void E_( EndThread_Video )( vdec_thread_t *p_vdec )
{

#ifdef LIBAVCODEC_PP
    if( p_vdec->pp_mode )
    {
        pp_free_mode( p_vdec->pp_mode );
        if( p_vdec->pp_context )
        {
            pp_free_context( p_vdec->pp_context );
        }
    }
#endif

    if( p_vdec->p_ff_pic )
    {
        free( p_vdec->p_ff_pic );
    }

    /* We are about to die. Reattach video output to p_vlc. */
    vout_Request( p_vdec->p_fifo, p_vdec->p_vout, 0, 0, 0, 0 );
}

/*****************************************************************************
 * ffmpeg_CopyPicture: copy a picture from ffmpeg internal buffers to a
 *                     picture_t structure (when not in direct rendering mode).
 *****************************************************************************/
static void ffmpeg_CopyPicture( picture_t    *p_pic,
                                AVFrame *p_ff_pic,
                                vdec_thread_t *p_vdec )
{
    int i_plane;
    int i_size;
    int i_line;

    uint8_t *p_dst;
    uint8_t *p_src;
    int i_src_stride;
    int i_dst_stride;

    if( ffmpeg_PixFmtToChroma( p_vdec->p_context->pix_fmt ) )
    {
#ifdef LIBAVCODEC_PP
        if( p_vdec->pp_mode && p_vdec->pp_context )
        {
            uint8_t *src[3], *dst[3];
            int     i_src_stride[3], i_dst_stride[3];

            for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
            {
                src[i_plane] = p_ff_pic->data[i_plane];
                dst[i_plane] = p_pic->p[i_plane].p_pixels;

                i_src_stride[i_plane] = p_ff_pic->linesize[i_plane];
                i_dst_stride[i_plane] = p_pic->p[i_plane].i_pitch;
            }
            pp_postprocess( src, i_src_stride,
                            dst, i_dst_stride,
                            p_vdec->p_context->width,
                            p_vdec->p_context->height,
                            p_ff_pic->qscale_table, p_ff_pic->qstride,
                            p_vdec->pp_mode, p_vdec->pp_context,
                            p_ff_pic->pict_type );
        }
        else
        {
#endif
            for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
            {
                p_src  = p_ff_pic->data[i_plane];
                p_dst = p_pic->p[i_plane].p_pixels;
                i_src_stride = p_ff_pic->linesize[i_plane];
                i_dst_stride = p_pic->p[i_plane].i_pitch;

                i_size = __MIN( i_src_stride, i_dst_stride );
                for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
                {
                    p_vdec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src, i_size );
                    p_src += i_src_stride;
                    p_dst += i_dst_stride;
                }
            }
#ifdef LIBAVCODEC_PP
        }
#endif
    }
    else
    {
        /* we need to convert to I420 */
        switch( p_vdec->p_context->pix_fmt )
        {
            AVPicture dest_pic;
            int i;

            case( PIX_FMT_YUV410P ):
            case( PIX_FMT_YUV411P ):
                for( i = 0; i < p_pic->i_planes; i++ )
                {
                    dest_pic.data[i] = p_pic->p[i].p_pixels;
                    dest_pic.linesize[i] = p_pic->p[i].i_pitch;
                }
                img_convert( &dest_pic, PIX_FMT_YUV420P,
                             (AVPicture *)p_ff_pic,
                             p_vdec->p_context->pix_fmt,
                             p_vdec->p_context->width,
                             p_vdec->p_context->height );
                break;
            default:
                msg_Err( p_vdec->p_fifo, "don't know how to convert chroma %i",
                         p_vdec->p_context->pix_fmt );
                p_vdec->p_fifo->b_error = 1;
                break;
        }
    }
}

/*****************************************************************************
 * ffmpeg_GetFrameBuf: callback used by ffmpeg to get a frame buffer.
 *****************************************************************************
 * It is used for direct rendering as well as to get the right PTS for each
 * decoded picture (even in indirect rendering mode).
 *****************************************************************************/
static int ffmpeg_GetFrameBuf( struct AVCodecContext *p_context,
                               AVFrame *p_ff_pic )
{
    vdec_thread_t *p_vdec = (vdec_thread_t *)p_context->opaque;
    picture_t *p_pic;

    /* Set picture PTS */
    if( p_context->flags & CODEC_FLAG_TRUNCATED )
    {
        p_ff_pic->pts = p_vdec->input_pts_previous;
        p_vdec->input_pts_previous = 0;
    }
    else
    {
        p_ff_pic->pts = p_vdec->input_pts;
        p_vdec->input_pts = 0;
    }

    /* Not much to do in indirect rendering mode */
    if( !p_vdec->b_direct_rendering )
    {
        return avcodec_default_get_buffer( p_context, p_ff_pic );
    }

    /* Some codecs set pix_fmt only after the 1st frame has been decoded,
     * so this check is necessary. */
    if( !ffmpeg_PixFmtToChroma( p_context->pix_fmt ) ||
        p_vdec->p_context->width % 16 || p_vdec->p_context->height % 16 )
    {
        msg_Dbg( p_vdec->p_fifo, "disabling direct rendering" );
        p_vdec->b_direct_rendering = 0;
        return avcodec_default_get_buffer( p_context, p_ff_pic );
    }

    /* Check and (re)create our vout if needed */
    p_vdec->p_vout = ffmpeg_CreateVout( p_vdec, p_vdec->p_context );
    if( !p_vdec->p_vout )
    {
        msg_Err( p_vdec->p_fifo, "cannot create vout" );
        p_vdec->p_fifo->b_error = 1; /* abort */
        p_vdec->b_direct_rendering = 0;
        return avcodec_default_get_buffer( p_context, p_ff_pic );
    }

    p_vdec->p_vout->render.b_allow_modify_pics = 0;

    /* Get a new picture */
    while( !(p_pic = vout_CreatePicture( p_vdec->p_vout, 0, 0, 0 ) ) )
    {
        if( p_vdec->p_fifo->b_die || p_vdec->p_fifo->b_error )
        {
            p_vdec->b_direct_rendering = 0;
            return avcodec_default_get_buffer( p_context, p_ff_pic );
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }
    p_vdec->p_context->draw_horiz_band = NULL;

    p_ff_pic->opaque = (void*)p_pic;
    p_ff_pic->type = FF_BUFFER_TYPE_USER;
    p_ff_pic->data[0] = p_pic->p[0].p_pixels;
    p_ff_pic->data[1] = p_pic->p[1].p_pixels;
    p_ff_pic->data[2] = p_pic->p[2].p_pixels;
    p_ff_pic->data[3] = NULL; /* alpha channel but I'm not sure */

    p_ff_pic->linesize[0] = p_pic->p[0].i_pitch;
    p_ff_pic->linesize[1] = p_pic->p[1].i_pitch;
    p_ff_pic->linesize[2] = p_pic->p[2].i_pitch;
    p_ff_pic->linesize[3] = 0;

    if( p_ff_pic->reference != 0 )
    {
        vout_LinkPicture( p_vdec->p_vout, p_pic );
    }
    /* FIXME what is that, should give good value */
    p_ff_pic->age = 256*256*256*64; // FIXME FIXME from ffmpeg

    return( 0 );
}

static void  ffmpeg_ReleaseFrameBuf( struct AVCodecContext *p_context,
                                     AVFrame *p_ff_pic )
{
    vdec_thread_t *p_vdec = (vdec_thread_t *)p_context->opaque;
    picture_t *p_pic;

    if( p_ff_pic->type != FF_BUFFER_TYPE_USER )
    {
        avcodec_default_release_buffer( p_context, p_ff_pic );
        return;
    }

    p_pic = (picture_t*)p_ff_pic->opaque;

    p_ff_pic->data[0] = NULL;
    p_ff_pic->data[1] = NULL;
    p_ff_pic->data[2] = NULL;
    p_ff_pic->data[3] = NULL;

    if( p_ff_pic->reference != 0 )
    {
        vout_UnlinkPicture( p_vdec->p_vout, p_pic );
    }
}
