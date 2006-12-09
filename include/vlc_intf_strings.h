/*****************************************************************************
 * vlc_intf_strings.h : Strings for main interfaces
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _VLC_ISTRINGS_H
#define _VLC_ISTRINGS_H 1

/*************** Open dialogs **************/

#define I_OP_OPF        N_("Quick &Open File...")
#define I_OP_ADVOP      N_("&Advanced Open...")
#define I_OP_OPDIR      N_("Open &Directory...")

#define I_OP_SEL_FILES  N_("Select one or more files to open")

/******************* Menus *****************/

#define I_MENU_INFO  N_("Information...")
#define I_MENU_MSG   N_("Messages...")
#define I_MENU_EXT   N_("Extended settings...")

#define I_MENU_ABOUT N_("About VLC media player...")

/* Playlist popup */
#define I_POP_PLAY N_("Play")
#define I_POP_PREPARSE N_("Fetch information")
#define I_POP_DEL N_("Delete")
#define I_POP_INFO N_("Information...")
#define I_POP_SORT N_("Sort")
#define I_POP_ADD N_("Add node")
#define I_POP_STREAM N_("Stream...")
#define I_POP_SAVE N_("Save...")

/*************** Playlist *************/

#define I_PL_LOOP       N_("Repeat all")
#define I_PL_REPEAT     N_("Repeat one")
#define I_PL_NOREPEAT   N_("No repeat")

#define I_PL_RANDOM     N_("Random")
#define I_PL_NORANDOM   N_("No random")

#define I_PL_ADDPL      N_("Add to playlist")
#define I_PL_ADDML      N_("Add to media library")

#define I_PL_ADDF       N_("Add file...")
#define I_PL_ADVADD     N_("Advanced open...")
#define I_PL_ADDDIR     N_("Add directory...")

#define I_PL_SAVE       N_("Save playlist to file...")
#define I_PL_LOAD       N_("Load playlist file...")

#define I_PL_SEARCH     N_("Search")
#define I_PL_FILTER     N_("Search filter")

#define I_PL_SD         N_("Additional sources")

/*************** Preferences *************/

#define I_HIDDEN_ADV N_( "Some options are available but hidden. "\
                         "Check \"Advanced options\" to see them." )
#endif
