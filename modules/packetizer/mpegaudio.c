/*****************************************************************************
 * mpegaudio.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mpegaudio.c,v 1.10 2003/09/10 22:59:55 fenrir Exp $
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
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static int  Run     ( decoder_fifo_t * );

vlc_module_begin();
    set_description( _("MPEG-I/II audio packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( Open, NULL );
vlc_module_end();

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

    mtime_t                 i_last_pts;
} packetizer_t;

static int  InitThread     ( packetizer_t * );
static void PacketizeThread( packetizer_t * );
static void EndThread      ( packetizer_t * );



static int mpegaudio_bitrate[2][3][16] =
{
  {
    /* v1 l1 */
    { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
    /* v1 l2 */
    { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
    /* v1 l3 */
    { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0}
  },
  {
     /* v2 l1 */
    { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
    /* v2 l2 */
    { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0},
    /* v2 l3 */
    { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0}
  }
};

static int mpegaudio_samplerate[2][4] = /* version 1 then 2 */
{
    { 44100, 48000, 32000, 0 },
    { 22050, 24000, 16000, 0 }
};


/*****************************************************************************
 * OpenDecoder: probe the packetizer and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC( 'm', 'p', 'g', 'a') )
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

    msg_Info( p_fifo, "Running mpegaudio packetizer" );

    p_pack = malloc( sizeof( packetizer_t ) );
    memset( p_pack, 0, sizeof( packetizer_t ) );

    p_pack->p_fifo = p_fifo;

    if( InitThread( p_pack ) != 0 )
    {
        DecoderError( p_fifo );
        return VLC_EGENERIC;
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

    free( p_pack );

    if( b_error )
    {
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}



/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/

static int InitThread( packetizer_t *p_pack )
{

    p_pack->output_format.i_cat = AUDIO_ES;
    p_pack->output_format.i_fourcc = p_pack->p_fifo->i_fourcc;
    p_pack->output_format.i_sample_rate = 0;
    p_pack->output_format.i_channels    = 0;
    p_pack->output_format.i_block_align = 0;
    p_pack->output_format.i_bitrate     = 0;
    p_pack->output_format.i_extra_data  = 0;
    p_pack->output_format.p_extra_data  = NULL;


    p_pack->p_sout_input = NULL;

    p_pack->i_last_pts = 0;

    if( InitBitstream( &p_pack->bit_stream, p_pack->p_fifo,
                       NULL, NULL ) != VLC_SUCCESS )
    {
        msg_Err( p_pack->p_fifo, "cannot initialize bitstream" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * PacketizeThread: packetize an unit (here copy a complete pes)
 *****************************************************************************/
static void PacketizeThread( packetizer_t *p_pack )
{
    sout_buffer_t   *p_sout_buffer;
    size_t          i_size;
    mtime_t         i_pts;

    uint32_t        i_sync, i_header;
    int             i_version, i_layer;
    int             i_channels, i_samplerate, i_bitrate;
    int             i_samplesperframe, i_framelength;
    int i_skip = 0;
    /* search a valid start code */
    for( ;; )
    {
        int i_crc, i_bitrate_index, i_samplerate_index;
        int i_padding, i_extention, i_mode, i_modeext, i_copyright;
        int i_original, i_emphasis;

        RealignBits( &p_pack->bit_stream );


        while( ShowBits( &p_pack->bit_stream, 11 ) != 0x07ff &&
               !p_pack->p_fifo->b_die && !p_pack->p_fifo->b_error )
        {
            //msg_Warn( p_pack->p_fifo, "trash..." );
            RemoveBits( &p_pack->bit_stream, 8 );
            i_skip++;
        }

        if( p_pack->p_fifo->b_die || p_pack->p_fifo->b_error )
        {
            return;
        }

        /* Set the Presentation Time Stamp */
        NextPTS( &p_pack->bit_stream, &i_pts, NULL );

        i_sync      = GetBits( &p_pack->bit_stream, 12 );
        i_header    = ShowBits( &p_pack->bit_stream, 20 );

        i_version   = 1 - GetBits( &p_pack->bit_stream, 1 );
        i_layer     = 3 - GetBits( &p_pack->bit_stream, 2 );
        i_crc       = 1 - GetBits( &p_pack->bit_stream, 1 );
        i_bitrate_index = GetBits( &p_pack->bit_stream, 4 );
        i_samplerate_index = GetBits( &p_pack->bit_stream, 2 );
        i_padding   = GetBits( &p_pack->bit_stream, 1 );
        i_extention = GetBits( &p_pack->bit_stream, 1 );
        i_mode      = GetBits( &p_pack->bit_stream, 2 );
        i_modeext   = GetBits( &p_pack->bit_stream, 2 );
        i_copyright = GetBits( &p_pack->bit_stream, 1 );
        i_original  = GetBits( &p_pack->bit_stream, 1 );
        i_emphasis  = GetBits( &p_pack->bit_stream, 2 );

        if( i_layer != 3 &&
            i_bitrate_index > 0x00 && i_bitrate_index < 0x0f &&
            i_samplerate_index != 0x03 &&
            i_emphasis != 0x02 )
        {
            i_channels = ( i_mode == 3 ) ? 1 : 2;
            i_bitrate = mpegaudio_bitrate[i_version][i_layer][i_bitrate_index];
            i_samplerate = mpegaudio_samplerate[i_version][i_samplerate_index];

            if( ( i_sync & 0x01 ) == 0x00 )
            {
                /* mpeg 2.5 */
                i_samplerate /= 2;
            }
            switch( i_layer )
            {
                case 0:
                    i_framelength = ( 12000 * i_bitrate /
                                      i_samplerate + i_padding ) * 4;
                    break;
                 case 1:
                    i_framelength = 144000 * i_bitrate /
                                      i_samplerate + i_padding;
                    break;
                 case 2:
                    i_framelength = ( i_version ? 72000 : 144000 ) *
                                    i_bitrate / i_samplerate + i_padding;
                    break;
                default:
                    i_framelength = 0;
            }
            switch( i_layer )
            {
                case 0:
                    i_samplesperframe = 384;
                    break;
                case 1:
                    i_samplesperframe = 1152;
                    break;
                case 2:
                    i_samplesperframe = i_version ? 576 : 1152;
                    break;
                default:
                    i_samplesperframe = 0;
            }
            break;
        }
    }

    if( !p_pack->p_sout_input )
    {
        /* add a input for the stream ouput */
        p_pack->output_format.i_sample_rate = i_samplerate;
        p_pack->output_format.i_channels    = i_channels;
        p_pack->output_format.i_block_align = 1;
        p_pack->output_format.i_bitrate     = i_bitrate*1000;

        p_pack->p_sout_input =
            sout_InputNew( p_pack->p_fifo,
                           &p_pack->output_format );

        if( !p_pack->p_sout_input )
        {
            msg_Err( p_pack->p_fifo,
                     "cannot add a new stream" );
            p_pack->p_fifo->b_error = 1;
            return;
        }
        msg_Dbg( p_pack->p_fifo,
                 "v:%d l:%d channels:%d samplerate:%d bitrate:%d size:%d",
                 i_version, i_layer, i_channels, i_samplerate,
                 i_bitrate, i_framelength );
    }

    if( i_pts <= 0 && p_pack->i_last_pts <= 0 )
    {
        msg_Dbg( p_pack->p_fifo, "need a starting pts" );
        return;
    }
    if( i_pts <= 0 )
    {
        i_pts = p_pack->i_last_pts +
            (uint64_t)1000000 *
            (uint64_t)i_samplesperframe /
            (uint64_t)i_samplerate;
    }
    p_pack->i_last_pts = i_pts;

    i_size = __MAX( i_framelength, 4 );
//    msg_Dbg( p_pack->p_fifo, "frame length %d b", i_size );
    p_sout_buffer =
        sout_BufferNew( p_pack->p_sout_input->p_sout, i_size );
    if( !p_sout_buffer )
    {
        p_pack->p_fifo->b_error = 1;
        return;
    }
    p_sout_buffer->p_buffer[0] = ( i_sync >> 4 )&0xff;
    p_sout_buffer->p_buffer[1] =
        ( ( i_sync << 4 )&0xf0 ) | ( ( i_header >> 16 )&0x0f );
    p_sout_buffer->p_buffer[2] = ( i_header >> 8 )&0xff;
    p_sout_buffer->p_buffer[3] = ( i_header      )&0xff;
    p_sout_buffer->i_bitrate = i_bitrate;

    p_sout_buffer->i_dts = i_pts;
    p_sout_buffer->i_pts = i_pts;

    p_sout_buffer->i_length =
            (uint64_t)1000000 *
            (uint64_t)i_samplesperframe /
            (uint64_t)i_samplerate;

    /* we are already aligned */
    GetChunk( &p_pack->bit_stream,
              p_sout_buffer->p_buffer + 4,
              i_size - 4 );

    sout_InputSendBuffer( p_pack->p_sout_input,
                          p_sout_buffer );
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
