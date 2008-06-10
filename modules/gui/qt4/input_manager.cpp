/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Ilkka Ollakka  <ileoo@videolan.org>
 *          Jean-Baptiste <jb@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"
#include "input_manager.hpp"
#include "dialogs_provider.hpp"

static int ChangeVideo( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param );
static int ChangeAudio( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param );
static int ItemChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int PLItemChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int InterfaceChanged( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void * );
static int ItemStateChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int ItemRateChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int ItemTitleChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int VolumeChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );

/**********************************************************************
 * InputManager implementation
 **********************************************************************
 * The Input Manager can be the main one around the playlist
 * But can also be used for VLM dialog or similar
 **********************************************************************/

InputManager::InputManager( QObject *parent, intf_thread_t *_p_intf) :
                           QObject( parent ), p_intf( _p_intf )
{
    i_old_playing_status = END_S;
    b_had_audio  = b_had_video = b_has_audio = b_has_video = false;
    b_has_subs   = false;
    old_name     = "";
    artUrl       = "";
    p_input      = NULL;
    i_rate       = 0;
    i_input_id   = 0;
}

InputManager::~InputManager()
{
    delInput();
}

/* Define the Input used.
   Add the callbacks on input
   p_input is yield once here */
void InputManager::setInput( input_thread_t *_p_input )
{
    delInput();
    p_input = _p_input;
    b_had_audio = b_had_video = b_has_audio = b_has_video = false;
    if( p_input && !( p_input->b_dead || p_input->b_die ) )
    {
        vlc_object_yield( p_input );
        emit statusChanged( PLAYING_S );
        UpdateMeta();
        UpdateTracks();
        UpdateNavigation();
        UpdateArt();
        addCallbacks();
        i_input_id = input_GetItem( p_input )->i_id;
    }
    else
    {
        p_input = NULL;
        i_input_id = 0;
        emit rateChanged( INPUT_RATE_DEFAULT );
        emit inputUnset();
    }
}

/* delete Input if it ever existed.
   Delete the callbacls on input
   p_input is released once here */
void InputManager::delInput()
{
    if( p_input )
    {
        delCallbacks();
        i_old_playing_status = END_S;
        i_input_id = 0;
        old_name   = "";
        artUrl     = "";
        emit positionUpdated( 0.0, 0 ,0 );
        emit statusChanged( END_S );
        emit nameChanged( "" );
        emit artChanged( "" );
        emit rateChanged( INPUT_RATE_DEFAULT );
        vlc_object_release( p_input );
        p_input = NULL;
    }
}

/* Add the callbacks on Input. Self explanatory */
void InputManager::addCallbacks()
{
    /* We don't care about:
       - spu-es
       - chapter
       - programs
       - audio-delay
       - spu-delay
       - bookmark
       - position, time, length, because they are included in intf-change
     */
    /* src/input/input.c:1629 */
    var_AddCallback( p_input, "state", ItemStateChanged, this );
    /* src/input/es-out.c:550 */
    var_AddCallback( p_input, "audio-es", ChangeAudio, this );
    /* src/input/es-out.c:551 */
    var_AddCallback( p_input, "video-es", ChangeVideo, this );
    /* src/input/input.c:1765 */
    var_AddCallback( p_input, "rate-change", ItemRateChanged, this );
    /* src/input/input.c:2003 */
    var_AddCallback( p_input, "title", ItemTitleChanged, this );
    /* src/input/input.c:734 for timers update*/
    var_AddCallback( p_input, "intf-change", InterfaceChanged, this );
}

/* Delete the callbacks on Input. Self explanatory */
void InputManager::delCallbacks()
{
    var_DelCallback( p_input, "audio-es", ChangeAudio, this );
    var_DelCallback( p_input, "video-es", ChangeVideo, this );
    var_DelCallback( p_input, "state", ItemStateChanged, this );
    var_DelCallback( p_input, "rate-change", ItemRateChanged, this );
    var_DelCallback( p_input, "title", ItemTitleChanged, this );
    var_DelCallback( p_input, "intf-change", InterfaceChanged, this );
}

/* Convert the event from the callbacks in actions */
void InputManager::customEvent( QEvent *event )
{
    int type = event->type();
    IMEvent *ple = static_cast<IMEvent *>(event);

    if ( type != PositionUpdate_Type &&
         type != ItemChanged_Type &&
         type != ItemRateChanged_Type &&
         type != ItemTitleChanged_Type &&
         type != ItemStateChanged_Type )
        return;

    if( !hasInput() ) return;

    if( ( type != PositionUpdate_Type  &&
          type != ItemRateChanged_Type &&
          type != ItemStateChanged_Type
        )
        && ( i_input_id != ple->i_id ) )
        return;

    if( type != PositionUpdate_Type )
        msg_Dbg( p_intf, "New Event: type %i", type );

    /* Actions */
    switch( type )
    {
    case PositionUpdate_Type:
        UpdatePosition();
        break;
    case ItemChanged_Type:
        UpdateMeta();
        UpdateNavigation();
        UpdateTracks();
        UpdateStatus();
        UpdateArt();
        break;
    case ItemRateChanged_Type:
        UpdateRate();
        break;
    case ItemTitleChanged_Type:
        UpdateNavigation();
        UpdateMeta();
        break;
    case ItemStateChanged_Type:
        UpdateTracks();
        UpdateStatus();
        break;
    }
}

void InputManager::UpdatePosition()
{
    /* Update position */
    int i_length, i_time; /* Int is enough, since we store seconds */
    float f_pos;
    i_length = var_GetTime(  p_input , "length" ) / 1000000;
    i_time = var_GetTime(  p_input , "time") / 1000000;
    f_pos = var_GetFloat(  p_input , "position" );
    emit positionUpdated( f_pos, i_time, i_length );
}

void InputManager::UpdateNavigation()
{
    /* Update navigation status */
    vlc_value_t val; val.i_int = 0;
    var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int > 0 )
    {
        val.i_int = 0;
        var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &val, NULL );
        emit navigationChanged( (val.i_int > 0) ? 1 : 2 );
    }
    else
    {
        emit navigationChanged( 0 );
    }
}

void InputManager::UpdateStatus()
{
    /* Update playing status */
    vlc_value_t val; val.i_int = 0;
    var_Get( p_input, "state", &val );
    if( i_old_playing_status != val.i_int )
    {
        i_old_playing_status = val.i_int;
        emit statusChanged( val.i_int );
    }
}

void InputManager::UpdateRate()
{
    /* Update Rate */
    int i_new_rate = var_GetInteger( p_input, "rate");
    if( i_new_rate != i_rate )
    {
        i_rate = i_new_rate;
        /* Update rate */
        emit rateChanged( i_rate );
    }
}

void InputManager::UpdateMeta()
{
    /* Update text, name and nowplaying */
    QString text;

    char *psz_name = input_item_GetTitle( input_GetItem( p_input ) );
    if( EMPTY_STR( psz_name ) )
    {
        free( psz_name );
        psz_name = input_item_GetName( input_GetItem( p_input ) );
    }

    char *psz_nowplaying =
        input_item_GetNowPlaying( input_GetItem( p_input ) );
    if( !EMPTY_STR( psz_nowplaying ) )
    {
        text.sprintf( "%s - %s", psz_nowplaying, psz_name );
    }
    else
    {
        char *psz_artist = input_item_GetArtist( input_GetItem( p_input ) );
        if( !EMPTY_STR( psz_artist ) )
        {
            text.sprintf( "%s - %s", psz_artist, psz_name );
        }
        else
        {
            text.sprintf( "%s", psz_name );
        }
        free( psz_artist );
    }
    free( psz_name );
    free( psz_nowplaying );

    if( old_name != text )
    {
        emit nameChanged( text );
        old_name=text;
    }
}

void InputManager::UpdateTracks()
{
    /* Has Audio, has Video Tracks ? */
    vlc_value_t val;
    var_Change( p_input, "audio-es", VLC_VAR_CHOICESCOUNT, &val, NULL );
    b_has_audio = val.i_int > 0;
    var_Change( p_input, "video-es", VLC_VAR_CHOICESCOUNT, &val, NULL );
    b_has_video = val.i_int > 0;
    var_Change( p_input, "spu-es", VLC_VAR_CHOICESCOUNT, &val, NULL );
    b_has_subs = val.i_int > 0;

    /* Update ZVBI status */
#ifdef ZVBI_COMPILED
    /* Update teletext status*/
    if( b_has_subs )
        emit teletextEnabled( false );/* FIXME */
#endif
}

void InputManager::UpdateArt()
{
    /* Update Art meta */
    QString url;
    char *psz_art = input_item_GetArtURL( input_GetItem( p_input ) );
    url.sprintf("%s", psz_art );
    free( psz_art );
    if( artUrl != url )
    {
        artUrl = url.replace( "file://",QString("" ) );
        /* Taglib seems to define a attachment://, It won't work yet */
        artUrl = url.replace( "attachment://",QString("" ) );
        emit artChanged( artUrl );
        msg_Dbg( p_intf, "Art:  %s", qtu( artUrl ) );
    }
}

/* User update of the slider */
void InputManager::sliderUpdate( float new_pos )
{
    if( hasInput() )
        var_SetFloat( p_input, "position", new_pos );
}

/* User togglePlayPause */
void InputManager::togglePlayPause()
{
    vlc_value_t state;
    var_Get( p_input, "state", &state );
    state.i_int = ( state.i_int != PLAYING_S ) ? PLAYING_S : PAUSE_S;
    var_Set( p_input, "state", state );
    emit statusChanged( state.i_int );
}

void InputManager::sectionPrev()
{
    if( hasInput() )
    {
        int i_type = var_Type( p_input, "next-chapter" );
        vlc_value_t val; val.b_bool = true;
        var_Set( p_input, (i_type & VLC_VAR_TYPE) != 0 ?
                            "prev-chapter":"prev-title", val );
    }
}

void InputManager::sectionNext()
{
    if( hasInput() )
    {
        int i_type = var_Type( p_input, "next-chapter" );
        vlc_value_t val; val.b_bool = true;
        var_Set( p_input, (i_type & VLC_VAR_TYPE) != 0 ?
                            "next-chapter":"next-title", val );
    }
}

void InputManager::sectionMenu()
{
    if( hasInput() )
    {
        vlc_value_t val; val.i_int = 2;
        var_Set( p_input, "title  0", val );
    }
}

#ifdef ZVBI_COMPILED
void InputManager::telexGotoPage( int page )
{
    if( hasInput() )
    {
        vlc_object_t *p_vbi;
        p_vbi = (vlc_object_t *) vlc_object_find_name( p_input,
                    "zvbi", FIND_ANYWHERE );
        if( p_vbi )
        {
            var_SetInteger( p_vbi, "vbi-page", page );
            vlc_object_release( p_vbi );
        }
    }
}

void InputManager::telexToggle( bool b_enabled )
{
    int i_page = b_enabled ? 100 : 0 ;

    telexGotoPage( i_page );
}

void InputManager::telexSetTransparency( bool b_transp )
{
    if( hasInput() )
    {
        vlc_object_t *p_vbi;
        p_vbi = (vlc_object_t *) vlc_object_find_name( p_input,
                    "zvbi", FIND_ANYWHERE );
        if( p_vbi )
        {
            var_SetBool( p_input->p_libvlc, "vbi-opaque", b_transp );
            vlc_object_release( p_vbi );
        }
    }
}
#endif

void InputManager::slower()
{
    if( hasInput() )
        var_SetVoid( p_input, "rate-slower" );
}

void InputManager::faster()
{
    if( hasInput() )
        var_SetVoid( p_input, "rate-faster" );
}

void InputManager::normalRate()
{
    if( hasInput() )
        var_SetInteger( p_input, "rate", INPUT_RATE_DEFAULT );
}

void InputManager::setRate( int new_rate )
{
    if( hasInput() )
        var_SetInteger( p_input, "rate", new_rate );
}

/**********************************************************************
 * MainInputManager implementation. Wrap an input manager and
 * take care of updating the main playlist input.
 * Used in the main playlist Dialog
 **********************************************************************/
MainInputManager * MainInputManager::instance = NULL;

MainInputManager::MainInputManager( intf_thread_t *_p_intf )
                 : QObject(NULL), p_intf( _p_intf )
{
    p_input = NULL;
    im = new InputManager( this, p_intf );

//    var_AddCallback( THEPL, "item-change", PLItemChanged, this );
    var_AddCallback( THEPL, "item-change", ItemChanged, im );
    var_AddCallback( THEPL, "playlist-current", PLItemChanged, this );
    var_AddCallback( THEPL, "activity", PLItemChanged, this );

    var_AddCallback( p_intf->p_libvlc, "volume-change", VolumeChanged, this );

    // No necessary, I think TODO REMOVE ME at the end
    //var_AddCallback( THEPL, "intf-change", ItemChanged, im );

    /* Warn our embedded IM about input changes */
    CONNECT( this, inputChanged( input_thread_t * ),
             im, setInput( input_thread_t * ) );
}

MainInputManager::~MainInputManager()
{
    if( p_input )
    {
       var_DelCallback( p_input, "state", PLItemChanged, this );
       vlc_object_release( p_input );
       emit inputChanged( NULL );
    }

    var_DelCallback( p_intf->p_libvlc, "volume-change", VolumeChanged, this );

    var_DelCallback( THEPL, "activity", PLItemChanged, this );
    var_DelCallback( THEPL, "item-change", ItemChanged, im );
//    var_DelCallback( THEPL, "item-change", PLItemChanged, this );

    var_DelCallback( THEPL, "playlist-current", PLItemChanged, this );
}

void MainInputManager::customEvent( QEvent *event )
{
    int type = event->type();
    if ( type != ItemChanged_Type && type != VolumeChanged_Type )
        return;

    // msg_Dbg( p_intf, "New MainIM Event of type: %i", type );
    if( type == VolumeChanged_Type )
    {
        emit volumeChanged();
        return;
    }

    /* Should be PLItemChanged Event */
    if( VLC_OBJECT_INTF == p_intf->i_object_type ) /* FIXME: don't use object type */
    {
        vlc_mutex_lock( &p_intf->change_lock );
        if( p_input && ( p_input->b_dead || p_input->b_die ) )
        {
            var_DelCallback( p_input, "state", PLItemChanged, this );
            vlc_object_release( p_input );
            emit inputChanged( NULL );
            p_input = NULL;
            vlc_mutex_unlock( &p_intf->change_lock );
            return;
        }

        if( !p_input )
        {
            QPL_LOCK;
            p_input = THEPL->p_input;
            if( p_input && !( p_input->b_die || p_input->b_dead) )
            {
                vlc_object_yield( p_input );
                var_AddCallback( p_input, "state", PLItemChanged, this );
                emit inputChanged( p_input );
            }
            else
                p_input = NULL;
            QPL_UNLOCK;
        }
        vlc_mutex_unlock( &p_intf->change_lock );
    }
    else
    {
        /* we are working as a dialogs provider */
        playlist_t *p_playlist = (playlist_t *) vlc_object_find( p_intf,
                                       VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        if( p_playlist )
        {
            p_input = p_playlist->p_input;
            emit inputChanged( p_input );
        }
    }
}

/* Playlist Control functions */
void MainInputManager::stop()
{
   playlist_Stop( THEPL );
}

void MainInputManager::next()
{
   playlist_Next( THEPL );
}

void MainInputManager::prev()
{
   playlist_Prev( THEPL );
}

void MainInputManager::togglePlayPause()
{
    if( p_input == NULL )
    {
        playlist_Play( THEPL );
        return;
    }
    getIM()->togglePlayPause();
}

/* Static callbacks */

/* IM */
static int InterfaceChanged( vlc_object_t *p_this, const char *psz_var,
                           vlc_value_t oldval, vlc_value_t newval, void *param )
{
    static int counter = 0;
    InputManager *im = (InputManager*)param;

    counter = ++counter % 4;
    if(!counter)
        return VLC_SUCCESS;
    IMEvent *event = new IMEvent( PositionUpdate_Type, 0 );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemStateChanged( vlc_object_t *p_this, const char *psz_var,
                           vlc_value_t oldval, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;

    IMEvent *event = new IMEvent( ItemStateChanged_Type, 0 );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemRateChanged( vlc_object_t *p_this, const char *psz_var,
                           vlc_value_t oldval, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;

    IMEvent *event = new IMEvent( ItemRateChanged_Type, 0 );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemTitleChanged( vlc_object_t *p_this, const char *psz_var,
                           vlc_value_t oldval, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;

    IMEvent *event = new IMEvent( ItemTitleChanged_Type, 0 );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    InputManager *im = (InputManager*)param;

    IMEvent *event = new IMEvent( ItemChanged_Type, newval.i_int );
    QApplication::postEvent( im, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}


static int ChangeAudio( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param )
{
    InputManager *im = (InputManager*)param;
    im->b_has_audio = true;
    return VLC_SUCCESS;
}

static int ChangeVideo( vlc_object_t *p_this, const char *var, vlc_value_t o,
                        vlc_value_t n, void *param )
{
    InputManager *im = (InputManager*)param;
    im->b_has_video = true;
    return VLC_SUCCESS;
}

/* MIM */
static int PLItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    MainInputManager *mim = (MainInputManager*)param;

    IMEvent *event = new IMEvent( ItemChanged_Type, newval.i_int );
    QApplication::postEvent( mim, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int VolumeChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    MainInputManager *mim = (MainInputManager*)param;

    IMEvent *event = new IMEvent( VolumeChanged_Type, newval.i_int );
    QApplication::postEvent( mim, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

