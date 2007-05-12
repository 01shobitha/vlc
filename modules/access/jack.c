/*****************************************************************************
 * jack.c: JACK audio input module
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * Copyright (C) 2007 Société des arts technologiques
 *
 * Authors: Arnaud Sala <arnaud.sala at savoirfairelinux.com>
 *          Julien Plissonneau Duquene <... at savoirfairelinux.com>
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

/**
 * \file modules/access/jack.c
 * \brief JACK audio input functions
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_vout.h>
#include <vlc_codecs.h>
#include <vlc_url.h>
#include <vlc_strings.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include <sys/mman.h> /* mlock() */
#include <errno.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Make VLC buffer audio data capturer from jack for the specified " \
    "length in milliseconds." )
#define PACE_TEXT N_( "Pace" )
#define PACE_LONGTEXT N_( \
    "Read the audio stream at VLC pace rather than Jack pace." )
#define AUTO_CONNECT_TEXT N_( "Auto Connection" )
#define AUTO_CONNECT_LONGTEXT N_( \
    "Automatically connect VLC input ports to available output ports." )

vlc_module_begin();
     set_description( _("JACK audio input") );
     set_capability( "access_demux", 0 );
     set_shortname( _( "JACK Input" ) );
     set_category( CAT_INPUT );
     set_subcategory( SUBCAT_INPUT_ACCESS );

     add_integer( "jack-input-caching", DEFAULT_PTS_DELAY / 1000, NULL,
         CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
     add_bool( "jack-input-use-vlc-pace", VLC_FALSE, NULL,
         PACE_TEXT, PACE_LONGTEXT, VLC_TRUE );
     add_bool( "jack-input-auto-connect", VLC_FALSE, NULL,
         PACE_TEXT, PACE_LONGTEXT, VLC_TRUE );

     add_shortcut( "jack" );
     set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

struct demux_sys_t
{
    /* Audio properties */
    vlc_fourcc_t                i_acodec_raw;
    unsigned int                i_channels;
    int                         i_sample_rate;
    int                         i_audio_max_frame_size;
    int                         i_frequency;
    block_t                     *p_block_audio;
    es_out_id_t                 *p_es_audio;
    date_t                      pts;

    /* Jack properties */
    jack_client_t               *p_jack_client;
    jack_port_t                 **pp_jack_port_input;
    jack_default_audio_sample_t **pp_jack_buffer;
    jack_ringbuffer_t           *p_jack_ringbuffer;
    jack_nframes_t              jack_buffer_size;
    jack_nframes_t              jack_sample_rate;
    size_t                      jack_sample_size;
};

static int Demux( demux_t * );
static int Control( demux_t *p_demux, int i_query, va_list args );

static void Parse ( demux_t * );
static int Process( jack_nframes_t i_frames, void *p_arg );

static block_t *GrabJack( demux_t * );

/*****************************************************************************
 * Open: Connect to the JACK server
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    unsigned int i;
    demux_t             *p_demux = ( demux_t* )p_this;
    demux_sys_t         *p_sys;
    es_format_t         fmt;
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    /* Allocate structure */
    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_demux, "out of memory, cannot allocate structure" );
        return VLC_ENOMEM;
    }
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    /* Parse MRL */
    Parse( p_demux );

    /* Create var */
    var_Create( p_demux, "jack-input-caching",
        VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_demux, "jack-input-use-vlc-pace",
        VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_demux, "jack-input-auto-connect",
        VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* JACK connexions */
    /* define name and connect to jack server */
    char p_vlc_client_name[32];
    sprintf( p_vlc_client_name, "vlc-input-%d", getpid() );
    p_sys->p_jack_client = jack_client_new( p_vlc_client_name );
    if( p_sys->p_jack_client == NULL )
    {
        msg_Err( p_demux, "failed to connect to JACK server" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* allocate input ports */
    if( p_sys->i_channels == 0 || p_sys->i_channels > 8 )
        p_sys->i_channels = 2 ; /* default number of port */
    p_sys->pp_jack_port_input = malloc(
        p_sys->i_channels * sizeof( jack_port_t* ) );
    if( p_sys->pp_jack_port_input == NULL )
    {
        msg_Err( p_demux, "out of memory, cannot allocate input ports" );
        return VLC_ENOMEM;
    }

    /* allocate ringbuffer */
    p_sys->p_jack_ringbuffer = jack_ringbuffer_create( p_sys->i_channels
        * jack_get_buffer_size( p_sys->p_jack_client )
        * sizeof( jack_default_audio_sample_t ) );
    if( p_sys->p_jack_ringbuffer == NULL )
    {
        msg_Err( p_demux, "out of memory, cannot allocate ringbuffer" );
        return VLC_ENOMEM;
    }

    /* register input ports */
    for( i = 0; i <  p_sys->i_channels; i++ )
    {
        char p_input_name[32];
        snprintf( p_input_name, 32, "vlc_in_%d", i+1 );
        p_sys->pp_jack_port_input[i] = jack_port_register(
            p_sys->p_jack_client, p_input_name, JACK_DEFAULT_AUDIO_TYPE,
            JackPortIsInput, 0 );
        if( p_sys->pp_jack_port_input[i] == NULL )
        {
            msg_Err( p_demux, "failed to register a JACK port" );
            if( p_sys->p_jack_client) jack_client_close( p_sys->p_jack_client );
            if( p_sys->pp_jack_port_input ) free( p_sys->pp_jack_port_input );
            if( p_sys->p_jack_ringbuffer ) jack_ringbuffer_free( p_sys->p_jack_ringbuffer );
            if( p_sys->pp_jack_buffer ) free( p_sys->pp_jack_buffer );
            free( p_sys );
            return VLC_EGENERIC;
        }
    }

    /* allocate buffer for input ports */
    p_sys->pp_jack_buffer = malloc ( p_sys->i_channels
        * sizeof( jack_default_audio_sample_t * ) );
    if( p_sys->pp_jack_buffer == NULL )
    {
        msg_Err( p_demux, "out of memory, cannot allocate input buffer" );
        return VLC_ENOMEM;
    }

    /* set process callback */
    jack_set_process_callback( p_sys->p_jack_client, Process, p_demux );

    /* tell jack server we are ready */
    if ( jack_activate( p_sys->p_jack_client ) )
    {
        msg_Err( p_demux, "failed to activate JACK client" );
        if( p_sys->p_jack_client) jack_client_close( p_sys->p_jack_client );
        if( p_sys->pp_jack_port_input ) free( p_sys->pp_jack_port_input );
        if( p_sys->p_jack_ringbuffer ) jack_ringbuffer_free( p_sys->p_jack_ringbuffer );
        if( p_sys->pp_jack_buffer ) free( p_sys->pp_jack_buffer );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* connect vlc input to jack output ports if requested */
    if( var_GetBool( p_demux, "jack-input-auto-connect" ) )
    {
        const char **pp_jack_port_output;
        int        i_out_ports = 0;
        int        i_in;
        int        j;
        pp_jack_port_output = jack_get_ports( p_sys->p_jack_client, NULL, NULL,
            JackPortIsOutput );

        while( pp_jack_port_output && pp_jack_port_output[i_out_ports] )
        {
            i_out_ports++;
        }
        if( i_out_ports > 0 )
        {
            for( j = 0; j < i_out_ports; j++ )
            {
                i_in = j % p_sys->i_channels;
                jack_connect( p_sys->p_jack_client, pp_jack_port_output[j],
                    jack_port_name( p_sys->pp_jack_port_input[i_in] ) );
            }
        }
        if( pp_jack_port_output ) free( pp_jack_port_output );
    }

    /* info about jack server */
    /* get buffers size */
    p_sys->jack_buffer_size = jack_get_buffer_size( p_sys->p_jack_client );
    /* get sample rate */
    p_sys->jack_sample_rate = jack_get_sample_rate( p_sys->p_jack_client );
    /* get sample size */
    p_sys->jack_sample_size = sizeof( jack_default_audio_sample_t );

    /* Define output format */
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'f','l','3','2' ) );
    fmt.audio.i_channels =  p_sys->i_channels;
    fmt.audio.i_rate =  p_sys->jack_sample_rate;
    fmt.audio.i_bitspersample =  p_sys->jack_sample_size * 8;
    fmt.audio.i_blockalign = fmt.audio.i_bitspersample / 8;
    fmt.i_bitrate = fmt.audio.i_rate * fmt.audio.i_bitspersample
        * fmt.audio.i_channels;

    p_sys->p_es_audio = es_out_Add( p_demux->out, &fmt );
    date_Init( &p_sys->pts, fmt.audio.i_rate, 1 );
    date_Set( &p_sys->pts, 1 );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Close: Disconnect from jack server and release associated resources
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t    *p_demux = ( demux_t* )p_this;
    demux_sys_t    *p_sys = p_demux->p_sys;

    msg_Dbg( p_demux,"Module unloaded" );
    if( p_sys->p_block_audio ) block_Release( p_sys->p_block_audio );
    if( p_sys->p_jack_client ) jack_client_close( p_sys->p_jack_client );
    if( p_sys->p_jack_ringbuffer ) jack_ringbuffer_free( p_sys->p_jack_ringbuffer );
    if( p_sys->pp_jack_port_input ) free( p_sys->pp_jack_port_input );
    if( p_sys->pp_jack_buffer ) free( p_sys->pp_jack_buffer );
    free( p_sys );
}


/*****************************************************************************
 * Control
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    vlc_bool_t  *pb;
    int64_t     *pi64;
    demux_sys_t *p_sys = p_demux->p_sys;

    switch( i_query )
    {
    /* Special for access_demux */
    case DEMUX_CAN_PAUSE:
    case DEMUX_SET_PAUSE_STATE:
        return VLC_SUCCESS;
    case DEMUX_CAN_CONTROL_PACE:
        pb = ( vlc_bool_t* )va_arg( args, vlc_bool_t * );
        *pb = var_GetBool( p_demux, "jack-input-use-vlc-pace" );
        return VLC_SUCCESS;

    case DEMUX_GET_PTS_DELAY:
        pi64 = ( int64_t* )va_arg( args, int64_t * );
        *pi64 = ( int64_t )var_GetInteger( p_demux, "jack-input-caching" )
            * 1000;
        return VLC_SUCCESS;

    case DEMUX_GET_TIME:
        pi64 = ( int64_t* )va_arg( args, int64_t * );
        *pi64 =  date_Get(&p_sys->pts);
            return VLC_SUCCESS;

    /* TODO implement others */
    default:
        return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}


/*****************************************************************************
 * Demux
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{

    demux_sys_t *p_sys;
    es_out_id_t  *p_es;
    block_t *p_block;

    p_sys = p_demux->p_sys;
    p_es = p_sys->p_es_audio;
    p_block = GrabJack( p_demux );

    if( p_block )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
        es_out_Send( p_demux->out, p_es, p_block );
    }

    return 1;
}


/*****************************************************************************
 * Process Callback : fill ringbuffer with Jack audio data
 *****************************************************************************/
int Process( jack_nframes_t i_frames, void *p_arg )
{
    demux_t            *p_demux = ( demux_t* )p_arg;
    demux_sys_t        *p_sys = p_demux->p_sys;
    unsigned int        i, j;
    size_t            i_write;

    /* Get and interlace buffers */
    for ( i = 0; i < p_sys->i_channels ; i++ )
    {
        p_sys->pp_jack_buffer[i] = jack_port_get_buffer(
            p_sys->pp_jack_port_input[i], i_frames );
    }

    /* fill ring buffer with signal */
    for( j = 0; j < p_sys->jack_buffer_size; j++ )
    {
        for( i = 0; i <p_sys->i_channels; i++ )
        {
            if( jack_ringbuffer_write_space( p_sys->p_jack_ringbuffer )
                < p_sys->jack_sample_size ) return 1; // buffer overflow
            i_write = jack_ringbuffer_write( p_sys->p_jack_ringbuffer,
                p_sys->pp_jack_buffer[i]+j, p_sys->jack_sample_size );
        }
    }

    return 1;
}


/*****************************************************************************
 * GrabJack: grab audio data in the Jack buffer
 *****************************************************************************/
static block_t *GrabJack( demux_t *p_demux )
{
    size_t      i_read;
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    /* read signal from ring buffer */
    i_read = jack_ringbuffer_read_space( p_sys->p_jack_ringbuffer );

    if( i_read < 100 ) /* avoid small read */
    {   /* vlc has too much free time on its hands? */
        msleep(1000);
        return NULL;
    }

    if( p_sys->p_block_audio )
    {
        p_block = p_sys->p_block_audio;
    }
    else
    {
        p_block = block_New( p_demux, i_read );
    }
    if( !p_block )
    {
        msg_Warn( p_demux, "cannot get block" );
        return 0;
    }

    p_sys->p_block_audio = p_block;
    p_block->i_buffer = i_read;
    p_sys->p_block_audio = 0;

    jack_ringbuffer_read( p_sys->p_jack_ringbuffer, p_block->p_buffer, i_read );

    p_block->i_dts = p_block->i_pts = date_Increment(
        &p_sys->pts, i_read/(p_sys->i_channels * p_sys->jack_sample_size) );

    return p_block;
}


/*****************************************************************************
 * Parse: Parse the MRL
 *****************************************************************************/
static void Parse( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    char *psz_dup = strdup( p_demux->psz_path );
    char *psz_parser = psz_dup;

    if( !strncmp( psz_parser, "channels=", strlen( "channels=" ) ) )
    {
        p_sys->i_channels = abs( strtol( psz_parser + strlen( "channels=" ),
            &psz_parser, 0 ) );
    }
    else
    {
        msg_Warn( p_demux, "unknown option" );
    }

    while( *psz_parser && *psz_parser != ':' )
    {
        psz_parser++;
    }

    if( *psz_parser == ':' )
    {
        for( ;; )
        {
            *psz_parser++ = '\0';
            if( !strncmp( psz_parser, "channels=", strlen( "channels=" ) ) )
            {
                p_sys->i_channels = abs( strtol(
                    psz_parser + strlen( "channels=" ), &psz_parser, 0 ) );
            }
            else
            {
                msg_Warn( p_demux, "unknown option" );
            }
            while( *psz_parser && *psz_parser != ':' )
            {
                psz_parser++;
            }

            if( *psz_parser == '\0' )
            {
                break;
            }
        }
    }
}

