#include <string>
#include <string_view>
#include <mutex>
#include <algorithm>
#include <vector>

#include <cstdlib>
#include <cstring>

#include "impl.hpp"
#include "red/session_impl.hpp"

#if defined(__ELF__) and __ELF__
    #define SESSION_IMPL_SECTION ".init_array"
    #define SESSION_IMPL_ELF 1
#elif defined(__MACH__) and __MACH__
    #define SESSION_IMPL_SECTION "__DATA,__mod_init_func"
    #define SESSION_IMPL_ELF 0
#endif

using namespace std::literals;

extern "C" char** environ;

namespace {

    char const** argv__{ };
    int argc__{ };

    [[gnu::section(SESSION_IMPL_SECTION)]]
    auto init = +[](int argc, char const** argv, char const**) {
        argv__ = argv;
        argc__ = argc;
        if constexpr (SESSION_IMPL_ELF) { return 0; }
    };

} /* nameless namespace */


namespace impl {

    char const* argv(std::size_t idx) noexcept { return argv__[idx]; }
    char const** argv() noexcept { return argv__; }
    int argc() noexcept { return argc__; }

    const char path_sep = ':';

    int osenv_find_pos(const char* k)
    {
        auto predicate = envstr_finder<char>(k);

        for (int i = 0; environ[i]; i++)
        {
            if (predicate(environ[i])) return i;
        }

        return -1;
    }

} /* namespace impl */

namespace red::session::detail
{
    using namespace impl;

    environ_cache::environ_cache()
    {
        try
        {
            size_t envsize = osenv_size(environ);
            myenv.insert(myenv.end(), environ, environ + envsize);
        }
        catch (const std::exception&)
        {
            std::throw_with_nested(std::runtime_error("Failed to create environment"));
        }
    }

    environ_cache::~environ_cache() noexcept
    {
    }

    bool environ_cache::contains(std::string_view key) const
    {
        return osenv_find_pos(key.data()) != -1;
    }

    std::string_view environ_cache::getvar(std::string_view key)
    {
        std::lock_guard _{ m_mtx };

        auto it = getenvstr_sync(key);
        if (it != myenv.end()) {
            std::string_view envstr = *it;
            return envstr.substr(key.size() + 1);
        }

        return {};
    }

    void environ_cache::setvar(std::string_view key, std::string_view value)
    {
        std::lock_guard _{ m_mtx };

        auto it = getenvstr(key);
        auto envstr = make_envstr(key, value);

        if (it != myenv.end()) {
            *it = std::move(envstr);
        }
        else {
            myenv.push_back(std::move(envstr));
        }

        setenv(key.data(), value.data(), true);
    }

    void environ_cache::rmvar(std::string_view key)
    {
        std::lock_guard _{ m_mtx };

        auto it = getenvstr(key);
        if (it != myenv.end()) {
            myenv.erase(it);
        }

        unsetenv(key.data());
    }


    auto environ_cache::getenvstr(std::string_view key) noexcept -> iterator
    {
        return std::find_if(myenv.begin(), myenv.end(), envstr_finder<char>(key));
    }

    auto environ_cache::getenvstr_sync(std::string_view key) -> iterator
    {
        auto it_cache = getenvstr(key);
        const char* envstr_os = nullptr;

        int offset = osenv_find_pos(key.data());
        if (offset != -1) {
            envstr_os = environ[offset];
        }

        if (it_cache == myenv.end() && envstr_os)
        {
            // not found in cache, found in OS env
            it_cache = myenv.insert(myenv.end(), envstr_os);
        }
        else if (it_cache != myenv.end())
        {
            if (envstr_os)
            {
                // found in both
                std::string_view var_cache = *it_cache, var_os = envstr_os;

                // skip key=
                auto const offset = key.size() + 1;
                var_cache.remove_prefix(offset);
                var_os.remove_prefix(offset);

                // sync if values differ
                if (var_cache != var_os) {
                    *it_cache = envstr_os;
                }
            }
            else
            {
                // found in cache, not in OS. Remove
                myenv.erase(it_cache);
                it_cache = myenv.end();
            }
        }

        return it_cache;
    }

}
