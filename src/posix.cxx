#include <string>
#include <string_view>
#include <mutex>
#include <algorithm>
#include <vector>

#include <cstdlib>
#include <cstring>

#include "impl.hpp"
#include "red/session_impl.hpp"


extern "C" char** environ;

namespace {

    char const** argv__{ };
    int argc__{ };

    // https://gcc.gnu.org/onlinedocs/gcc-8.3.0/gcc/Common-Function-Attributes.html#index-constructor-function-attribute
    // https://stackoverflow.com/a/37012337
    [[gnu::constructor]]
    void init(int argc, const char** argv) {
        argc__ = argc;
        argv__ = argv;
    }

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

    bool environ_cache::contains(const std::string& key) const
    {
        return osenv_find_pos(key.data()) != -1;
    }

    std::string_view environ_cache::getvar(const std::string& key)
    {
        std::lock_guard _{ m_mtx };

        auto it = getenvstr_sync(key);
        if (it != myenv.end()) {
            std::string_view envstr = *it;
            return envstr.substr(key.size() + 1);
        }

        return {};
    }

    void environ_cache::setvar(const std::string& key, const std::string& value)
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

    void environ_cache::rmvar(const std::string& key)
    {
        std::lock_guard _{ m_mtx };

        auto it = getenvstr(key);
        if (it != myenv.end()) {
            myenv.erase(it);
        }

        unsetenv(key.data());
    }

	auto environ_cache::find(const std::string& key) const noexcept -> const_iterator
	{
		return std::find_if(myenv.cbegin(), myenv.cend(), envstr_finder<char>(key));
	}

    auto environ_cache::getenvstr(std::string_view key) noexcept -> iterator
    {
        return std::find_if(myenv.begin(), myenv.end(), envstr_finder<char>(key));
    }

    auto environ_cache::getenvstr_sync(std::string_view key) -> iterator
    {
        auto it_cache = getenvstr(key);
        const char* envstr_os = nullptr;

        int pos = osenv_find_pos(key.data());
        if (pos != -1) {
            envstr_os = environ[pos];
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
                std::string_view var_cache = *it_cache, envstr_view = envstr_os, var_os = envstr_view;

                // skip key=
                auto const offset = key.size() + 1;
                var_cache.remove_prefix(offset);
                var_os.remove_prefix(offset);

                // sync if values differ
                if (var_cache != var_os) {
                    *it_cache = envstr_view;
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
