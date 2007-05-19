/*****************************************************************************
 * announce.c : announce handler
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <assert.h>

#include <vlc/vlc.h>
#include <vlc_sout.h>
#include <vlc_network.h> /* FIXME: fix RegisterSDP() and remove this */
#include "stream_output.h"

struct announce_method_t
{
} sap_method;

/****************************************************************************
 * Sout-side functions
 ****************************************************************************/

/**
 *  Register a new session with the announce handler
 *
 * \param p_sout a sout instance structure
 * \param p_session a session descriptor
 * \param p_method an announce method descriptor
 * \return VLC_SUCCESS or an error
 */
int sout_AnnounceRegister( sout_instance_t *p_sout,
                       session_descriptor_t *p_session,
                       announce_method_t *p_method )
{
    int i_ret;
    announce_handler_t *p_announce = (announce_handler_t*)
                              vlc_object_find( p_sout,
                                              VLC_OBJECT_ANNOUNCE,
                                              FIND_ANYWHERE );

    if( !p_announce )
    {
        msg_Dbg( p_sout, "No announce handler found, creating one" );
        p_announce = announce_HandlerCreate( p_sout );
        if( !p_announce )
        {
            msg_Err( p_sout, "Creation failed" );
            return VLC_ENOMEM;
        }
        vlc_object_yield( p_announce );
        msg_Dbg( p_sout, "creation done" );
    }

    i_ret = announce_Register( p_announce, p_session, p_method );
    vlc_object_release( p_announce );

    return i_ret;
}

/**
 *  Register a new session with the announce handler, using a pregenerated SDP
 *
 * \param p_sout a sout instance structure
 * \param psz_sdp the SDP to register
 * \param psz_uri session address (needed for SAP address auto detection)
 * \param p_method an announce method descriptor
 * \return the new session descriptor structure
 */
session_descriptor_t *
sout_AnnounceRegisterSDP( sout_instance_t *p_sout, const char *cfgpref,
                          const char *psz_sdp, const char *psz_uri,
                          announce_method_t *p_method )
{
    session_descriptor_t *p_session;
    announce_handler_t *p_announce = (announce_handler_t*)
                                     vlc_object_find( p_sout,
                                              VLC_OBJECT_ANNOUNCE,
                                              FIND_ANYWHERE );
    if( !p_announce )
    {
        msg_Dbg( p_sout, "no announce handler found, creating one" );
        p_announce = announce_HandlerCreate( p_sout );
        if( !p_announce )
        {
            msg_Err( p_sout, "Creation failed" );
            return NULL;
        }
        vlc_object_yield( p_announce );
    }

    p_session = sout_AnnounceSessionCreate(VLC_OBJECT (p_sout), cfgpref);
    p_session->psz_sdp = strdup( psz_sdp );

    /* GRUIK. We should not convert back-and-forth from string to numbers */
    struct addrinfo *res;
    if (vlc_getaddrinfo (VLC_OBJECT (p_sout), psz_uri, 0, NULL, &res) == 0)
    {
        if (res->ai_addrlen <= sizeof (p_session->addr))
            memcpy (&p_session->addr, res->ai_addr,
                    p_session->addrlen = res->ai_addrlen);
        vlc_freeaddrinfo (res);
    }

    announce_Register( p_announce, p_session, p_method );

    vlc_object_release( p_announce );
    return p_session;
}

/**
 *  UnRegister an existing session
 *
 * \param p_sout a sout instance structure
 * \param p_session the session descriptor
 * \return VLC_SUCCESS or an error
 */
int sout_AnnounceUnRegister( sout_instance_t *p_sout,
                             session_descriptor_t *p_session )
{
    int i_ret;
    announce_handler_t *p_announce = (announce_handler_t*)
                              vlc_object_find( p_sout,
                                              VLC_OBJECT_ANNOUNCE,
                                              FIND_ANYWHERE );
    if( !p_announce )
    {
        msg_Dbg( p_sout, "unable to remove announce: no announce handler" );
        return VLC_ENOOBJ;
    }
    i_ret  = announce_UnRegister( p_announce, p_session );

    vlc_object_release( p_announce );

    return i_ret;
}

/**
 * Create and initialize a session descriptor
 *
 * \return a new session descriptor
 */
session_descriptor_t * sout_AnnounceSessionCreate (vlc_object_t *obj,
                                                   const char *cfgpref)
{
    size_t cfglen = strlen (cfgpref);
    if (cfglen > 100)
        return NULL;

    char varname[cfglen + sizeof ("description")], *subvar = varname + cfglen;
    strcpy (varname, cfgpref);

    session_descriptor_t *p_session = calloc (1, sizeof (*p_session));
    if (p_session == NULL)
        return NULL;

    strcpy (subvar, "name");
    p_session->psz_name = var_GetNonEmptyString (obj, varname);
    strcpy (subvar, "group");
    p_session->psz_group = var_GetNonEmptyString (obj, varname);

    strcpy (subvar, "description");
    p_session->description = var_GetNonEmptyString (obj, varname);
    strcpy (subvar, "url");
    p_session->url = var_GetNonEmptyString (obj, varname);
    strcpy (subvar, "email");
    p_session->email = var_GetNonEmptyString (obj, varname);
    strcpy (subvar, "phone");
    p_session->phone = var_GetNonEmptyString (obj, varname);

    return p_session;
}

int sout_SessionSetMedia (vlc_object_t *obj, session_descriptor_t *p_session,
                          const char *fmt, const char *src, int sport,
                          const char *dst, int dport)
{
    if ((p_session->sdpformat = strdup (fmt)) == NULL)
        return VLC_ENOMEM;

    /* GRUIK. We should not convert back-and-forth from string to numbers */
    struct addrinfo *res;
    if (vlc_getaddrinfo (obj, dst, dport, NULL, &res) == 0)
    {
        if (res->ai_addrlen > sizeof (p_session->addr))
            goto oflow;

        memcpy (&p_session->addr, res->ai_addr,
                p_session->addrlen = res->ai_addrlen);
        vlc_freeaddrinfo (res);
    }
    if (vlc_getaddrinfo (obj, src, sport, NULL, &res) == 0)
    {
        if (res->ai_addrlen > sizeof (p_session->orig))
            goto oflow;
        memcpy (&p_session->orig, res->ai_addr,
               p_session->origlen = res->ai_addrlen);
        vlc_freeaddrinfo (res);
    }
    return 0;

oflow:
    vlc_freeaddrinfo (res);
    return VLC_ENOMEM;
}

/**
 * Destroy a session descriptor and free all
 *
 * \param p_session the session to destroy
 * \return Nothing
 */
void sout_AnnounceSessionDestroy( session_descriptor_t *p_session )
{
    if( p_session )
    {
        free (p_session->psz_name);
        free (p_session->psz_group);
        free (p_session->psz_sdp);
        free (p_session->description);
        free (p_session->sdpformat);
        free (p_session->url);
        free (p_session->email);
        free (p_session->phone);
        free( p_session );
    }
}

/**
 * \return the SAP announce method
 */
announce_method_t * sout_SAPMethod (void)
{
    return &sap_method;
}

void sout_MethodRelease (announce_method_t *m)
{
    assert (m == &sap_method);
}

/************************************************************************
 * Announce handler functions (private)
 ************************************************************************/

/**
 * Create the announce handler object
 *
 * \param p_this a vlc_object structure
 * \return the new announce handler or NULL on error
 */
announce_handler_t *__announce_HandlerCreate( vlc_object_t *p_this )
{
    announce_handler_t *p_announce;

    p_announce = vlc_object_create( p_this, VLC_OBJECT_ANNOUNCE );

    if( !p_announce )
    {
        msg_Err( p_this, "out of memory" );
        return NULL;
    }

    p_announce->p_sap = NULL;
    vlc_object_attach( p_announce, p_this->p_libvlc);

    return p_announce;
}

/**
 * Destroy a  announce handler object
 *
 * \param p_announce the announce handler to destroy
 * \return VLC_SUCCESS or an error
 */
int announce_HandlerDestroy( announce_handler_t *p_announce )
{
    if( p_announce->p_sap )
    {
        vlc_object_kill ((vlc_object_t *)p_announce->p_sap);
        /* Wait for the SAP thread to exit */
        vlc_thread_join( (vlc_object_t *)p_announce->p_sap );
        announce_SAPHandlerDestroy( p_announce->p_sap );
    }

    /* Free the structure */
    vlc_object_destroy( p_announce );

    return VLC_SUCCESS;
}

/* Register an announce */
int announce_Register( announce_handler_t *p_announce,
                       session_descriptor_t *p_session,
                       announce_method_t *p_method )
{
    if (p_method == NULL)
        return VLC_EGENERIC;

    msg_Dbg( p_announce, "registering announce");
    if( p_method == &sap_method )
    {
        /* Do we already have a SAP announce handler ? */
        if( !p_announce->p_sap )
        {
            sap_handler_t *p_sap = announce_SAPHandlerCreate( p_announce );
            msg_Dbg( p_announce, "creating SAP announce handler");
            if( !p_sap )
            {
                msg_Err( p_announce, "SAP handler creation failed" );
                return VLC_ENOOBJ;
            }
            p_announce->p_sap = p_sap;
        }
        /* this will set p_session->p_sap for later deletion */
        msg_Dbg( p_announce, "adding SAP session");
        p_announce->p_sap->pf_add( p_announce->p_sap, p_session );
    }
    else
    {
        msg_Err( p_announce, "announce type unsupported" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}


/* Unregister an announce */
int announce_UnRegister( announce_handler_t *p_announce,
                  session_descriptor_t *p_session )
{
    msg_Dbg( p_announce, "unregistering announce" );
    if( p_announce->p_sap )
        p_announce->p_sap->pf_del( p_announce->p_sap, p_session );
    return VLC_SUCCESS;
}
