#!/usr/bin/python
# -*- coding: utf8 -*-
#
# Copyright (C) 2006 Rafaël Carré <funman at videolanorg>
#
# $Id$
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
#

#
# NOTE: this controller is a SAMPLE, and thus doesn't implement all the D-Bus Media Player specification. http://wiki.videolan.org/index.php/DBus-spec
# This is an unfinished document (on the 12/06/2006) and has been designed to be as general as possible.
# So don't expect that much from this, but basic capabilities should work out of the box (Play/Pause/Next/Add)
#
# Also notice it has been designed first for a previous specificaiton, and thus some code may not work/be disabled
#
# You'll need pygtk >= 2.10 to use gtk.StatusIcon
#
import dbus
import dbus.glib
import gtk
import gtk.glade
import gobject
import os

global position
global timer
global playing
playing = False

def itemchange_handler(item):
    gobject.timeout_add( 2000, timeset)
    try:
        a = item["artist"]
    except:
        a = ""
    if a == "":
        a = item["URI"] 
    l_item.set_text(a)

#connect to the bus
bus = dbus.SessionBus()
player_o = bus.get_object("org.freedesktop.MediaPlayer", "/Player")
tracklist_o = bus.get_object("org.freedesktop.MediaPlayer", "/TrackList")

tracklist  = dbus.Interface(tracklist_o, "org.freedesktop.MediaPlayer")
player = dbus.Interface(player_o, "org.freedesktop.MediaPlayer")
try:
    player_o.connect_to_signal("TrackChange", itemchange_handler, dbus_interface="org.freedesktop.MediaPlayer")
except:
    True

#plays an element
def AddTrack(widget):
    mrl = e_mrl.get_text()
    if mrl != None and mrl != "":
        tracklist.AddTrack(mrl, True)
    else:
        mrl = bt_file.get_filename()
        if mrl != None and mrl != "":
            tracklist.AddTrack("directory://" + mrl, True)
    update(0)

#basic control
def Next(widget):
    player.Next(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    update(0)

def Prev(widget):
    player.Prev(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    update(0)

def Stop(widget):
    player.Stop(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    update(0)

#update status display
def update(widget):
    item = tracklist.GetMetadata(tracklist.GetCurrentTrack())
    vol.set_value(player.VolumeGet())
    try: 
        a = item["artist"]
    except:        a = ""
    if a == "":
        a = item["URI"] 
    l_item.set_text(a)
    GetPlayStatus(0)

#get playing status from remote vlc
def GetPlayStatus(widget):
    global playing
    status = player.GetStatus()
    if status == 0:
        img_bt_toggle.set_from_stock("gtk-media-pause", gtk.ICON_SIZE_SMALL_TOOLBAR)
        playing = True
    else:
        img_bt_toggle.set_from_stock("gtk-media-play", gtk.ICON_SIZE_SMALL_TOOLBAR)
        playing = False

def Quit(widget):
    player.Quit(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    l_item.set_text("")

def Pause(widget):
    player.Pause()
    status = player.GetStatus()
    if status == 0:
        img_bt_toggle.set_from_stock(gtk.STOCK_MEDIA_PAUSE, gtk.ICON_SIZE_SMALL_TOOLBAR)
	gobject.timeout_add( 2000, timeset)
    else:
        img_bt_toggle.set_from_stock(gtk.STOCK_MEDIA_PLAY, gtk.ICON_SIZE_SMALL_TOOLBAR)
    update(0)

#callback for volume
def volchange(widget, data):
    player.VolumeSet(vol.get_value_as_int(), reply_handler=(lambda *args: None), error_handler=(lambda *args: None))

#callback for position change
def timechange(widget, x=None, y=None):
    player.PositionSet(int(time_s.get_value()), reply_handler=(lambda *args: None), error_handler=(lambda *args: None))

#refresh position
def timeset():
    global playing
    time_s.set_value(player.PositionGet())
    return playing

#simple/full display
def expander(widget):
    if exp.get_expanded() == False:
        exp.set_label("Less")
    else:
        exp.set_label("More")

#close event
def delete_event(self, widget):
    self.hide()
    return True

def destroy(widget):
    gtk.main_quit()

def key_release(widget, event):
    global position
    if event.keyval == gtk.keysyms.Escape:
        position = window.get_position()
        widget.hide()

#click on the tray icon
def tray_button(widget):
    global position
    if window.get_property('visible'):
        position = window.get_position()
        window.hide()
    else:
        window.move(position[0], position[1])
        window.show()

xml = gtk.glade.XML('dbus-vlc.glade')

bt_close    = xml.get_widget('close')
bt_quit     = xml.get_widget('quit')
bt_file     = xml.get_widget('ChooseFile')
bt_mrl      = xml.get_widget('AddMRL')
bt_next     = xml.get_widget('next')
bt_prev     = xml.get_widget('prev')
bt_stop     = xml.get_widget('stop')
bt_toggle   = xml.get_widget('toggle')
l_item      = xml.get_widget('item')
e_mrl       = xml.get_widget('mrl')
window      = xml.get_widget('window1')
img_bt_toggle=xml.get_widget('image6')
exp         = xml.get_widget('expander2')
expvbox     = xml.get_widget('expandvbox')
vlcicon     = xml.get_widget('eventicon')
vol         = xml.get_widget('vol')
time_s      = xml.get_widget('time_s')
time_l      = xml.get_widget('time_l')

window.connect('delete_event',  delete_event)
window.connect('destroy',       destroy)
window.connect('key_release_event', key_release)

tray = gtk.status_icon_new_from_icon_name("vlc")
tray.connect('activate', tray_button)

def icon_clicked(widget, event):
    update(0)

bt_close.connect('clicked',     destroy)
bt_quit.connect('clicked',      Quit)
bt_mrl.connect('clicked',       AddTrack)
bt_toggle.connect('clicked',    Pause)
bt_next.connect('clicked',      Next)
bt_prev.connect('clicked',      Prev)
bt_stop.connect('clicked',      Stop)
exp.connect('activate',         expander)
vol.connect('change-value',     volchange)
vol.connect('scroll-event',     volchange)
time_s.connect('adjust-bounds', timechange)
vlcicon.set_events(gtk.gdk.BUTTON_PRESS_MASK) 
vlcicon.connect('button_press_event', icon_clicked) 
time_s.set_update_policy(gtk.UPDATE_DISCONTINUOUS)
gobject.timeout_add( 2000, timeset)

library = "/media/mp3"

try:
    os.chdir(library)
    bt_file.set_current_folder(library)
except:
    print "edit this file to point to your media library"

window.set_icon_name('vlc')
window.set_title("VLC - D-Bus ctrl")
window.show()

try:
    update(0)
except:
    True

icon_theme = gtk.icon_theme_get_default()
try:
    pix = icon_theme.load_icon("vlc",24,0)
    window.set_icon(pix)
except:
    True
position = window.get_position()
vol.set_value(player.VolumeGet())

gtk.main()
