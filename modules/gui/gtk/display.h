/*****************************************************************************
 * display.h: Gtk+ tools for main interface.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: display.h,v 1.2 2002/09/30 11:05:39 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          St�phane Borel <stef@via.ecp.fr>
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
 * Prototypes
 *****************************************************************************/

gint E_(GtkModeManage)      ( intf_thread_t * p_intf );
void E_(GtkDisplayDate)     ( GtkAdjustment *p_adj );
void E_(GtkHideTooltips)    ( vlc_object_t * );
void    GtkHideToolbarText  ( vlc_object_t * );

