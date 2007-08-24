/*****************************************************************************
 * rtsp.c: RTSP support for RTP stream output module
 *****************************************************************************
 * Copyright (C) 2003-2007 the VideoLAN team
 * $Id: rtp.c 21407 2007-08-22 20:10:41Z courmisch $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc/vlc.h>
#include <vlc_sout.h>

#include <vlc_httpd.h>
#include <vlc_url.h>
#include <vlc_network.h>
#include <assert.h>

#include "rtp.h"

typedef struct rtsp_session_t rtsp_session_t;

struct rtsp_stream_t
{
    vlc_mutex_t     lock;
    sout_stream_t  *owner;
    httpd_host_t   *host;
    httpd_url_t    *url;
    char           *psz_control;
    char           *psz_path;

    int             sessionc;
    rtsp_session_t **sessionv;
};


static int  RtspCallback( httpd_callback_sys_t *p_args,
                          httpd_client_t *cl,
                          httpd_message_t *answer, httpd_message_t *query );
static int  RtspCallbackId( httpd_callback_sys_t *p_args,
                            httpd_client_t *cl,
                            httpd_message_t *answer, httpd_message_t *query );
static void RtspClientDel( rtsp_stream_t *rtsp, rtsp_session_t *session );

rtsp_stream_t *RtspSetup( sout_stream_t *p_stream, const vlc_url_t *url )
{
    rtsp_stream_t *rtsp = malloc( sizeof( *rtsp ) );

    if( rtsp == NULL )
        return NULL;

    rtsp->owner = p_stream;
    rtsp->sessionc = 0;
    rtsp->sessionv = NULL;
    vlc_mutex_init( p_stream, &rtsp->lock );

    msg_Dbg( p_stream, "rtsp setup: %s : %d / %s\n",
             url->psz_host, url->i_port, url->psz_path );

    rtsp->psz_path = strdup( url->psz_path ? url->psz_path : "/" );
    if( rtsp->psz_path == NULL )
        goto error;

    if( asprintf( &rtsp->psz_control, "rtsp://%s:%d%s",
                  url->psz_host,  url->i_port > 0 ? url->i_port : 554,
                  rtsp->psz_path ) == -1 )
    {
        rtsp->psz_control = NULL;
        goto error;
    }

    rtsp->host = httpd_HostNew( VLC_OBJECT(p_stream), url->psz_host,
                                url->i_port > 0 ? url->i_port : 554 );
    if( rtsp->host == NULL )
        goto error;

    rtsp->url = httpd_UrlNewUnique( rtsp->host, rtsp->psz_path, NULL, NULL,
                                    NULL );
    if( rtsp->url == NULL )
        goto error;

    httpd_UrlCatch( rtsp->url, HTTPD_MSG_DESCRIBE, RtspCallback, (void*)rtsp );
    httpd_UrlCatch( rtsp->url, HTTPD_MSG_SETUP,    RtspCallback, (void*)rtsp );
    httpd_UrlCatch( rtsp->url, HTTPD_MSG_PLAY,     RtspCallback, (void*)rtsp );
    httpd_UrlCatch( rtsp->url, HTTPD_MSG_PAUSE,    RtspCallback, (void*)rtsp );
    httpd_UrlCatch( rtsp->url, HTTPD_MSG_TEARDOWN, RtspCallback, (void*)rtsp );
    return rtsp;

error:
    RtspUnsetup( rtsp );
    return NULL;
}


void RtspUnsetup( rtsp_stream_t *rtsp )
{
    while( rtsp->sessionc > 0 )
        RtspClientDel( rtsp, rtsp->sessionv[0] );

    if( rtsp->url )
        httpd_UrlDelete( rtsp->url );

    if( rtsp->host )
        httpd_HostDelete( rtsp->host );

    vlc_mutex_destroy( &rtsp->lock );
}


struct rtsp_stream_id_t
{
    rtsp_stream_t    *stream;
    sout_stream_id_t *sout_id;
    httpd_url_t      *url;
    const char       *dst;
    int               ttl;
    unsigned          loport, hiport;
};


typedef struct rtsp_strack_t rtsp_strack_t;

/* For unicast streaming */
struct rtsp_session_t
{
    rtsp_stream_t *stream;

    /* is it in "play" state */
    vlc_bool_t     b_playing;

    /* output (id-access) */
    int            trackc;
    rtsp_strack_t *trackv;

    char name[0];
};


/* Unicast session track */
struct rtsp_strack_t
{
    sout_stream_id_t  *id;
    sout_access_out_t *access;
    vlc_bool_t         playing;
};


rtsp_stream_id_t *RtspAddId( rtsp_stream_t *rtsp, sout_stream_id_t *sid,
                             unsigned num,
                             /* Multicast stuff - TODO: cleanup */
                             const char *dst, int ttl,
                             unsigned loport, unsigned hiport )
{
    char urlbuf[strlen( rtsp->psz_control ) + 1 + 10];
    rtsp_stream_id_t *id = malloc( sizeof( *id ) );
    httpd_url_t *url;

    if( id == NULL )
        return NULL;

    id->stream = rtsp;
    id->sout_id = sid;
    /* TODO: can we assume that this need not be strdup'd? */
    id->dst = dst;
    if( id->dst != NULL )
    {
        id->ttl = ttl;
        id->loport = loport;
        id->hiport = hiport;
    }

    sprintf( urlbuf, "%s/trackID=%d", rtsp->psz_path, num );
    msg_Dbg( rtsp->owner, "RTSP: adding %s\n", urlbuf );
    url = id->url = httpd_UrlNewUnique( rtsp->host, urlbuf, NULL, NULL, NULL );

    if( url == NULL )
    {
        free( id );
        return NULL;
    }

    httpd_UrlCatch( url, HTTPD_MSG_DESCRIBE, RtspCallbackId, (void *)id );
    httpd_UrlCatch( url, HTTPD_MSG_SETUP,    RtspCallbackId, (void *)id );
    httpd_UrlCatch( url, HTTPD_MSG_PLAY,     RtspCallbackId, (void *)id );
    httpd_UrlCatch( url, HTTPD_MSG_PAUSE,    RtspCallbackId, (void *)id );
    httpd_UrlCatch( url, HTTPD_MSG_TEARDOWN, RtspCallbackId, (void *)id );

    return id;
}


void RtspDelId( rtsp_stream_t *rtsp, rtsp_stream_id_t *id )
{
    vlc_mutex_lock( &rtsp->lock );
    for( int i = 0; i < rtsp->sessionc; i++ )
    {
        rtsp_session_t *ses = rtsp->sessionv[i];

        for( int j = 0; j < ses->trackc; j++ )
        {
            if( ses->trackv[j].id == id->sout_id )
            {
                rtsp_strack_t *tr = ses->trackv + j;
                sout_AccessOutDelete( tr->access );
                REMOVE_ELEM( ses->trackv, ses->trackc, j );
                /* FIXME: are we supposed to notify the client? */
            }
        }
    }

    vlc_mutex_unlock( &rtsp->lock );
    httpd_UrlDelete( id->url );
    free( id );
}


/** rtsp must be locked */
static
rtsp_session_t *RtspClientNew( rtsp_stream_t *rtsp, const char *name )
{
    rtsp_session_t *s = malloc( sizeof( *s ) + strlen( name ) + 1 );

    s->stream = rtsp;
    s->b_playing = VLC_FALSE;
    s->trackc = 0;
    s->trackv = NULL;
    strcpy( s->name, name );

    TAB_APPEND( rtsp->sessionc, rtsp->sessionv, s );

    return s;
}


/** rtsp must be locked */
static
rtsp_session_t *RtspClientGet( rtsp_stream_t *rtsp, const char *name )
{
    int i;

    if( name == NULL )
        return NULL;

    /* FIXME: use a hash/dictionary */
    for( i = 0; i < rtsp->sessionc; i++ )
    {
        if( !strcmp( rtsp->sessionv[i]->name, name ) )
            return rtsp->sessionv[i];
    }
    return NULL;
}


/** rtsp must be locked */
static
void RtspClientDel( rtsp_stream_t *rtsp, rtsp_session_t *session )
{
    int i;
    TAB_REMOVE( rtsp->sessionc, rtsp->sessionv, session );

    for( i = 0; i < session->trackc; i++ )
    {
        rtp_del_sink( session->trackv[i].id, session->trackv[i].access );
        sout_AccessOutDelete( session->trackv[i].access );
    }

    free( session->trackv );
    free( session );
}


/** Aggregate RTSP callback */
static int RtspCallback( httpd_callback_sys_t *p_args,
                         httpd_client_t *cl,
                         httpd_message_t *answer, httpd_message_t *query )
{
    rtsp_stream_t *rtsp = (rtsp_stream_t *)p_args;
    const char *psz_session = NULL, *psz_cseq;

    if( answer == NULL || query == NULL )
    {
        return VLC_SUCCESS;
    }
    //fprintf( stderr, "RtspCallback query: type=%d\n", query->i_type );

    answer->i_proto = HTTPD_PROTO_RTSP;
    answer->i_version= query->i_version;
    answer->i_type   = HTTPD_MSG_ANSWER;
    answer->i_body = 0;
    answer->p_body = NULL;

    if( httpd_MsgGet( query, "Require" ) != NULL )
    {
        answer->i_status = 551;
        httpd_MsgAdd( query, "Unsupported", "%s",
                      httpd_MsgGet( query, "Require" ) );
    }
    else
    switch( query->i_type )
    {
        case HTTPD_MSG_DESCRIBE:
        {
            char *psz_sdp = SDPGenerate( rtsp->owner, rtsp->psz_control );

            answer->i_status = 200;
            httpd_MsgAdd( answer, "Content-Type",  "%s", "application/sdp" );
            httpd_MsgAdd( answer, "Content-Base",  "%s", rtsp->psz_control );
            answer->p_body = (uint8_t *)psz_sdp;
            answer->i_body = strlen( psz_sdp );
            break;
        }

        case HTTPD_MSG_SETUP:
            answer->i_status = 459;
            break;

        case HTTPD_MSG_PLAY:
        {
            rtsp_session_t *ses;
            answer->i_status = 200;

            psz_session = httpd_MsgGet( query, "Session" );

            vlc_mutex_lock( &rtsp->lock );
            ses = RtspClientGet( rtsp, psz_session );
            if( ( ses != NULL ) && !ses->b_playing )
            {
                /* FIXME */
                ses->b_playing = VLC_TRUE;

                for( int i = 0; i < ses->trackc; i++ )
                    rtp_add_sink( ses->trackv[i].id, ses->trackv[i].access );
            }
            vlc_mutex_unlock( &rtsp->lock );
            break;
        }

        case HTTPD_MSG_PAUSE:
            answer->i_status = 405;
            httpd_MsgAdd( answer, "Allow", "DESCRIBE, PLAY, TEARDOWN" );
            break;

        case HTTPD_MSG_TEARDOWN:
        {
            rtsp_session_t *ses;

            /* for now only multicast so easy again */
            answer->i_status = 200;

            psz_session = httpd_MsgGet( query, "Session" );

            vlc_mutex_lock( &rtsp->lock );
            ses = RtspClientGet( rtsp, psz_session );
            if( ses != NULL )
                RtspClientDel( rtsp, ses );
            vlc_mutex_unlock( &rtsp->lock );
            break;
        }

        default:
            return VLC_EGENERIC;
    }

    httpd_MsgAdd( answer, "Server", "%s", PACKAGE_STRING );
    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    psz_cseq = httpd_MsgGet( query, "Cseq" );
    if( psz_cseq )
        httpd_MsgAdd( answer, "Cseq", "%s", psz_cseq );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( psz_session )
        httpd_MsgAdd( answer, "Session", "%s;timeout=5", psz_session );
    return VLC_SUCCESS;
}


/** Finds the next transport choice */
static inline const char *transport_next( const char *str )
{
    /* Looks for comma */
    str = strchr( str, ',' );
    if( str == NULL )
        return NULL; /* No more transport options */

    str++; /* skips comma */
    while( strchr( "\r\n\t ", *str ) )
        str++;

    return (*str) ? str : NULL;
}


/** Finds the next transport parameter */
static inline const char *parameter_next( const char *str )
{
    while( strchr( ",;", *str ) == NULL )
        str++;

    return (*str == ';') ? (str + 1) : NULL;
}


/** Non-aggregate RTSP callback */
static int RtspCallbackId( httpd_callback_sys_t *p_args,
                           httpd_client_t *cl,
                           httpd_message_t *answer, httpd_message_t *query )
{
    rtsp_stream_id_t *id = (rtsp_stream_id_t *)p_args;
    rtsp_stream_t    *rtsp = id->stream;
    sout_stream_t    *p_stream = id->stream->owner;
    char psz_session_init[21];
    const char *psz_session;
    const char *psz_cseq;

    if( answer == NULL || query == NULL )
        return VLC_SUCCESS;
    //fprintf( stderr, "RtspCallback query: type=%d\n", query->i_type );

    /* */
    answer->i_proto = HTTPD_PROTO_RTSP;
    answer->i_version= query->i_version;
    answer->i_type   = HTTPD_MSG_ANSWER;
    answer->i_body = 0;
    answer->p_body = NULL;

    /* Create new session ID if needed */
    psz_session = httpd_MsgGet( query, "Session" );
    if( psz_session == NULL )
    {
        /* FIXME: should be somewhat secure randomness */
        snprintf( psz_session_init, sizeof(psz_session_init), I64Fu,
                  NTPtime64() + rand() );
    }

    if( httpd_MsgGet( query, "Require" ) != NULL )
    {
        answer->i_status = 551;
        httpd_MsgAdd( query, "Unsupported", "%s",
                      httpd_MsgGet( query, "Require" ) );
    }
    else
    switch( query->i_type )
    {
        case HTTPD_MSG_SETUP:
        {
            answer->i_status = 461;

            for( const char *tpt = httpd_MsgGet( query, "Transport" );
                 tpt != NULL;
                 tpt = transport_next( tpt ) )
            {
                vlc_bool_t b_multicast = VLC_TRUE, b_unsupp = VLC_FALSE;
                unsigned loport = 5004, hiport = 5005; /* from RFC3551 */

                /* Check transport protocol. */
                /* Currently, we only support RTP/AVP over UDP */
                if( strncmp( tpt, "RTP/AVP", 7 ) )
                    continue;
                tpt += 7;
                if( strncmp( tpt, "/UDP", 4 ) == 0 )
                    tpt += 4;
                if( strchr( ";,", *tpt ) == NULL )
                    continue;

                /* Parse transport options */
                for( const char *opt = parameter_next( tpt );
                     opt != NULL;
                     opt = parameter_next( opt ) )
                {
                    if( strncmp( opt, "multicast", 9 ) == 0)
                        b_multicast = VLC_TRUE;
                    else
                    if( strncmp( opt, "unicast", 7 ) == 0 )
                        b_multicast = VLC_FALSE;
                    else
                    if( sscanf( opt, "client_port=%u-%u", &loport, &hiport ) == 2 )
                        ;
                    else
                    if( strncmp( opt, "mode=", 5 ) == 0 )
                    {
                        if( strncasecmp( opt + 5, "play", 4 )
                         && strncasecmp( opt + 5, "\"PLAY\"", 6 ) )
                        {
                            /* Not playing?! */
                            b_unsupp = VLC_TRUE;
                            break;
                        }
                    }
                    else
                    {
                    /*
                     * Every other option is unsupported:
                     *
                     * "source" and "append" are invalid.
                     *
                     * For multicast, "port", "layers", "ttl" are set by the
                     * stream output configuration.
                     *
                     * For unicast, we do not allow "destination" as it
                     * carries a DoS risk, and we decide on "server_port".
                     *
                     * "interleaved" and "ssrc" are not implemented.
                     */
                        b_unsupp = VLC_TRUE;
                        break;
                    }
                }

                if( b_unsupp )
                    continue;

                if( b_multicast )
                {
                    const char *dst = id->dst;
                    if( dst == NULL )
                        continue;

                    answer->i_status = 200;

                    httpd_MsgAdd( answer, "Transport",
                                  "RTP/AVP/UDP;destination=%s;port=%u-%u;"
                                  "ttl=%d;mode=play",
                                  dst, id->loport, id->hiport,
                                  ( id->ttl > 0 ) ? id->ttl : 1 );
                }
                else
                {
                    char ip[NI_MAXNUMERICHOST], url[NI_MAXNUMERICHOST + 8];
                    static const char access[] = "udp{raw,rtcp}";
                    rtsp_session_t *ses = NULL;
                    rtsp_strack_t track = { id->sout_id, NULL, VLC_FALSE };

                    if( httpd_ClientIP( cl, ip ) == NULL )
                    {
                        answer->i_status = 500;
                        continue;
                    }

                    snprintf( url, sizeof( url ),
                              ( strchr( ip, ':' ) != NULL ) ? "[%s]:%d" : "%s:%d",
                              ip, loport );

                    track.access = sout_AccessOutNew( p_stream->p_sout,
                                                      access, url );
                    if( track.access == NULL )
                    {
                        msg_Err( p_stream,
                                 "cannot create access output for %s://%s",
                                 access, url );
                        answer->i_status = 500;
                        continue;
                    }

                    char *src = var_GetNonEmptyString( track.access, "src-addr" );
                    int sport = var_GetInteger( track.access, "src-port" );

                    vlc_mutex_lock( &rtsp->lock );
                    if( psz_session == NULL )
                    {
                        psz_session = psz_session_init;
                        ses = RtspClientNew( rtsp, psz_session );
                    }
                    else
                    {
                        /* FIXME: we probably need to remove an access out,
                         * if there is already one for the same ID */
                        ses = RtspClientGet( rtsp, psz_session );
                        if( ses == NULL )
                        {
                            answer->i_status = 454;
                            vlc_mutex_unlock( &rtsp->lock );
                            continue;
                        }
                    }

                    INSERT_ELEM( ses->trackv, ses->trackc, ses->trackc, track );
                    vlc_mutex_unlock( &rtsp->lock );

                    httpd_ServerIP( cl, ip );

                    if( ( src != NULL ) && strcmp( src, ip ) )
                    {
                        /* Specify source IP if it is different from the RTSP
                         * control connection server address */
                        char *ptr = strchr( src, '%' );
                        if( ptr != NULL ) *ptr = '\0'; /* remove scope ID */

                        httpd_MsgAdd( answer, "Transport",
                                      "RTP/AVP/UDP;unicast;source=%s;"
                                      "client_port=%u-%u;server_port=%u-%u;"
                                      "mode=play",
                                      src, loport, loport + 1, sport, sport + 1 );
                    }
                    else
                    {
                        httpd_MsgAdd( answer, "Transport",
                                      "RTP/AVP/UDP;unicast;"
                                      "client_port=%u-%u;server_port=%u-%u;"
                                      "mode=play",
                                      loport, loport + 1, sport, sport + 1 );
                    }

                    answer->i_status = 200;
                    free( src );
                }
                break;
            }
            break;
        }

        case HTTPD_MSG_PAUSE:
            answer->i_status = 405;
            httpd_MsgAdd( answer, "Allow", "SETUP, TEARDOWN" );
            break;

        case HTTPD_MSG_TEARDOWN:
        {
            rtsp_session_t *ses;

            answer->i_status = 200;

            psz_session = httpd_MsgGet( query, "Session" );

            vlc_mutex_lock( &rtsp->lock );
            ses = RtspClientGet( rtsp, psz_session );
            if( ses != NULL )
            {
                for( int i = 0; i < ses->trackc; i++ )
                {
                    if( ses->trackv[i].id == id->sout_id )
                    {
                        rtp_del_sink( id->sout_id, ses->trackv[i].access );
                        sout_AccessOutDelete( ses->trackv[i].access );
                        REMOVE_ELEM( ses->trackv, ses->trackc, i );
                    }
                }
            }
            vlc_mutex_unlock( &rtsp->lock );
            break;
        }

        default:
            answer->i_status = 460;
            break;
    }

    psz_cseq = httpd_MsgGet( query, "Cseq" );
    if( psz_cseq )
        httpd_MsgAdd( answer, "Cseq", "%s", psz_cseq );
    httpd_MsgAdd( answer, "Server", "%s", PACKAGE_STRING );
    httpd_MsgAdd( answer, "Content-Length", "%d", answer->i_body );
    httpd_MsgAdd( answer, "Cache-Control", "%s", "no-cache" );

    if( psz_session )
        httpd_MsgAdd( answer, "Session", "%s"/*;timeout=5*/, psz_session );
    return VLC_SUCCESS;
}
