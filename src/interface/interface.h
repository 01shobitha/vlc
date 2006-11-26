/*****************************************************************************
 * interface.h: Internal interface prototypes and structures
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 * $Id: interface.c 17743 2006-11-13 15:43:19Z courmisch $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Clément Stenac <zorglub@videolan.org>
 *          Felix Kühne <fkuehne@videolan.org>
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

/**********************************************************************
 * Interaction
 **********************************************************************/

void intf_InteractionManage( playlist_t *);
void intf_InteractionDestroy( interaction_t *);
