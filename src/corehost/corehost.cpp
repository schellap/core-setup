// Copyright (c) .NET Foundation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "trace.h"
#include "utils.h"
#include "pal.h"
#include "fx_ver.h"
#include "hostfxr.h"
#include "error_codes.h"
#include "resources.h"

#if FEATURE_APPHOST
#define CURHOST_TYPE    _X("apphost")
#define CUREXE_PKG_VER APPHOST_PKG_VER
#else // !FEATURE_APPHOST
#define CURHOST_TYPE    _X("dotnet")
#define CUREXE_PKG_VER HOST_PKG_VER
#endif // !FEATURE_APPHOST

typedef int(*hostfxr_load_fn) (hostfxr_interface_t* init);
typedef int(*hostfxr_unload_fn) ();
typedef int(*hostfxr_main_fn) (const int argc, const pal::char_t* argv[]);

pal::string_t resolve_fxr_path(const pal::string_t& own_dir)
{
#if FEATURE_APPHOST
    pal::string_t fxr_path;
    if (library_exists_in_dir(own_dir, LIBFXR_NAME, &fxr_path))
    {
        trace::info(_X("Resolved fxr [%s]..."), fxr_path.c_str());
        return fxr_path;
    }
    return pal::string_t();
#else
    pal::string_t fxr_dir = own_dir;
    append_path(&fxr_dir, _X("host"));
    append_path(&fxr_dir, _X("fxr"));
    if (pal::directory_exists(fxr_dir))
    {
        trace::info(_X("Reading fx resolver directory=[%s]"), fxr_dir.c_str());

        std::vector<pal::string_t> list;
        pal::readdir(fxr_dir, &list);

        fx_ver_t max_ver(-1, -1, -1);
        for (const auto& dir : list)
        {
            trace::info(_X("Considering fxr version=[%s]..."), dir.c_str());

            pal::string_t ver = get_filename(dir);

            fx_ver_t fx_ver(-1, -1, -1);
            if (fx_ver_t::parse(ver, &fx_ver, false))
            {
                max_ver = std::max(max_ver, fx_ver);
            }
        }

        pal::string_t max_ver_str = max_ver.as_str();
        append_path(&fxr_dir, max_ver_str.c_str());
        trace::info(_X("Detected latest fxr version=[%s]..."), fxr_dir.c_str());

        pal::string_t ret_path;
        if (library_exists_in_dir(fxr_dir, LIBFXR_NAME, &ret_path))
        {
            trace::info(_X("Resolved fxr [%s]..."), ret_path.c_str());
            return ret_path;
        }
    }
    // TODO: Do not allow dotnet to load hostfxr side-by-side.
    pal::string_t fxr_path;
    if (library_exists_in_dir(own_dir, LIBFXR_NAME, &fxr_path))
    {
        trace::info(_X("Resolved fxr [%s]..."), fxr_path.c_str());
        return fxr_path;
    }
    return pal::string_t();
#endif
}

int run(const int argc, const pal::char_t* argv[])
{
    pal::string_t binding;
#ifdef FEATURE_APPHOST
    if (pal::apphost_can_run(&binding))
    {
        trace::error(_X("A fatal error was encountered. This executable was not bound to load a managed DLL."));
        return StatusCode::AppHostExeNotBoundFailure;
    }
#endif
    pal::string_t own_path;
    if (!pal::get_own_executable_path(&own_path) || !pal::realpath(&own_path))
    {
        trace::error(_X("Failed to resolve full path of the current executable [%s]"), own_path.c_str());
        return StatusCode::CoreHostCurExeFindFailure;
    }

    pal::dll_t fxr;

    pal::string_t own_dir = get_directory(own_path);

    // Load library
    pal::string_t fxr_path = resolve_fxr_path(own_dir);
    if (fxr_path.empty())
    {
        trace::error(_X("A fatal error occurred, the required library %s could not be found at %s"), LIBFXR_NAME, own_dir.c_str());
        return StatusCode::CoreHostLibMissingFailure;
    }

    if (!pal::load_library(fxr_path.c_str(), &fxr))
    {
        trace::error(_X("The library %s was found, but loading it from %s failed"), LIBFXR_NAME, fxr_path.c_str());
        trace::error(_X("  - Installing .NET Core prerequisites might help resolve this problem."));
        trace::error(_X("     %s"), DOTNET_CORE_URL);
        return StatusCode::CoreHostLibLoadFailure;
    }

    // pal::unload_library will be done by process exit.

    // Obtain the entrypoints.
    hostfxr_load_fn load_fn = (hostfxr_load_fn) pal::get_symbol(fxr, "hostfxr_load");
    hostfxr_unload_fn unload_fn = (hostfxr_unload_fn) pal::get_symbol(fxr, "hostfxr_unload");
    hostfxr_main_fn main_fn = (hostfxr_main_fn) pal::get_symbol(fxr, "hostfxr_main");

    // The load and unload exports were added to hostfxr.dll post 1.1.0 RTM.
    bool hostfxr_exports_load_api = (load_fn != nullptr && unload_fn != nullptr);

    if (!hostfxr_exports_load_api)
    {
        // Execute main.
        return main_fn(argc, argv);
    }
    else
    {
        // Initialize hostfxr.
        hostfxr_interface_t init;
        init.version_lo = HOSTFXR_INTERFACE_LAYOUT_VERSION_LO;
        init.version_hi = HOSTFXR_INTERFACE_LAYOUT_VERSION_HI;
        init.exe_version = _STRINGIFY(CUREXE_PKG_VER);
        init.exe_commit = _STRINGIFY(REPO_COMMIT_HASH);
        init.exe_type = CURHOST_TYPE; 
        init.exe_binding = binding.c_str();

        int ret_code = load_fn(&init);
        if (ret_code == 0)
        {
            // Execute main.
            ret_code = main_fn(argc, argv);

            // Unload notification.
            (void) unload_fn();
        }
        else
        {
            trace::error(_X("An error occurred during initialization of '%s'"), LIBFXR_NAME);
        }

        return ret_code;
    }
}

static char sccsid[] = "@(#)"            \
                       "version: "       \
                       CUREXE_PKG_VER    \
                       "; commit: "      \
                       REPO_COMMIT_HASH  \
                       "; built: "       \
                       __DATE__          \
                       " "               \
                       __TIME__          \
                       ;

#if defined(_WIN32)
int __cdecl wmain(const int argc, const pal::char_t* argv[])
#else
int main(const int argc, const pal::char_t* argv[])
#endif
{
    trace::setup();

    if (trace::is_enabled())
    {
        trace::info(_X("--- Invoked %s [version: %s, commit hash: %s] main = {"), CURHOST_TYPE, _STRINGIFY(CUREXE_PKG_VER), _STRINGIFY(REPO_COMMIT_HASH));
        for (int i = 0; i < argc; ++i)
        {
            trace::info(_X("%s"), argv[i]);
        }
        trace::info(_X("}"));
    }

    return run(argc, argv);
}

