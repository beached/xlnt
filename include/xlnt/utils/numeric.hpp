// Copyright (c) 2014-2021 Thomas Fussell
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE
//
// @license: http://www.opensource.org/licenses/mit-license.php
// @author: see AUTHORS file

#pragma once

#include <xlnt/xlnt_config.hpp>
#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#undef min
#undef max

namespace xlnt {
namespace detail {

/// <summary>
/// constexpr abs
/// </summary>
template <typename Number>
constexpr Number abs(Number val)
{
    return (val < Number{0}) ? -val : val;
}

/// <summary>
/// constexpr max
/// </summary>
template <typename NumberL, typename NumberR>
constexpr typename std::common_type<NumberL, NumberR>::type max(NumberL lval, NumberR rval)
{
    return (lval < rval) ? rval : lval;
}

/// <summary>
/// constexpr min
/// </summary>
template <typename NumberL, typename NumberR>
constexpr typename std::common_type<NumberL, NumberR>::type min(NumberL lval, NumberR rval)
{
    return (lval < rval) ? lval : rval;
}

/// <summary>
/// Floating point equality requires a bit of fuzzing due to the imprecise nature of fp calculation
/// References:
/// - Several blogs/articles were referenced with the following being the most useful
/// -- https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
/// -- http://realtimecollisiondetection.net/blog/?p=89
/// - Testing Frameworks {Catch2, Boost, Google}, primarily for selecting the default scale factor
/// -- None of these even remotely agree
/// </summary>
template <typename EpsilonType = float, // the type to extract epsilon from
    typename LNumber, typename RNumber> // parameter types (deduced)
bool float_equals(const LNumber &lhs, const RNumber &rhs,
    int epsilon_scale = 20) // scale the "fuzzy" equality. Higher value gives a more tolerant comparison
{
    // a type that lhs and rhs can agree on
    using common_t = typename std::common_type<LNumber, RNumber>::type;
    // asserts for sane usage
    static_assert(std::is_floating_point<LNumber>::value || std::is_floating_point<RNumber>::value,
        "Using this function with two integers is just wasting time. Use ==");
    static_assert(std::numeric_limits<EpsilonType>::epsilon() < EpsilonType{1},
        "epsilon >= 1.0 will cause all comparisons to return true");

    // NANs always compare false with themselves
    if (std::isnan(lhs) || std::isnan(rhs))
    {
        return false;
    }
    // epsilon type defaults to float because even if both args are a higher precision type
    // either or both could have been promoted by prior operations
    // if a higher precision is required, the template type can be changed
    constexpr common_t epsilon = static_cast<common_t>(std::numeric_limits<EpsilonType>::epsilon());
    // the "epsilon" then needs to be scaled into the comparison range
    // epsilon for numeric_limits is valid when abs(x) <1.0, scaling only needs to be upwards
    // in particular, this prevents a lhs of 0 from requiring an exact comparison
    // additionally, a scale factor is applied.
    common_t scaled_fuzz = epsilon_scale * epsilon * max(max(xlnt::detail::abs<common_t>(lhs),
                                                             xlnt::detail::abs<common_t>(rhs)), // |max| of parameters.
                               common_t{1}); // clamp
    return ((lhs + scaled_fuzz) >= rhs) && ((rhs + scaled_fuzz) >= lhs);
}

/// <summary>
/// Whitespace as matched by std::isspace in the "C" locale
/// </summary>
constexpr bool is_c_space(char c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

/// <summary>
/// Removes the leading whitespace and '+' that std::strto* accept but std::from_chars does not.
/// Returns false when the '+' is followed by a '-', which std::strto* rejects
/// </summary>
inline bool remove_strto_prefix(std::string_view &sv)
{
    while (!sv.empty() && is_c_space(sv.front()))
    {
        sv.remove_prefix(1);
    }
    if (!sv.empty() && sv.front() == '+')
    {
        sv.remove_prefix(1);
        return sv.empty() || sv.front() != '-';
    }
    return true;
}

/// <summary>
/// True when sv starts with "0x" or "0X" followed by a hex digit
/// </summary>
inline bool has_hex_prefix(std::string_view sv)
{
    if (sv.size() < 3 || sv[0] != '0' || (sv[1] != 'x' && sv[1] != 'X'))
    {
        return false;
    }
    const char c = sv[2];
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/// <summary>
/// Replacement for std::strtol/std::strtoul with the same results for Integer = long/unsigned long.
/// The sign is parsed separately and the magnitude as an unsigned integer, then:
/// - signed Integer: out of range values saturate to min/max (strtol)
/// - unsigned Integer: out of range values saturate to max, otherwise a '-' negates in unsigned arithmetic (strtoul)
/// Accepts leading whitespace, a sign and (base 16) a "0x" prefix. Returns 0 when no number is found
/// </summary>
template <typename Integer>
Integer parse_integer(std::string_view sv, int base = 10)
{
    static_assert(std::is_integral<Integer>::value, "parse_integer requires an integer type");
    using Unsigned = typename std::make_unsigned<Integer>::type;
    if (!remove_strto_prefix(sv))
    {
        return 0;
    }
    const bool negative = !sv.empty() && sv.front() == '-';
    if (negative)
    {
        sv.remove_prefix(1);
    }
    if (base == 16 && has_hex_prefix(sv))
    {
        sv.remove_prefix(2);
    }
    Unsigned magnitude = 0;
    auto result = std::from_chars(sv.data(), std::next(sv.data(), sv.size()), magnitude, base);
    if (result.ec == std::errc::invalid_argument)
    {
        return 0;
    }
    const bool overflow = result.ec == std::errc::result_out_of_range;
    if constexpr (std::is_signed<Integer>::value)
    {
        // |min| is one more than max
        const auto limit = static_cast<Unsigned>(std::numeric_limits<Integer>::max()) + (negative ? 1u : 0u);
        if (overflow || magnitude > limit)
        {
            return negative ? std::numeric_limits<Integer>::min() : std::numeric_limits<Integer>::max();
        }
    }
    else if (overflow)
    {
        return std::numeric_limits<Integer>::max();
    }
    // modular conversion back to Integer, well defined since C++20
    return static_cast<Integer>(negative ? static_cast<Unsigned>(0u - magnitude) : magnitude);
}

class number_serialiser
{
    static constexpr int Excel_Digit_Precision = 15; // sf

    // For an out of range number in [first, last) (unsigned, without "0x"), true if it overflowed
    // rather than underflowed. That is, if the position of its most significant digit is positive
    static bool is_overflow(const char *first, const char *last, bool hex)
    {
        const auto is_exponent = [hex](char c) { return hex ? (c == 'p' || c == 'P') : (c == 'e' || c == 'E'); };
        long long position = 0;
        bool seen_digit = false;
        bool seen_point = false;
        for (; first != last && !is_exponent(*first); ++first)
        {
            if (*first == '.')
            {
                seen_point = true;
            }
            else if (*first != '0' || seen_digit)
            {
                if (seen_point)
                {
                    break;
                }
                seen_digit = true;
                ++position;
            }
            else if (seen_point)
            {
                --position;
            }
        }
        while (first != last && !is_exponent(*first))
        {
            ++first;
        }
        if (first == last)
        {
            return position > 0;
        }
        ++first;
        if (first != last && *first == '+')
        {
            ++first;
        }
        long long exponent = 0;
        if (std::from_chars(first, last, exponent).ec == std::errc::result_out_of_range)
        {
            return *first != '-';
        }
        // each hex digit is 4 binary digits, matching the binary exponent of hex floats
        return (hex ? 4 : 1) * position + exponent > 0;
    }

public:
    // for printing to file.
    // This matches the output format of excel irrespective of current locale
    std::string serialise(double d) const
    {
        char buf[30];
        auto result = std::to_chars(buf, buf + sizeof(buf), d, std::chars_format::general, Excel_Digit_Precision);
        assert(result.ec == std::errc{});
        return std::string(buf, result.ptr);
    }

    // replacement for std::to_string / s*printf("%f", ...)
    // behaves same irrespective of locale
    std::string serialise_short(double d) const
    {
        // fixed notation of the largest double is ~310 characters
        char buf[std::numeric_limits<double>::max_exponent10 + 20];
        auto result = std::to_chars(buf, buf + sizeof(buf), d, std::chars_format::fixed, 6);
        assert(result.ec == std::errc{});
        return std::string(buf, result.ptr);
    }

    // std::from_chars based replacement for std::strtod, accepting the same input.
    // *len_converted is the number of characters of s consumed, 0 if s does not start with a number
    double deserialise(const std::string &s, ptrdiff_t *len_converted) const
    {
        assert(!s.empty());
        assert(len_converted != nullptr);
        *len_converted = 0;
        std::string_view sv(s);
        if (!remove_strto_prefix(sv) || sv.empty())
        {
            return 0.0;
        }
        const bool negative = sv.front() == '-';
        const char *const last = std::next(sv.data(), sv.size());
        double d = 0.0;
        std::from_chars_result result{};
        bool hex = false;
        if (has_hex_prefix(sv.substr(negative ? 1 : 0)))
        {
            // from_chars does not accept the "0x" prefix, or a sign after it
            const char *const digits = std::next(sv.data(), (negative ? 3 : 2));
            if (*digits != '-')
            {
                result = std::from_chars(digits, last, d, std::chars_format::hex);
                hex = result.ec != std::errc::invalid_argument;
            }
        }
        if (!hex)
        {
            result = std::from_chars(sv.data(), last, d);
        }
        if (result.ec == std::errc::invalid_argument)
        {
            return 0.0;
        }
        *len_converted = result.ptr - s.data();
        if (result.ec == std::errc::result_out_of_range)
        {
            // match std::strtod, which returns HUGE_VAL on overflow and 0 on underflow
            const char *const number = std::next(sv.data(), (negative ? 1 : 0) + (hex ? 2 : 0));
            d = is_overflow(number, result.ptr, hex) ? HUGE_VAL : 0.0;
        }
        else if (!hex)
        {
            return d; // from_chars has already applied the sign
        }
        return negative ? -d : d;
    }

    double deserialise(const std::string &s) const
    {
        ptrdiff_t ignore;
        return deserialise(s, &ignore);
    }
};

} // namespace detail
} // namespace xlnt
