/*****************************************************************************
 * input_netlist.c: netlist management
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: input_netlist.c,v 1.20 2000/12/21 13:07:45 massiot Exp $
 *
 * Authors: Henri Fallon <henri@videolan.org>
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
#include "defs.h"

#include <stdlib.h>
#include <sys/uio.h>                                         /* struct iovec */

#include "config.h"
#include "common.h"
#include "threads.h"                                                /* mutex */
#include "mtime.h"
#include "intf_msg.h"                                           /* intf_*Msg */

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input_netlist.h"
#include "input.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/*****************************************************************************
 * input_NetlistInit: allocates netlist buffers and init indexes
 *****************************************************************************/
int input_NetlistInit( input_thread_t * p_input, int i_nb_data, int i_nb_pes,
                       size_t i_buffer_size )
{
    unsigned int i_loop;
    netlist_t * p_netlist;

    /* First we allocate and initialise our netlist struct */
    p_input->p_method_data = malloc(sizeof(netlist_t));
    if ( p_input->p_method_data == NULL )
    {
        intf_ErrMsg("Unable to malloc the netlist struct\n");
        return (-1);
    }
    
    p_netlist = (netlist_t *) p_input->p_method_data;
    
    /* allocate the buffers */ 
    p_netlist->p_buffers = 
        (byte_t *) malloc(i_buffer_size* i_nb_data );
    if ( p_netlist->p_buffers == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (1)\n");
        return (-1);
    }
    
    p_netlist->p_data = 
        (data_packet_t *) malloc(sizeof(data_packet_t)*(i_nb_data));
    if ( p_netlist->p_data == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (2)\n");
        return (-1);
    }
    
    p_netlist->p_pes = 
        (pes_packet_t *) malloc(sizeof(pes_packet_t)*(i_nb_pes));
    if ( p_netlist->p_pes == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (3)\n");
        return (-1);
    }
    
    /* allocate the FIFOs */
    p_netlist->pp_free_data = 
        (data_packet_t **) malloc (i_nb_data * sizeof(data_packet_t *) );
    if ( p_netlist->pp_free_data == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (4)\n");
    }
    p_netlist->pp_free_pes = 
        (pes_packet_t **) malloc (i_nb_pes * sizeof(pes_packet_t *) );
    if ( p_netlist->pp_free_pes == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (5)\n");
    }
    
    p_netlist->p_free_iovec = ( struct iovec * )
        malloc( (i_nb_data + INPUT_READ_ONCE) * sizeof(struct iovec) );
    if ( p_netlist->p_free_iovec == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (6)\n");
    }
    
    /* Fill the data FIFO */
    for ( i_loop = 0; i_loop < i_nb_data; i_loop++ )
    {
        p_netlist->pp_free_data[i_loop] = 
            p_netlist->p_data + i_loop;

        p_netlist->pp_free_data[i_loop]->p_buffer = 
            p_netlist->p_buffers + i_loop * i_buffer_size;
        
        //peut-�tre pas n�cessaire ici vu qu'on le fera � chaque fois
        //dans NewPacket et Getiovec
        p_netlist->pp_free_data[i_loop]->p_payload_start = 
            p_netlist->pp_free_data[i_loop]->p_buffer;

        p_netlist->pp_free_data[i_loop]->p_payload_end =
            p_netlist->pp_free_data[i_loop]->p_buffer + i_buffer_size;
    }
    /* Fill the PES FIFO */
    for ( i_loop = 0; i_loop < i_nb_pes ; i_loop++ )
    {
        p_netlist->pp_free_pes[i_loop] = 
            p_netlist->p_pes + i_loop;
    }
   
    /* Deal with the iovec */
    for ( i_loop = 0; i_loop < i_nb_data; i_loop++ )
    {
        p_netlist->p_free_iovec[i_loop].iov_base = 
            p_netlist->p_buffers + i_loop * i_buffer_size;
   
        p_netlist->p_free_iovec[i_loop].iov_len = i_buffer_size;
    }
    
    /* vlc_mutex_init */
    vlc_mutex_init (&p_netlist->lock);
    
    /* initialize indexes */
    p_netlist->i_data_start = 0;
    p_netlist->i_data_end = i_nb_data - 1;

    //INPUT_READ_ONCE en trop
    p_netlist->i_pes_start = 0;
    p_netlist->i_pes_end = i_nb_pes + INPUT_READ_ONCE - 1;

    //inutiles, en fait toujours strictement == � i_data_start (les deux
    //pointent vers le m�me buffer donc il faut garder une synchronisation)
    p_netlist->i_iovec_start = 0;
    p_netlist->i_iovec_end = i_nb_data - 1;
    
    p_netlist->i_nb_data = i_nb_data;
    p_netlist->i_nb_pes = i_nb_pes;
    p_netlist->i_buffer_size = i_buffer_size;

    return (0); /* Everything went all right */
}

/*****************************************************************************
 * input_NetlistGetiovec: returns an iovec pointer for a readv() operation
 *****************************************************************************/
struct iovec * input_NetlistGetiovec( void * p_method_data )
{
    netlist_t * p_netlist;

    /* cast */
    p_netlist = ( netlist_t * ) p_method_data;
    
    /* check */
    //v�rifier que -truc % bidule fait bien ce qu'il faut (me souviens plus
    //de la d�finition de % sur -N)
    if ( 
     (p_netlist->i_iovec_end - p_netlist->i_iovec_start)%p_netlist->i_nb_data 
     < INPUT_READ_ONCE )
    {
        intf_ErrMsg("Empty iovec FIFO. Unable to allocate memory\n");
        return (NULL);
    }

    /* readv only takes contiguous buffers */
    if( p_netlist->i_nb_data - p_netlist->i_iovec_start < INPUT_READ_ONCE )
        memcpy( &p_netlist->p_free_iovec[p_netlist->i_nb_data], 
                p_netlist->p_free_iovec, 
                INPUT_READ_ONCE-(p_netlist->i_nb_data-p_netlist->i_iovec_start)
              );
    //manque un sizeof() dans le memcpy

    //pas tout de suite (->dans Mviovec), parce que readv ne va pas
    //_n�cessairement_ prendre tous les iovec disponibles (cf. man readv)
    p_netlist->i_iovec_start += INPUT_READ_ONCE;
    p_netlist->i_iovec_start %= p_netlist->i_nb_data;

    //il faudrait aussi initialiser les data_packet_t correspondants,
    //comme dans NewPacket

    return &p_netlist->p_free_iovec[p_netlist->i_iovec_start];
}

/*****************************************************************************
 * input_NetlistMviovec: move the iovec pointer after a readv() operation
 *****************************************************************************/
void input_NetlistMviovec( void * p_method_data, size_t i_nb_iovec )
{
    netlist_t * p_netlist;

    /* cast */
    p_netlist = (netlist_t *) p_method_data;
    
    //remplacer i_iovec_start par i_data_start, en fait c'est la m�me
    //chose.
    //il manque un lock
    p_netlist->i_iovec_start += i_nb_iovec;
    p_netlist->i_iovec_start %= p_netlist->i_nb_data;
}

/*****************************************************************************
 * input_NetlistNewPacket: returns a free data_packet_t
 *****************************************************************************/
struct data_packet_s * input_NetlistNewPacket( void * p_method_data,
                                               size_t i_buffer_size )
{    
    netlist_t * p_netlist; 
    struct data_packet_s * p_return;
    
    /* cast */
    p_netlist = ( netlist_t * ) p_method_data; 

#ifdef DEBUG
    if( i_buffer_size > p_netlist->i_buffer_size )
    {
        /* This should not happen */
        intf_ErrMsg( "Netlist packet too small !" );
        return NULL;
    }
#endif

    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );
        
    /* check */
    if ( p_netlist->i_data_start == p_netlist->i_data_end )
    {
        intf_ErrMsg("Empty Data FIFO in netlist. Unable to allocate memory\n");
        return ( NULL );
    }
    
    p_return = (p_netlist->pp_free_data[p_netlist->i_data_start]);
    p_netlist->i_data_start++;
    p_netlist->i_data_start %= p_netlist->i_nb_data;

    //on vire aussi, forc�ment
    p_netlist->i_iovec_start++; 
    p_netlist->i_iovec_start %= p_netlist->i_nb_data;

    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);

    if (i_buffer_size < p_netlist->i_buffer_size) 
    {
        p_return->p_payload_end = p_return->p_payload_start + i_buffer_size;
    }
   
    /* initialize data */
    p_return->p_next = NULL;
    p_return->b_discard_payload = 0;
    //p_payload_start = ..., p_payload_end = ... (risque d'�tre modifi�
    //� tout moment par l'input et les d�codeurs, donc on ne peut rien
    //supposer...)
    return ( p_return );
}

/*****************************************************************************
 * input_NetlistNewPES: returns a free pes_packet_t
 *****************************************************************************/
struct pes_packet_s * input_NetlistNewPES( void * p_method_data )
{
    netlist_t * p_netlist;
    pes_packet_t * p_return;
    
    /* cast */ 
    p_netlist = (netlist_t *) p_method_data;
    
    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );
    
    /* check */
    if ( p_netlist->i_pes_start == p_netlist->i_pes_end )
    {
        intf_ErrMsg("Empty PES FIFO in netlist - Unable to allocate memory\n");
        return ( NULL );
    }

    /* allocate */
    p_return = p_netlist->pp_free_pes[p_netlist->i_pes_start];
    p_netlist->i_pes_start++;
    p_netlist->i_pes_start %= p_netlist->i_nb_pes; 
   
    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);
    
    /* initialize PES */
    p_return->b_messed_up = 
        p_return->b_data_alignment = 
        p_return->b_discontinuity = 
        p_return->b_has_pts = 0;
    p_return->i_pes_size = 0;
    p_return->p_first = NULL;
   
    return ( p_return );
}

/*****************************************************************************
 * input_NetlistDeletePacket: puts a data_packet_t back into the netlist
 *****************************************************************************/
void input_NetlistDeletePacket( void * p_method_data, data_packet_t * p_data )
{
    netlist_t * p_netlist;
    
    /* cast */
    p_netlist = (netlist_t *) p_method_data;

    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );

    /* Delete data_packet */
    p_netlist->i_data_end ++;
    p_netlist->i_data_end %= p_netlist->i_nb_data;
    p_netlist->pp_free_data[p_netlist->i_data_end] = p_data;
    
    /* Delete the corresponding iovec */
    p_netlist->i_iovec_end++; 
    p_netlist->i_iovec_end %= p_netlist->i_nb_data;
    p_netlist->p_free_iovec[p_netlist->i_iovec_end].iov_base = 
        p_data->p_buffer;
    
    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);    
}

/*****************************************************************************
 * input_NetlistDeletePES: puts a pes_packet_t back into the netlist
 *****************************************************************************/
void input_NetlistDeletePES( void * p_method_data, pes_packet_t * p_pes )
{
    netlist_t * p_netlist; 
    data_packet_t * p_current_packet;
    
    /* cast */
    p_netlist = (netlist_t *)p_method_data;

    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );

    /* delete free  p_pes->p_first, p_next ... */
    p_current_packet = p_pes->p_first;
    while ( p_current_packet != NULL )
    {
        /* copy of NetListDeletePacket, duplicate code avoid many locks */

        p_netlist->i_data_end ++;
        p_netlist->i_data_end %= p_netlist->i_nb_data;
        p_netlist->pp_free_data[p_netlist->i_data_end] = p_current_packet;
        
        p_netlist->i_iovec_end++; 
        p_netlist->i_iovec_end %= p_netlist->i_nb_data;
        p_netlist->p_free_iovec[p_netlist->i_iovec_end].iov_base = 
            p_netlist->p_data->p_buffer;
    
        p_current_packet = p_current_packet->p_next;
    }
 
    /* delete our current PES packet */
    p_netlist->i_pes_end ++;
    p_netlist->i_pes_end %= p_netlist->i_nb_pes;
    p_netlist->pp_free_pes[p_netlist->i_pes_end] = p_pes;
    
    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);
}

/*****************************************************************************
 * input_NetlistEnd: frees all allocated structures
 *****************************************************************************/
void input_NetlistEnd( input_thread_t * p_input)
{
    netlist_t * p_netlist;

    /* cast */
    p_netlist = ( netlist_t * ) p_input->p_method_data;
    
    /* free the FIFO, the buffer, and the netlist structure */
    free (p_netlist->pp_free_data);
    free (p_netlist->pp_free_pes);
    free (p_netlist->p_pes);
    free (p_netlist->p_data);
    //et p_buffers il pue ?

    /* free the netlist */
    free (p_netlist);
}
