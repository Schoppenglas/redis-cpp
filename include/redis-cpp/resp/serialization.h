//-------------------------------------------------------------------
//  redis-cpp
//  https://github.com/tdv/redis-cpp
//  Created:     03.2020
//  Copyright (C) 2020 tdv
//-------------------------------------------------------------------

#ifndef __REDISCPP_RESP_SERIALIZATION_H__
#define __REDISCPP_RESP_SERIALIZATION_H__

// STD
#include <cstdint>
#include <forward_list>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <variant>
#include <utility>

// REDIS-CPP
#include "redis-cpp/resp/detail/marker.h"
#include "redis-cpp/resp/detail/overloaded.h"

namespace rediscpp
{
inline namespace resp
{
namespace serialization
{

template <typename T>
void put(std::ostream &stream, T &&value)
{
    value.put(stream);
}

class simple_string final
{
public:
    simple_string(std::string_view value) noexcept
        : value_{std::move(value)}
    {
    }

    void put(std::ostream &stream)
    {
        stream << detail::marker::simple_string
               << value_
               << detail::marker::cr
               << detail::marker::lf;
    }

private:
    std::string_view value_;
};

class error_message final
{
public:
    error_message(std::string_view value) noexcept
        : value_{std::move(value)}
    {
    }

    void put(std::ostream &stream)
    {
        stream << detail::marker::error_message
               << value_
               << detail::marker::cr
               << detail::marker::lf;
    }

private:
    std::string_view value_;
};

template <typename T>
class integer final
{
public:
    static_assert(
            std::is_integral_v<std::decay_t<T>>,
            "The class \"rediscpp::resp::serialization::integer\" "
            "has to be created only from an integral type."
        );

    integer(T && value) noexcept
        : value_{std::move(value)}
    {
    }

    void put(std::ostream &stream)
    {
        stream << detail::marker::integer
               << static_cast<std::int64_t>(value_)
               << detail::marker::cr
               << detail::marker::lf;
    }

private:
    T value_;
};

class bulk_string final
{
public:
    bulk_string() noexcept
        : value_{}
    {
    }

    bulk_string(std::string_view value) noexcept
        : value_{std::move(value)}
    {
    }

    void put(std::ostream &stream)
    {
        if (value_.data())
        {
            stream << detail::marker::bulk_string
                   << value_.length()
                   << detail::marker::cr
                   << detail::marker::lf
                   << value_
                   << detail::marker::cr
                   << detail::marker::lf;
        }
        else
        {
            stream << detail::marker::bulk_string
                   << -1
                   << detail::marker::cr
                   << detail::marker::lf;
        }
    }

private:
    std::string_view value_;
};

class binary_data final
{
public:
    binary_data(void const *data, std::size_t length) noexcept
        : data_{data}
        , length_{length}
    {
    }

    void put(std::ostream &stream)
    {
        if (data_)
        {
            stream << detail::marker::bulk_string
                   << length_
                   << detail::marker::cr
                   << detail::marker::lf;

            stream.write(static_cast<std::ostream::char_type const *>(data_),
                    static_cast<std::streamsize>(length_));

            stream << detail::marker::cr
                   << detail::marker::lf;
        }
        else
        {
            stream << detail::marker::bulk_string
                   << -1
                   << detail::marker::cr
                   << detail::marker::lf;
        }
    }

private:
    void const *data_;
    std::size_t length_;
};

class null final
{
public:
    void put(std::ostream &stream)
    {
        serialization::put(stream, bulk_string{});
    }
};

template <typename ... T>
class array final
{
public:
    array(T && ... values) noexcept
        : values_{std::make_tuple(std::forward<T>(values) ... )}
    {
    }

    using list_type = std::forward_list<std::string_view>;

    array(list_type const &list)
        : values_{list}
    {
    }


    void put(std::ostream &stream)
    {
        std::visit(detail::overloaded{
               [&stream] (tuple_type &values)
               {
                   stream << detail::marker::array
                          << std::tuple_size_v<tuple_type>
                          << detail::marker::cr
                          << detail::marker::lf;

                   auto put_item = [&stream] (auto && arg)
                   {
                       serialization::put(stream, arg);
                   };

                   auto printer = [print = std::move(put_item)] (auto && ... args)
                   {
                       (print(args), ... );
                   };

                   std::apply(std::move(printer), values);
                },
                [&stream] (list_type &values)
                {
                    std::size_t count = 0;
                    for (auto const &i : values)
                        ++count;

                    stream << detail::marker::array
                           << count
                           << detail::marker::cr
                           << detail::marker::lf;

                    for (auto const &i : values)
                        serialization::put(stream, simple_string{i});
                }
            }, values_);
    }

private:
    using tuple_type = std::tuple<std::decay_t<T> ... >;
    using holder_type = std::variant<tuple_type, list_type>;
    holder_type values_;
    //std::tuple<std::decay_t<T> ... > values_;
};

template <>
class array<null> final
{
public:
    array(null const &)
    {
    }


    void put(std::ostream &stream)
    {
        stream << detail::marker::array
               << -1
               << detail::marker::cr
               << detail::marker::lf;
    }
};

}   // namespace serialization
}   // namespace resp
}   // namespace rediscpp

#endif  // !__REDISCPP_RESP_SERIALIZATION_H__