#ifndef RED_SESSION_HPP
#define RED_SESSION_HPP

#include <type_traits>
#include <string_view>
#include <utility>

#include <range/v3/view/split.hpp>

#include "ranges.hpp"

namespace red::session {

    class environment
    {
    public:
        class variable
        {
        public:
            class splitpath_t;
        
            operator std::string() const { return m_value; }
            std::string_view key() const noexcept { return m_key; }
            std::string_view value() const noexcept { return m_value; }
            splitpath_t split () const;

            explicit variable(std::string_view key_) : m_key(key_) {
                m_value = sys::getenv(key_);
            }
            variable& operator=(std::string_view value) {
                sys::setenv(m_key, value);
                m_value.assign(value);
                return *this;
            }

        private:
            std::string m_key, m_value;
        };

        using iterator = detail::environment_iterator;
        using value_type = variable;
        using size_type = size_t;
        // using value_range = decltype(detail::environ_keyval(*this,false));
        // using key_range = value_range;
        friend class variable;

        template <class T>
        using Is_Strview = std::enable_if_t<
            std::conjunction_v<
                std::is_convertible<const T&, std::string_view>, 
                std::negation<std::is_convertible<const T&, const char*>>
            >
        >;

        template <class T>
        using Is_Strview_Convertible = std::enable_if_t<std::is_convertible_v<const T&, std::string_view>>;


        environment() noexcept;

        template <class T, class = Is_Strview<T>>
        variable operator [] (T const& k) const { return variable(k); }
        variable operator [] (std::string_view k) const { return variable(k); }
        // variable operator [] (std::string const& k) const { return variable(k); }
        // variable operator [] (char const* k) const { return variable(k); }

        template <class K, class = Is_Strview_Convertible<K>>
        iterator find(K const& key) const noexcept { return do_find(key); }

        bool contains(std::string_view key) const;

        iterator cbegin() const noexcept { return detail::environ_iterator(sys::envp()); }
        iterator cend() const noexcept { return ranges::default_sentinel; }
        auto begin() const noexcept { return cbegin(); }
        auto end() const noexcept { return cend(); }

        size_type size() const noexcept;
		[[nodiscard]] bool empty() const noexcept { return size() == 0; }

        template <class K, class = Is_Strview_Convertible<K>>
        void erase(K const& key) { do_erase(key); }

        /*value_range*/ auto values() const noexcept {
            return detail::environ_keyval(*this, false);
        }
        /*key_range*/ auto keys() const noexcept {
            return detail::environ_keyval(*this, true);
        }
    private:
        void do_erase(std::string_view key);
        iterator do_find(std::string_view k) const;
    };

    class environment::variable::splitpath_t
    {
        using range_t = ranges::split_view<ranges::views::all_t<std::string_view>, ranges::single_view<char>>;
        std::string m_value;
        range_t rng;

    public:
        splitpath_t(std::string_view val) : m_value(val) {
            rng = range_t(m_value, sys::path_sep);
        }

        auto begin() {
            return rng.begin();
        }
        auto end() {
            return rng.end();
        }
    };


    class arguments
    {
    public:
        using iterator = char const* const*;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using value_type = std::string_view;
        using index_type = size_t;
        using size_type = size_t;

        value_type operator [] (index_type) const noexcept;
        value_type at(index_type) const;

        [[nodiscard]] bool empty() const noexcept { return size() == 0; }
        size_type size() const noexcept;

        iterator cbegin() const noexcept;
        iterator cend() const noexcept;

        iterator begin() const noexcept { return cbegin(); }
        iterator end() const noexcept { return cend(); }

        reverse_iterator crbegin() const noexcept { return reverse_iterator{ cend() }; }
        reverse_iterator crend() const noexcept { return reverse_iterator{ cbegin() }; }

        reverse_iterator rbegin() const noexcept { return crbegin(); }
        reverse_iterator rend() const noexcept { return crend(); }

        [[nodiscard]] const char** argv() const noexcept;
        [[nodiscard]] int argc() const noexcept;
    };

    template<class Iter>
    std::string join_paths(Iter begin, Iter end);

    template<class Range>
    std::string join_paths(Range rng);

#if defined(SESSION_NOEXTENTIONS)
    void init_args(int argc, const char** argv);
#endif

} /* namespace red::session */

#endif /* RED_SESSION_HPP */