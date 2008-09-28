/*****************************************************************************
 * input_clock.c: Clock/System date convertions, stream management
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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
#include <vlc_input.h>
#include "input_clock.h"

/* TODO:
 * - clean up locking once clock code is stable
 *
 */

/*
 * DISCUSSION : SYNCHRONIZATION METHOD
 *
 * In some cases we can impose the pace of reading (when reading from a
 * file or a pipe), and for the synchronization we simply sleep() until
 * it is time to deliver the packet to the decoders. When reading from
 * the network, we must be read at the same pace as the server writes,
 * otherwise the kernel's buffer will trash packets. The risk is now to
 * overflow the input buffers in case the server goes too fast, that is
 * why we do these calculations :
 *
 * We compute a mean for the pcr because we want to eliminate the
 * network jitter and keep the low frequency variations. The mean is
 * in fact a low pass filter and the jitter is a high frequency signal
 * that is why it is eliminated by the filter/average.
 *
 * The low frequency variations enable us to synchronize the client clock
 * with the server clock because they represent the time variation between
 * the 2 clocks. Those variations (ie the filtered pcr) are used to compute
 * the presentation dates for the audio and video frames. With those dates
 * we can decode (or trash) the MPEG2 stream at "exactly" the same rate
 * as it is sent by the server and so we keep the synchronization between
 * the server and the client.
 *
 * It is a very important matter if you want to avoid underflow or overflow
 * in all the FIFOs, but it may be not enough.
 */

/* p_input->p->i_cr_average : Maximum number of samples used to compute the
 * dynamic average value.
 * We use the following formula :
 * new_average = (old_average * c_average + new_sample_value) / (c_average +1)
 */


/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Maximum gap allowed between two CRs. */
#define CR_MAX_GAP (INT64_C(2000000)*100/9)

/* Latency introduced on DVDs with CR == 0 on chapter change - this is from
 * my dice --Meuuh */
#define CR_MEAN_PTS_GAP (300000)

/*****************************************************************************
 * Structures
 *****************************************************************************/

/**
 * This structure holds long term average
 */
typedef struct
{
    mtime_t i_value;
    int     i_residue;

    int     i_count;
    int     i_divider;
} average_t;
static void    AvgInit( average_t *, int i_divider );
static void    AvgClean( average_t * );

static void    AvgReset( average_t * );
static void    AvgUpdate( average_t *, mtime_t i_value );
static mtime_t AvgGet( average_t * );

/* */
typedef struct
{
    mtime_t i_stream;
    mtime_t i_system;
} clock_point_t;

static inline clock_point_t clock_point_Create( mtime_t i_stream, mtime_t i_system )
{
    clock_point_t p = { .i_stream = i_stream, .i_system = i_system };
    return p;
}

/* */
struct input_clock_t
{
    /* */
    vlc_mutex_t lock;

    /* Reference point */
    bool          b_has_reference;
    clock_point_t ref;

    /* Last point
     * It is used to detect unexpected stream discontinuities */
    clock_point_t last;

    /* Maximal timestamp returned by input_clock_GetTS (in system unit) */
    mtime_t i_ts_max;

    /* Clock drift */
    mtime_t i_next_drift_update;
    average_t drift;

    /* Current modifiers */
    int     i_rate;
};

static mtime_t ClockStreamToSystem( input_clock_t *, mtime_t i_stream );
static mtime_t ClockSystemToStream( input_clock_t *, mtime_t i_system );

/*****************************************************************************
 * input_clock_New: create a new clock
 *****************************************************************************/
input_clock_t *input_clock_New( int i_cr_average, int i_rate )
{
    input_clock_t *cl = malloc( sizeof(*cl) );
    if( !cl )
        return NULL;

    vlc_mutex_init( &cl->lock );
    cl->b_has_reference = false;
    cl->ref = clock_point_Create( 0, 0 );

    cl->last = clock_point_Create( 0, 0 );

    cl->i_ts_max = 0;

    cl->i_next_drift_update = 0;
    AvgInit( &cl->drift, i_cr_average );

    cl->i_rate = i_rate;

    return cl;
}

/*****************************************************************************
 * input_clock_Delete: destroy a new clock
 *****************************************************************************/
void input_clock_Delete( input_clock_t *cl )
{
    AvgClean( &cl->drift );
    vlc_mutex_destroy( &cl->lock );
    free( cl );
}

/*****************************************************************************
 * input_clock_Update: manages a clock reference
 *
 *  i_ck_stream: date in stream clock
 *  i_ck_system: date in system clock
 *****************************************************************************/
void input_clock_Update( input_clock_t *cl,
                         vlc_object_t *p_log, bool b_can_pace_control,
                         mtime_t i_ck_stream, mtime_t i_ck_system )
{
    bool b_reset_reference = false;

    vlc_mutex_lock( &cl->lock );
    if( ( !cl->b_has_reference ) ||
        ( i_ck_stream == 0 && cl->last.i_stream != 0 ) )
    {
        /* */
        b_reset_reference= true;
    }
    else if( cl->last.i_stream != 0 &&
             ( (cl->last.i_stream - i_ck_stream) > CR_MAX_GAP ||
               (cl->last.i_stream - i_ck_stream) < -CR_MAX_GAP ) )
    {
        /* Stream discontinuity, for which we haven't received a
         * warning from the stream control facilities (dd-edited
         * stream ?). */
        msg_Warn( p_log, "clock gap, unexpected stream discontinuity" );
        cl->i_ts_max = 0;

        /* */
        msg_Warn( p_log, "feeding synchro with a new reference point trying to recover from clock gap" );
        b_reset_reference= true;
    }
    if( b_reset_reference )
    {
        cl->i_next_drift_update = 0;
        AvgReset( &cl->drift );

        /* Feed synchro with a new reference point. */
        cl->b_has_reference = true;
        cl->ref = clock_point_Create( i_ck_stream,
                                      __MAX( cl->i_ts_max + CR_MEAN_PTS_GAP, i_ck_system ) );
    }

    if( !b_can_pace_control && cl->i_next_drift_update < i_ck_system )
    {
        const mtime_t i_converted = ClockSystemToStream( cl, i_ck_system );

        AvgUpdate( &cl->drift, i_converted - i_ck_stream );

        cl->i_next_drift_update = i_ck_system + CLOCK_FREQ/5; /* FIXME why that */
    }
    cl->last = clock_point_Create( i_ck_stream, i_ck_system );

    vlc_mutex_unlock( &cl->lock );
}

/*****************************************************************************
 * input_clock_Reset:
 *****************************************************************************/
void input_clock_Reset( input_clock_t *cl )
{
    vlc_mutex_lock( &cl->lock );

    cl->b_has_reference = false;
    cl->ref = clock_point_Create( 0, 0 );
    cl->i_ts_max = 0;

    vlc_mutex_unlock( &cl->lock );
}

/*****************************************************************************
 * input_clock_ChangeRate:
 *****************************************************************************/
void input_clock_ChangeRate( input_clock_t *cl, int i_rate )
{
    vlc_mutex_lock( &cl->lock );

    /* Move the reference point */
    if( cl->b_has_reference )
        cl->ref = cl->last;

    cl->i_rate = i_rate;

    vlc_mutex_unlock( &cl->lock );
}

/*****************************************************************************
 * input_clock_GetWakeup
 *****************************************************************************/
mtime_t input_clock_GetWakeup( input_clock_t *cl )
{
    mtime_t i_wakeup = 0;

    vlc_mutex_lock( &cl->lock );

    /* Synchronized, we can wait */
    if( cl->b_has_reference )
        i_wakeup = ClockStreamToSystem( cl, cl->last.i_stream );

    vlc_mutex_unlock( &cl->lock );

    return i_wakeup;
}

/*****************************************************************************
 * input_clock_GetTS: manages a PTS or DTS
 *****************************************************************************/
mtime_t input_clock_GetTS( input_clock_t *cl,
                           mtime_t i_pts_delay, mtime_t i_ts )
{
    mtime_t i_converted_ts;

    vlc_mutex_lock( &cl->lock );

    if( !cl->b_has_reference )
    {
        vlc_mutex_unlock( &cl->lock );
        return 0;
    }

    /* */
    i_converted_ts = ClockStreamToSystem( cl, i_ts + AvgGet( &cl->drift ) );
    if( i_converted_ts > cl->i_ts_max )
        cl->i_ts_max = i_converted_ts;

    vlc_mutex_unlock( &cl->lock );

    return i_converted_ts + i_pts_delay;
}
/*****************************************************************************
 * input_clock_GetRate: Return current rate
 *****************************************************************************/
int input_clock_GetRate( input_clock_t *cl )
{
    int i_rate;

    vlc_mutex_lock( &cl->lock );
    i_rate = cl->i_rate;
    vlc_mutex_unlock( &cl->lock );

    return i_rate;
}

/*****************************************************************************
 * ClockStreamToSystem: converts a movie clock to system date
 *****************************************************************************/
static mtime_t ClockStreamToSystem( input_clock_t *cl, mtime_t i_stream )
{
    if( !cl->b_has_reference )
        return 0;

    return ( i_stream - cl->ref.i_stream ) * cl->i_rate / INPUT_RATE_DEFAULT +
           cl->ref.i_system;
}

/*****************************************************************************
 * ClockSystemToStream: converts a system date to movie clock
 *****************************************************************************
 * Caution : a valid reference point is needed for this to operate.
 *****************************************************************************/
static mtime_t ClockSystemToStream( input_clock_t *cl, mtime_t i_system )
{
    assert( cl->b_has_reference );
    return ( i_system - cl->ref.i_system ) * INPUT_RATE_DEFAULT / cl->i_rate +
            cl->ref.i_stream;
}

/*****************************************************************************
 * Long term average helpers
 *****************************************************************************/
static void AvgInit( average_t *p_avg, int i_divider )
{
    p_avg->i_divider = i_divider;
    AvgReset( p_avg );
}
static void AvgClean( average_t *p_avg )
{
    VLC_UNUSED(p_avg);
}
static void AvgReset( average_t *p_avg )
{
    p_avg->i_value = 0;
    p_avg->i_residue = 0;
    p_avg->i_count = 0;
}
static void AvgUpdate( average_t *p_avg, mtime_t i_value )
{
    const int i_f0 = __MIN( p_avg->i_divider - 1, p_avg->i_count );
    const int i_f1 = p_avg->i_divider - i_f0;

    const mtime_t i_tmp = i_f0 * p_avg->i_value + i_f1 * i_value + p_avg->i_residue;

    p_avg->i_value   = i_tmp / p_avg->i_divider;
    p_avg->i_residue = i_tmp % p_avg->i_divider;

    p_avg->i_count++;
}
static mtime_t AvgGet( average_t *p_avg )
{
    return p_avg->i_value;
}

