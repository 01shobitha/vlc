/*****************************************************************************
 * spudec.c : SPU decoder thread
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: spudec.c,v 1.25 2003/09/02 20:19:26 gbazin Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#include "spudec.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );      
static int  RunDecoder    ( decoder_fifo_t * );
static int  InitThread    ( spudec_thread_t * );
static void EndThread     ( spudec_thread_t * );
static vout_thread_t *FindVout( spudec_thread_t *);

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin();
    set_description( _("DVD subtitles decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able 
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC('s','p','u',' ')
         && p_dec->p_fifo->i_fourcc != VLC_FOURCC('s','p','u','b') )
    {   
        return VLC_EGENERIC;
    }
    
    p_dec->pf_run = RunDecoder;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t * p_fifo )
{
    spudec_thread_t *     p_spudec;

    /* Allocate the memory needed to store the thread's structure */
    p_spudec = (spudec_thread_t *)malloc( sizeof(spudec_thread_t) );

    if ( p_spudec == NULL )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    
    /*
     * Initialize the thread properties
     */
    p_spudec->p_vout = NULL;
    p_spudec->p_fifo = p_fifo;
        
    /*
     * Initialize thread and free configuration
     */
    p_spudec->p_fifo->b_error = InitThread( p_spudec );

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_spudec->p_fifo->b_die) && (!p_spudec->p_fifo->b_error) )
    {
        if( E_(SyncPacket)( p_spudec ) )
        {
            continue;
        }

        /* Find/Wait for a video output */
        p_spudec->p_vout = FindVout( p_spudec );
        if( p_spudec->p_vout)
        {
            E_(ParsePacket)( p_spudec );
            vlc_object_release( p_spudec->p_vout );
        }
    }

    /*
     * Error loop
     */
    if( p_spudec->p_fifo->b_error )
    {
        DecoderError( p_spudec->p_fifo );

        /* End of thread */
        EndThread( p_spudec );
        return -1;
    }

    /* End of thread */
    EndThread( p_spudec );
    return 0;
}

/* following functions are local */

/*****************************************************************************
 * InitThread: initialize spu decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
static int InitThread( spudec_thread_t *p_spudec )
{
    int i_ret;
    
    /* Call InitBitstream anyway so p_spudec->bit_stream is in a known
     * state before calling CloseBitstream */
    i_ret = InitBitstream( &p_spudec->bit_stream, p_spudec->p_fifo,
                            NULL, NULL );

    /* Check for a video output */
    p_spudec->p_vout = FindVout( p_spudec );
    
    if( !p_spudec->p_vout )
    {
        return -1;
    }
    
    /* It was just a check */
    vlc_object_release( p_spudec->p_vout );
    p_spudec->p_vout = NULL;
    
    return i_ret;
}

/*****************************************************************************
 * FindVout: Find a vout or wait for one to be created.
 *****************************************************************************/
static vout_thread_t *FindVout( spudec_thread_t *p_spudec )
{
    vout_thread_t *p_vout = NULL;
    
    /* Find an available video output */
    do
    {
        if( p_spudec->p_fifo->b_die || p_spudec->p_fifo->b_error )
        {
            break;
        }

        p_vout = vlc_object_find( p_spudec->p_fifo, VLC_OBJECT_VOUT,
                                                              FIND_ANYWHERE );

        if( p_vout )
        {
            break;
        }

        msleep( VOUT_OUTMEM_SLEEP );
    }
    while( 1 );

    return p_vout;
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( spudec_thread_t *p_spudec )
{
    if( p_spudec->p_vout != NULL 
         && p_spudec->p_vout->p_subpicture != NULL )
    {
        subpicture_t *  p_subpic;
        int             i_subpic;
    
        for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
        {
            p_subpic = &p_spudec->p_vout->p_subpicture[i_subpic];

            if( p_subpic != NULL &&
              ( ( p_subpic->i_status == RESERVED_SUBPICTURE )
             || ( p_subpic->i_status == READY_SUBPICTURE ) ) )
            {
                vout_DestroySubPicture( p_spudec->p_vout, p_subpic );
            }
        }
    }
    
    CloseBitstream( &p_spudec->bit_stream );
    free( p_spudec );
}
