/*****************************************************************************
 * mpegvideo.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mpegvideo.c,v 1.20 2003/11/07 16:53:54 massiot Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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
 * Problem with this implementation:
 *
 * Although we should time-stamp each picture with a PTS, this isn't possible
 * with the current implementation.
 * The problem comes from the fact that for non-low-delay streams we can't
 * calculate the PTS of pictures used as backward reference. Even the temporal
 * reference number doesn't help here because all the pictures don't
 * necessarily have the same duration (eg. 3:2 pulldown).
 *
 * However this doesn't really matter as far as the MPEG muxers are concerned
 * because they allow having empty PTS fields. --gibalou
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>
#include "codecs.h"                         /* WAVEFORMATEX BITMAPINFOHEADER */

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct packetizer_s
{
    /* Input properties */
    decoder_fifo_t          *p_fifo;
    bit_stream_t            bit_stream;

    /* Output properties */
    sout_packetizer_input_t *p_sout_input;
    sout_format_t           output_format;

    mtime_t                 i_interpolated_dts;
    mtime_t                 i_old_duration;
    mtime_t                 i_last_ref_pts;
    double                  d_frame_rate;
    int                     i_progressive_sequence;
    int                     i_low_delay;
    uint8_t                 p_sequence_header[150];
    int                     i_sequence_header_length;
    int                     i_last_sequence_header;
    vlc_bool_t              b_expect_discontinuity;

} packetizer_t;

static int  Open    ( vlc_object_t * );
static int  Run     ( decoder_fifo_t * );

static int  InitThread     ( packetizer_t * );
static void PacketizeThread   ( packetizer_t * );
static void EndThread      ( packetizer_t * );
static void BitstreamCallback ( bit_stream_t *, vlc_bool_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("MPEG-I/II video packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( Open, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the packetizer and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC( 'm', 'p', 'g', 'v') &&
        p_dec->p_fifo->i_fourcc != VLC_FOURCC( 'm', 'p', 'g', '1') &&
        p_dec->p_fifo->i_fourcc != VLC_FOURCC( 'm', 'p', 'g', '2') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_run = Run;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int Run( decoder_fifo_t *p_fifo )
{
    packetizer_t *p_pack;
    int b_error;

    msg_Info( p_fifo, "Running mpegvideo packetizer" );
    if( !( p_pack = malloc( sizeof( packetizer_t ) ) ) )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    memset( p_pack, 0, sizeof( packetizer_t ) );

    p_pack->p_fifo = p_fifo;

    if( InitThread( p_pack ) != 0 )
    {
        DecoderError( p_fifo );
        return( -1 );
    }

    while( ( !p_pack->p_fifo->b_die )&&( !p_pack->p_fifo->b_error ) )
    {
        PacketizeThread( p_pack );
    }


    if( ( b_error = p_pack->p_fifo->b_error ) )
    {
        DecoderError( p_pack->p_fifo );
    }

    EndThread( p_pack );

    if( p_pack )
    {
        free( p_pack );
    }

    if( b_error )
    {
        return( -1 );
    }

    return( 0 );
}


/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/

static int InitThread( packetizer_t *p_pack )
{

    p_pack->output_format.i_cat = VIDEO_ES;
    p_pack->output_format.i_fourcc = VLC_FOURCC( 'm', 'p', 'g', 'v');
    p_pack->output_format.i_width  = 0;
    p_pack->output_format.i_height = 0;
    p_pack->output_format.i_bitrate= 0;
    p_pack->output_format.i_extra_data = 0;
    p_pack->output_format.p_extra_data = NULL;

    p_pack->b_expect_discontinuity = 0;
    p_pack->p_sout_input = NULL;

    if( InitBitstream( &p_pack->bit_stream, p_pack->p_fifo,
                       BitstreamCallback, (void *)p_pack ) != VLC_SUCCESS )
    {
        msg_Err( p_pack->p_fifo, "cannot initialize bitstream" );
        return -1;
    }

    return( 0 );
}

/* from ISO 13818-2 */
/* converting frame_rate_code to frame_rate */
static const double pd_frame_rates[16] =
{
    0, 24000.0/1001, 24, 25, 30000.0/1001, 30, 50, 60000.0/1001, 60,
    0, 0, 0, 0, 0, 0, 0
};

static int CopyUntilNextStartCode( packetizer_t   *p_pack,
                                   sout_buffer_t  *p_sout_buffer,
                                   unsigned int   *pi_pos )
{
    int i_copy = 0;

    do
    {
        p_sout_buffer->p_buffer[(*pi_pos)++] =
                GetBits( &p_pack->bit_stream, 8 );
        i_copy++;

        if( *pi_pos + 2048 > p_sout_buffer->i_buffer_size )
        {
            sout_BufferRealloc( p_pack->p_sout_input->p_sout,
                                p_sout_buffer,
                                p_sout_buffer->i_buffer_size + 50 * 1024);
        }

    } while( ShowBits( &p_pack->bit_stream, 24 ) != 0x01 &&
             !p_pack->p_fifo->b_die && !p_pack->p_fifo->b_error );

    return( i_copy );
}

/*****************************************************************************
 * PacketizeThread: packetize an unit (here copy a complete pes)
 *****************************************************************************/
static void PacketizeThread( packetizer_t *p_pack )
{
    sout_buffer_t *p_sout_buffer = NULL;
    vlc_bool_t    b_seen_slice = VLC_FALSE;
    int32_t       i_pos;
    int           i_skipped;
    mtime_t       i_duration; /* of the parsed picture */

    mtime_t       i_pts = 0;
    mtime_t       i_dts = 0;

    /* needed to calculate pts/dts */
    int i_temporal_ref = 0;
    int i_picture_coding_type = 0;
    int i_picture_structure = 0x03; /* frame picture */
    int i_top_field_first = 0;
    int i_repeat_first_field = 0;
    int i_progressive_frame = 0;

    if( !p_pack->p_sout_input )
    {
        byte_t p_temp[512];/* 150 bytes is the maximal size
                               of a sequence_header + sequence_extension */
        int i_frame_rate_code;


        /* skip data until we find a sequence_header_code */
        /* TODO: store skipped somewhere so can send it to the mux
         * after the input is created */
        i_skipped = 0;
        while( ShowBits( &p_pack->bit_stream, 32 ) != 0x1B3 &&
               !p_pack->p_fifo->b_die && !p_pack->p_fifo->b_error )
        {
            RemoveBits( &p_pack->bit_stream, 8 );
            i_skipped++;
        }
        msg_Warn( p_pack->p_fifo, "sequence_header_code found (%d skipped)",
                 i_skipped );

        /* copy the start_code */
        i_pos = 0;
        GetChunk( &p_pack->bit_stream, p_temp, 4 ); i_pos += 4;

        /* horizontal_size_value */
        p_pack->output_format.i_width = ShowBits( &p_pack->bit_stream, 12 );
        /* vertical_size_value */
        p_pack->output_format.i_height= ShowBits( &p_pack->bit_stream, 24 ) & 0xFFF;
        /* frame_rate_code */
        i_frame_rate_code = ShowBits( &p_pack->bit_stream, 32 ) & 0xF;

        /* copy headers */
        GetChunk( &p_pack->bit_stream, p_temp + i_pos, 7 ); i_pos += 7;

        /* intra_quantiser_matrix [non_intra_quantiser_matrix] */
        if( ShowBits( &p_pack->bit_stream, 7 ) & 0x1 )
        {

            GetChunk( &p_pack->bit_stream, p_temp + i_pos, 64 ); i_pos += 64;

            if( ShowBits( &p_pack->bit_stream, 8 ) & 0x1 )
            {
                GetChunk( &p_pack->bit_stream, p_temp + i_pos, 65); i_pos += 65;
            }
        }
        /* non_intra_quantiser_matrix */
        else if( ShowBits( &p_pack->bit_stream, 8 ) & 0x1 )
        {
            GetChunk( &p_pack->bit_stream, p_temp + i_pos, 65); i_pos += 65;
        }
        /* nothing */
        else
        {
            GetChunk( &p_pack->bit_stream, p_temp + i_pos, 1 ); i_pos += 1;
        }

        /* sequence_extension (10 bytes) */
        if( ShowBits( &p_pack->bit_stream, 32 ) != 0x1B5 )
        {
            msg_Dbg( p_pack->p_fifo, "ARRGG no extension_start_code" );
            p_pack->i_progressive_sequence = 1;
            p_pack->i_low_delay = 1;
        }
        else
        {
            GetChunk( &p_pack->bit_stream, p_temp + i_pos, 10 );
            p_pack->i_progressive_sequence = ( p_temp[i_pos+5]&0x08 ) ? 1 : 0;
            p_pack->i_low_delay = ( p_temp[i_pos+9]&0x80 ) ? 1 : 0;
            i_pos += 10;
        }

        /* remember sequence_header and sequence_extention */
        memcpy( p_pack->p_sequence_header, p_temp, i_pos );
        p_pack->i_sequence_header_length = i_pos;

        p_pack->d_frame_rate =  pd_frame_rates[i_frame_rate_code];

        msg_Warn( p_pack->p_fifo,
                  "creating input (image size %dx%d, frame rate %.2f)",
                  p_pack->output_format.i_width,
                  p_pack->output_format.i_height,
                  p_pack->d_frame_rate );

        /* now we have informations to create the input */
        p_pack->p_sout_input = sout_InputNew( p_pack->p_fifo,
                                              &p_pack->output_format );

        if( !p_pack->p_sout_input )
        {
            msg_Err( p_pack->p_fifo, "cannot add a new stream" );
            p_pack->p_fifo->b_error = VLC_TRUE;
            return;
        }

        p_sout_buffer = sout_BufferNew( p_pack->p_sout_input->p_sout,
                                        100 * 1024 );
        p_sout_buffer->i_size = i_pos;
        memcpy( p_sout_buffer->p_buffer, p_temp, i_pos );

    }
    else
    {
        p_sout_buffer =
            sout_BufferNew( p_pack->p_sout_input->p_sout, 100 * 1024 );
        i_pos = 0;
    }

    p_pack->i_last_sequence_header++;

    for( ;; )
    {
        uint32_t    i_code;
        if( p_pack->p_fifo->b_die || p_pack->p_fifo->b_error )
        {
            break;
        }

        i_code = ShowBits( &p_pack->bit_stream, 32 );

        if( b_seen_slice && ( i_code < 0x101 || i_code > 0x1af ) )
        {
            break;
        }

        if( i_code == 0x1B8 ) /* GOP */
        {
            /* usefull for bad MPEG-1 : repeat the sequence_header
               every second */
            if( p_pack->i_last_sequence_header > (int) p_pack->d_frame_rate )
            {
               memcpy( p_sout_buffer->p_buffer + i_pos,
                       p_pack->p_sequence_header,
                       p_pack->i_sequence_header_length );
               i_pos += p_pack->i_sequence_header_length;
               p_pack->i_last_sequence_header = 0;
            }
            p_sout_buffer->i_flags |= SOUT_BUFFER_FLAGS_GOP;
            CopyUntilNextStartCode( p_pack, p_sout_buffer, &i_pos );
        }
        else if( i_code == 0x100 ) /* Picture */
        {
            /* picture_start_code */
            GetChunk( &p_pack->bit_stream,
                      p_sout_buffer->p_buffer + i_pos, 4 ); i_pos += 4;

            NextPTS( &p_pack->bit_stream, &i_pts, &i_dts );
            i_temporal_ref = ShowBits( &p_pack->bit_stream, 10 );
            i_picture_coding_type = ShowBits( &p_pack->bit_stream, 13 ) & 0x3;

            CopyUntilNextStartCode( p_pack, p_sout_buffer, &i_pos );
        }
        else if( i_code == 0x1b5 )
        {
            /* extention start code                   32 */
            GetChunk( &p_pack->bit_stream,
                      p_sout_buffer->p_buffer + i_pos, 4 ); i_pos += 4;

            if( ShowBits( &p_pack->bit_stream, 4 ) == 0x08 )
            {
                /* picture coding extention */

                /* extention start code identifier(b1000)  4 */
                /* f_code[2][2]                           16 */
                /* intra_dc_precision                      2 */
                /* picture_structure                       2 */
                /* top_field_first                         1 */
                i_picture_structure =
                    ShowBits( &p_pack->bit_stream, 24 ) & 0x03;
                i_top_field_first =
                    ShowBits( &p_pack->bit_stream, 25 ) & 0x01;
                i_repeat_first_field =
                    ShowBits( &p_pack->bit_stream, 31 ) & 0x01;

                GetChunk( &p_pack->bit_stream,
                          p_sout_buffer->p_buffer + i_pos, 4 ); i_pos += 4;

                i_progressive_frame =
                    ShowBits( &p_pack->bit_stream, 1 ) & 0x01;
            }
            CopyUntilNextStartCode( p_pack, p_sout_buffer, &i_pos );
        }
        else
        {
            if( i_code >= 0x101 && i_code <= 0x1af )
            {
                b_seen_slice = VLC_TRUE;
            }

            if( i_code == 0x1B3 )
            {
                p_pack->i_last_sequence_header = 0;
            }
            CopyUntilNextStartCode( p_pack, p_sout_buffer, &i_pos );
        }
    }

    if( i_pts <= 0 && i_dts <= 0 && p_pack->i_interpolated_dts <= 0 )
    {
        msg_Dbg( p_pack->p_fifo, "need a starting pts/dts" );
        sout_BufferDelete( p_pack->p_sout_input->p_sout, p_sout_buffer );
        return;
    }

    sout_BufferRealloc( p_pack->p_sout_input->p_sout,
                        p_sout_buffer, i_pos );
    p_sout_buffer->i_size = i_pos;

    /* calculate frame duration */
    if( p_pack->i_progressive_sequence || i_picture_structure == 0x03)
    {
        i_duration = (mtime_t)( 1000000 / p_pack->d_frame_rate );
    }
    else
    {
        i_duration = (mtime_t)( 1000000 / p_pack->d_frame_rate / 2);
    }

    if( p_pack->i_progressive_sequence )
    {
        if( i_top_field_first == 0 && i_repeat_first_field == 1 )
        {
            i_duration = 2 * i_duration;
        }
        else if( i_top_field_first == 1 && i_repeat_first_field == 1 )
        {
            i_duration = 3 * i_duration;
        }
    }
    else
    {
        if( i_picture_structure == 0x03 )
        {
            if( i_progressive_frame && i_repeat_first_field )
            {
                i_duration += i_duration / 2;
            }
        }
    }

    if( p_pack->i_low_delay || i_picture_coding_type == 0x03 )
    {
        /* Trivial case (DTS == PTS) */
        /* Correct interpolated dts when we receive a new pts/dts */
        if( i_pts > 0 ) p_pack->i_interpolated_dts = i_pts;
        if( i_dts > 0 ) p_pack->i_interpolated_dts = i_dts;
    }
    else
    {
        /* Correct interpolated dts when we receive a new pts/dts */
        if( p_pack->i_last_ref_pts )
            p_pack->i_interpolated_dts = p_pack->i_last_ref_pts;
        if( i_dts > 0 ) p_pack->i_interpolated_dts = i_dts;

        p_pack->i_last_ref_pts = i_pts;
    }

    /* Don't even try to calculate the PTS unless it is given in the
     * original stream */
    p_sout_buffer->i_pts = i_pts ? i_pts : -1;

    p_sout_buffer->i_dts = p_pack->i_interpolated_dts;

    if( p_pack->i_low_delay || i_picture_coding_type == 0x03 )
    {
        /* Trivial case (DTS == PTS) */
        p_pack->i_interpolated_dts += i_duration;
    }
    else
    {
        p_pack->i_interpolated_dts += p_pack->i_old_duration;
        p_pack->i_old_duration = i_duration;
    }

    p_sout_buffer->i_length = p_pack->i_interpolated_dts -
                                p_sout_buffer->i_dts;

    p_sout_buffer->i_bitrate = (int)( 8 * i_pos * p_pack->d_frame_rate );

#if 0
    msg_Dbg( p_pack->p_fifo, "------------> dts=%lld pts=%lld duration=%lld",
             p_sout_buffer->i_dts, p_sout_buffer->i_pts,
             p_sout_buffer->i_length );
#endif

    if ( p_pack->b_expect_discontinuity )
    {
        msg_Warn( p_pack->p_fifo, "discontinuity encountered, dropping a frame" );
        p_pack->b_expect_discontinuity = 0;
        sout_BufferDelete( p_pack->p_sout_input->p_sout, p_sout_buffer );
    }
    else
    {
        sout_InputSendBuffer( p_pack->p_sout_input, p_sout_buffer );
    }
}


/*****************************************************************************
 * EndThread : packetizer thread destruction
 *****************************************************************************/
static void EndThread ( packetizer_t *p_pack)
{
    if( p_pack->p_sout_input )
    {
        sout_InputDelete( p_pack->p_sout_input );
    }
}

/*****************************************************************************
 * BitstreamCallback: Import parameters from the new data/PES packet
 *****************************************************************************
 * This function is called by input's NextDataPacket.
 *****************************************************************************/
static void BitstreamCallback ( bit_stream_t * p_bit_stream,
                                vlc_bool_t b_new_pes )
{
    packetizer_t * p_pack = (packetizer_t *)p_bit_stream->p_callback_arg;

    if( p_bit_stream->p_data->b_discard_payload
            || (b_new_pes && p_bit_stream->p_pes->b_discontinuity) )
    {
        p_pack->b_expect_discontinuity = 1;
    }
}

