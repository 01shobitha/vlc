/*****************************************************************************
 * libvlc.h: Internal libvlc generic/misc declaration
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 the VideoLAN team
 * Copyright © 2006-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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

#ifndef LIBVLC_LIBVLC_H
# define LIBVLC_LIBVLC_H 1

extern const char vlc_usage[];

/* Hotkey stuff */
extern const struct hotkey libvlc_hotkeys[];
extern const size_t libvlc_hotkeys_size;
extern int vlc_key_to_action (vlc_object_t *, const char *,
                              vlc_value_t, vlc_value_t, void *);

/*
 * OS-specific initialization
 */
void system_Init      ( libvlc_int_t *, int *, const char *[] );
void system_Configure ( libvlc_int_t *, int *, const char *[] );
void system_End       ( libvlc_int_t * );

#if defined( SYS_BEOS )
/* Nothing at the moment, create beos_specific.h when needed */
#elif defined( __APPLE__ )
/* Nothing at the moment, create darwin_specific.h when needed */
#elif defined( WIN32 ) || defined( UNDER_CE )
VLC_EXPORT( const char * , system_VLCPath, (void));
#else
# define system_Init( a, b, c )      (void)0
# define system_Configure( a, b, c ) (void)0
# define system_End( a )             (void)0
#endif


/*
 * Threads subsystem
 */
int vlc_threads_init( void );
void vlc_threads_end( void );

/** The global thread var for msg stack context
 *  We store this as a static global variable so we don't need a vlc_object_t
 *  everywhere.
 *  This key is created in vlc_threads_init and is therefore ready to use at
 *  the very beginning of the universe */
extern vlc_threadvar_t msg_context_global_key;

/*
 * CPU capabilities
 */
extern uint32_t cpu_flags;
uint32_t CPUCapabilities( void );

/*
 * Unicode stuff
 */

/*
 * LibVLC objects stuff
 */

/**
 * Creates a VLC object.
 *
 * Note that because the object name pointer must remain valid, potentially
 * even after the destruction of the object (through the message queues), this
 * function CANNOT be exported to plugins as is. In this case, the old
 * vlc_object_create() must be used instead.
 *
 * @param p_this an existing VLC object
 * @param i_size byte size of the object structure
 * @param i_type object type, usually VLC_OBJECT_CUSTOM
 * @param psz_type object type name
 * @return the created object, or NULL.
 */
extern void *
vlc_custom_create (vlc_object_t *p_this, size_t i_size, int i_type,
                   const char *psz_type);

/**
 * libvlc_global_data_t (global variable)
 *
 * This structure has an unique instance, statically allocated in libvlc and
 * never accessed from the outside. It stores process-wide VLC variables,
 * mostly process-wide locks, and (currently) the module bank and objects tree.
 */
typedef struct libvlc_global_data_t
{
    VLC_COMMON_MEMBERS

   /* Object structure data */
    int                    i_counter;   ///< object counter
    int                    i_objects;   ///< Attached objects count
    vlc_object_t **        pp_objects;  ///< Array of all objects

    module_bank_t *        p_module_bank; ///< The module bank

    /* Arch-specific variables */
#if defined( SYS_BEOS )
    char *                 psz_vlcpath;
#elif defined( __APPLE__ )
    char *                 psz_vlcpath;
#elif defined( WIN32 )
    char *                 psz_vlcpath;
#endif
} libvlc_global_data_t;


libvlc_global_data_t *vlc_global (void);
libvlc_int_t *vlc_current_object (int i_object);

/**
 * Private LibVLC data for each object.
 */
struct vlc_object_internals_t
{
    /* Object variables */
    variable_t *    p_vars;
    vlc_mutex_t     var_lock;
    int             i_vars;

    /* Thread properties, if any */
    vlc_thread_t    thread_id;
    bool            b_thread;

    /* Objects thread synchronization */
    int             pipes[2];
    vlc_spinlock_t  spin;

    /* Objects management */
    unsigned         i_refcount;
    vlc_destructor_t pf_destructor;
    bool             b_attached;
};

#define ZOOM_SECTION N_("Zoom")
#define ZOOM_QUARTER_KEY_TEXT N_("1:4 Quarter")
#define ZOOM_HALF_KEY_TEXT N_("1:2 Half")
#define ZOOM_ORIGINAL_KEY_TEXT N_("1:1 Original")
#define ZOOM_DOUBLE_KEY_TEXT N_("2:1 Double")

static inline vlc_object_internals_t *vlc_internals( vlc_object_t *obj )
{
    return ((vlc_object_internals_t *)obj) - 1;
}

/**
 * Private LibVLC instance data.
 */
typedef struct libvlc_priv_t
{
    vlc_mutex_t        config_lock; ///< config file lock

    vlc_mutex_t        timer_lock;  ///< Lock to protect timers
    counter_t        **pp_timers;   ///< Array of all timers
    int                i_timers;    ///< Number of timers
    bool               b_stats;     ///< Whether to collect stats
} libvlc_priv_t;

static inline libvlc_priv_t *libvlc_priv (libvlc_int_t *libvlc)
{
    return (libvlc_priv_t *)(libvlc + 1);
}

static inline bool libvlc_stats (vlc_object_t *obj)
{
   return libvlc_priv (obj->p_libvlc)->b_stats;
}

/**
 * LibVLC "main module" configuration settings array.
 */
extern module_config_t libvlc_config[];
extern const size_t libvlc_config_count;

/*
 * Variables stuff
 */
void var_OptionParse (vlc_object_t *, const char *, bool trusted);

#endif
