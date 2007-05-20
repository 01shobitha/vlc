/*****************************************************************************
 * entry.c : Callbacks for module entry point
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
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

#include <vlc/vlc.h>
#include <assert.h>

#include "modules/modules.h"
#include "libvlc.h"
#include "../libvlc.h"

static const char default_name[] = "unnamed";

module_t *vlc_module_create (vlc_object_t *obj)
{
    module_t *module =
        (module_t *)vlc_custom_create (obj, sizeof (module_t),
                                       VLC_OBJECT_MODULE, "module");
    if (module == NULL)
        return NULL;

#ifndef HAVE_SHARED_LIBVLC
    module->p_symbols = &vlc_global( obj )->p_module_bank->symbols;
#endif
    module->b_reentrant = module->b_unloadable = VLC_TRUE;
    module->psz_object_name = module->psz_longname = default_name;
    module->pp_shortcuts[0] = default_name;
    module->i_cpu = 0;
    module->psz_capability = "";
    module->i_score = 1;
    return module;
}


module_t *vlc_submodule_create (module_t *module)
{
    assert (module != NULL);
    assert (!module->b_submodule); // subsubmodules are not supported

    module_t *submodule =
        (module_t *)vlc_custom_create (VLC_OBJECT (module), sizeof (module_t),
                                       VLC_OBJECT_MODULE, "submodule");
    if (submodule == NULL)
        return NULL;

    vlc_object_attach (submodule, module);
    submodule->b_submodule = VLC_TRUE;

    /* Muahahaha! Heritage! Polymorphism! Ugliness!! */
    memcpy (submodule->pp_shortcuts, module->pp_shortcuts,
            sizeof (submodule->pp_shortcuts));

    submodule->psz_object_name = module->psz_object_name;
    submodule->psz_shortname = module->psz_shortname;
    submodule->psz_longname = module->psz_longname;
    submodule->psz_program = module->psz_program;
    submodule->psz_capability = module->psz_capability;
    submodule->i_score = module->i_score;
    submodule->i_cpu = module->i_cpu;
    return submodule;
}


int vlc_module_set (module_t *module, int propid, void *value)
{
    switch (propid)
    {
        case VLC_MODULE_CPU_REQUIREMENT:
            assert (!module->b_submodule);
            module->i_cpu |= (int)value;
            break;

        case VLC_MODULE_SHORTCUT:
        {
            unsigned i;
            for (i = 0; module->pp_shortcuts[i] != NULL; i++);
            if (i >= MODULE_SHORTCUT_MAX)
                return VLC_ENOMEM;

            module->pp_shortcuts[i] = (char *)value;
            break;
        }

        case VLC_MODULE_SHORTNAME:
            module->psz_shortname = (char *)value;
            break;

        case VLC_MODULE_DESCRIPTION:
            module->psz_longname = (char *)value;
            break;

        case VLC_MODULE_HELP:
            module->psz_help = (char *)value;
            break;

        case VLC_MODULE_CAPABILITY:
            module->psz_capability = (char *)value;
            break;

        case VLC_MODULE_SCORE:
            module->i_score = (int)value;
            break;

        case VLC_MODULE_PROGRAM:
            module->psz_program = (char *)value;
            break;

        case VLC_MODULE_CB_OPEN:
            module->pf_activate = (int (*) (vlc_object_t *))value;
            break;

        case VLC_MODULE_CB_CLOSE:
            module->pf_deactivate = (void (*) (vlc_object_t *))value;
            break;

        case VLC_MODULE_UNLOADABLE:
            module->b_unloadable = (value != NULL);
            break;

        case VLC_MODULE_NAME:
            module->pp_shortcuts[0] = module->psz_object_name = (char *)value;
            if (module->psz_longname == default_name)
                module->psz_longname = (char *)value;
            break;

        default:
            msg_Err (module, "unknown module property %d", propid);
            msg_Err (module, "LibVLC might be too old to use this module.");
            return VLC_EGENERIC;
    }
    return 0;
}
