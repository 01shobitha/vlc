/**
 * @file libvlc.cs
 * @brief libvlc CIL bindings
 *
 * $Id$
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace VideoLAN.LibVLC
{
    /**
     * The VLC class is used to create LibVLC Instance objects.
     * The VLC class has only one static method and cannot be instanciated.
     *
     * @code
     * string[] argv = new string[]{ "-vvv", "-I", "dummy" };
     *
     * Instance vlc = VLC.CreateInstance (argv);
     * @endcode
     */
    public sealed class VLC
    {
        /**
         * Loads native LibVLC and creates a LibVLC instance.
         *
         * @param args VLC command line parameters for the LibVLC Instance.
         *
         * @return a new LibVLC Instance
         */
        public static Instance CreateInstance (string[] args)
        {
            U8String[] argv = new U8String[args.Length];
            for (int i = 0; i < args.Length; i++)
                argv[i] = new U8String (args[i]);

            NativeException ex = new NativeException ();

            InstanceHandle h = InstanceHandle.Create (argv.Length, argv, ex);
            ex.Raise ();

            return new Instance (h);
        }
    };

    /**
     * Safe handle for unmanaged LibVLC instance pointer.
     */
    public sealed class InstanceHandle : NonNullHandle
    {
        private InstanceHandle ()
        {
        }

        [DllImport ("libvlc.dll", EntryPoint="libvlc_new")]
        internal static extern
        InstanceHandle Create (int argc, U8String[] argv, NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_release")]
        static extern void Destroy (IntPtr ptr, NativeException ex);

        /**
         * System.Runtime.InteropServices.SafeHandle::ReleaseHandle.
         */
        protected override bool ReleaseHandle ()
        {
            Destroy (handle, null);
            return true;
        }
    };

    /**
     * LibVLC Instance provides basic media player features from VLC,
     * such as play/pause/stop and flat playlist management.
     */
    public class Instance : BaseObject<InstanceHandle>
    {
        Dictionary<int, PlaylistItem> items;

        internal Instance (InstanceHandle self) : base (self)
        {
            items = new Dictionary<int, PlaylistItem> ();
        }

        /**
         * Creates a MediaDescriptor.
         * @param mrl Media Resource Locator (file path or URL)
         * @return create MediaDescriptor object.
         */
        public MediaDescriptor CreateDescriptor (string mrl)
        {
            U8String umrl = new U8String (mrl);
            DescriptorHandle dh = DescriptorHandle.Create (self, umrl, ex);
            ex.Raise ();

            return new MediaDescriptor (dh);
        }

        [DllImport ("libvlc.dll", EntryPoint="libvlc_playlist_loop")]
        static extern void PlaylistLoop (InstanceHandle self, bool b,
                                         NativeException ex);
        /** Sets the playlist loop flag. */
        public bool Loop
        {
            set
            {
                PlaylistLoop (self, value, ex);
                ex.Raise ();
            }
        }

        [DllImport ("libvlc.dll", EntryPoint="libvlc_playlist_play")]
        static extern void PlaylistPlay (InstanceHandle self, int id, int optc,
                                         U8String[] optv, NativeException ex);
        /** Plays the next playlist item (if not already playing). */
        public void Play ()
        {
            PlaylistPlay (self, -1, 0, new U8String[0], ex);
            ex.Raise ();
        }

        [DllImport ("libvlc.dll", EntryPoint="libvlc_playlist_pause")]
        static extern void PlaylistPause (InstanceHandle self,
                                          NativeException ex);
        /** Toggles pause (starts playing if stopped, pauses if playing). */
        public void TogglePause ()
        {
            PlaylistPause (self, ex);
            ex.Raise ();
        }

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_playlist_isplaying")]
        static extern int PlaylistIsPlaying (InstanceHandle self,
                                             NativeException ex);
        /** Whether the playlist is running, or paused/stopped. */
        public bool IsPlaying
        {
            get
            {
                int ret = PlaylistIsPlaying (self, ex);
                ex.Raise ();
                return ret != 0;
            }
        }

        [DllImport ("libvlc.dll", EntryPoint="libvlc_playlist_stop")]
        static extern void PlaylistStop (InstanceHandle self,
                                         NativeException ex);
        /** Stops playing. */
        public void Stop ()
        {
            PlaylistStop (self, ex);
            ex.Raise ();
        }

        [DllImport ("libvlc.dll", EntryPoint="libvlc_playlist_next")]
        static extern void PlaylistNext (InstanceHandle self,
                                         NativeException ex);
        /** Switches to next playlist item, and starts playing it. */
        public void Next ()
        {
            PlaylistNext (self, ex);
            ex.Raise ();
        }

        [DllImport ("libvlc.dll", EntryPoint="libvlc_playlist_prev")]
        static extern void PlaylistPrev (InstanceHandle self,
                                         NativeException ex);
        /** Switches to previous playlist item, and starts playing it. */
        public void Prev ()
        {
            PlaylistPrev (self, ex);
            ex.Raise ();
        }

        [DllImport ("libvlc.dll", EntryPoint="libvlc_playlist_clear")]
        static extern void PlaylistClear (InstanceHandle self,
                                          NativeException ex);
        /** Clears the whole playlist. */
        public void Clear ()
        {
            PlaylistClear (self, ex);
            ex.Raise ();

            foreach (PlaylistItem item in items.Values)
                item.Close ();
            items.Clear ();
        }

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_playlist_add_extended")]
        static extern int PlaylistAdd (InstanceHandle self, U8String uri,
                                       U8String name, int optc,
                                       U8String[] optv, NativeException e);
        /**
         * Appends an item to the playlist, with options.
         * @param mrl Media Resource Locator (file name or URL)
         * @param name playlist item user-visible name
         * @param opts item options (see LibVLC documentation for details)
         * @return created playlist item.
         */
        public PlaylistItem Add (string mrl, string name, string[] opts)
        {
            U8String umrl = new U8String (mrl);
            U8String uname = new U8String (name);
            U8String[] optv = new U8String[opts.Length];
            for (int i = 0; i < opts.Length; i++)
                optv[i] = new U8String (opts[i]);

            int id = PlaylistAdd (self, umrl, uname, optv.Length, optv, ex);
            ex.Raise ();

            PlaylistItem item = new PlaylistItem (id);
            items.Add (id, item);
            return item;
        }
        /**
         * Appends an item with options.
         * @param mrl Media Resource Locator (file name or URL)
         * @param opts item options (see LibVLC documentation for details)
         * @return created playlist item.
         */
        public PlaylistItem Add (string mrl, string[] opts)
        {
            return Add (mrl, null, opts);
        }
        /**
         * Appends an item to the playlist.
         * @param mrl Media Resource Locator (file name or URL)
         * @param name playlist item user-visible name
         * @return created playlist item.
         */
        public PlaylistItem Add (string mrl, string name)
        {
            return Add (mrl, name, new string[0]);
        }
        /**
         * Appends an item to the playlist.
         * @param mrl Media Resource Locator (file name or URL)
         * @return created playlist item.
         */
        public PlaylistItem Add (string mrl)
        {
            return Add (mrl, null, new string[0]);
        }

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_playlist_delete_item")]
        static extern int PlaylistDelete (InstanceHandle self, int id,
                                          NativeException e);
        /**
         * Removes an item from the playlist.
         * @param item playlist item (as obtained from Add())
         */
        public void Delete (PlaylistItem item)
        {
            int id = item.Id;
            PlaylistDelete (self, id, ex);
            ex.Raise ();

            item.Close ();
            items.Remove (id);
        }
    };

    /**
     * A playlist item.
     */
    public class PlaylistItem
    {
        int id;
        bool deleted;

        internal PlaylistItem (int id)
        {
            this.id = id;
            this.deleted = false;
        }

        internal void Close ()
        {
            deleted = true;
        }

        internal int Id
        {
            get
            {
                if (deleted)
                    throw new ObjectDisposedException ("Playlist item deleted");
                return id;
            }
        }
    };

    /** Safe handle for unmanaged LibVLC media descriptor */
    public sealed class DescriptorHandle : NonNullHandle
    {
        private DescriptorHandle ()
        {
        }

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_new")]
        public static extern
        DescriptorHandle Create (InstanceHandle inst, U8String mrl,
                                 NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_release")]
        public static extern void Release (IntPtr ptr);

        protected override bool ReleaseHandle ()
        {
            Release (handle);
            return true;
        }
    };

    /**
     * Media descriptor. Not implemented yet.
     */
    public class MediaDescriptor : BaseObject<DescriptorHandle>
    {
        internal MediaDescriptor (DescriptorHandle self) : base (self)
        {
        }
    };
};
