/// @file
///	@ingroup 	flux.artnet
///	@copyright	Copyright 2026 Sander Notenbaert. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "c74_min.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace flux::artnet {

inline bool atom_is_symbol(const c74::min::atom& value, const char* name)
{
    if (value.type() != c74::min::message_type::symbol_argument)
        return false;
    const c74::min::symbol comparison{static_cast<c74::max::t_symbol*>(value)};
    return comparison == name;
}

inline bool atom_to_int(const c74::min::atom& value, int& result)
{
    using c74::min::message_type;

    switch (value.type()) {
        case message_type::int_argument:
            result = static_cast<int>(value);
            return true;
        case message_type::float_argument: {
            const double number = static_cast<double>(value);
            if (!std::isfinite(number) || std::floor(number) != number)
                return false;
            if (number < static_cast<double>(std::numeric_limits<int>::min()) || number > static_cast<double>(std::numeric_limits<int>::max()))
                return false;
            result = static_cast<int>(number);
            return true;
        }
        default:
            return false;
    }
}

inline bool atom_to_number(const c74::min::atom& value, double& result)
{
    using c74::min::message_type;

    switch (value.type()) {
        case message_type::int_argument:
            result = static_cast<double>(static_cast<int>(value));
            return true;
        case message_type::float_argument:
            result = static_cast<double>(value);
            return std::isfinite(result);
        default:
            return false;
    }
}

inline bool atom_to_string(const c74::min::atom& value, std::string& result)
{
    if (value.type() != c74::min::message_type::symbol_argument)
        return false;
    const c74::min::symbol text{static_cast<c74::max::t_symbol*>(value)};
    result = static_cast<std::string>(text);
    return true;
}

inline bool atoms_to_ints(const c74::min::atoms& args, std::vector<int>& result, std::string& error)
{
    result.clear();
    result.reserve(args.size());

    for (std::size_t i = 0; i < args.size(); ++i) {
        int value = 0;
        if (!atom_to_int(args[i], value)) {
            error = "argument " + std::to_string(i + 1) + " is not an integer";
            return false;
        }
        result.push_back(value);
    }
    return true;
}

/// Parse DMX channel values from a message.
///
/// Integer atoms are treated as raw DMX values (0-255). Float atoms are
/// treated as normalized values, clipped to 0.0-1.0 and rounded to the nearest
/// DMX step. This lets a list mix integers and normalized floats.
inline bool atoms_to_dmx_values(const c74::min::atoms& args, std::vector<int>& result, std::string& error)
{
    result.clear();
    result.reserve(args.size());

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& value = args[i];

        switch (value.type()) {
            case c74::min::message_type::int_argument:
                result.push_back(static_cast<int>(value));
                break;
            case c74::min::message_type::float_argument: {
                const double normalized = static_cast<double>(value);
                if (!std::isfinite(normalized)) {
                    error = "channel value " + std::to_string(i + 1) + " is not finite";
                    return false;
                }
                const double clipped = std::clamp(normalized, 0.0, 1.0);
                result.push_back(static_cast<int>(std::lround(clipped * 255.0)));
                break;
            }
            default:
                error = "channel value " + std::to_string(i + 1) + " is not a number";
                return false;
        }
    }
    return true;
}

/// Parse a list of integers from either separate numeric atoms or a single
/// symbol containing whitespace- or comma-separated numbers. The symbol form
/// makes the list attributes robust when edited through a text field.
inline bool parse_int_list(const c74::min::atoms& args, std::vector<int>& result, std::string& error)
{
    result.clear();

    if (args.size() == 1 && args[0].type() == c74::min::message_type::symbol_argument) {
        std::string text;
        if (!atom_to_string(args[0], text)) {
            error = "expected an integer list";
            return false;
        }
        std::replace(text.begin(), text.end(), ',', ' ');

        std::istringstream stream{text};
        std::string token;
        while (stream >> token) {
            try {
                std::size_t consumed = 0;
                const long value = std::stol(token, &consumed);
                if (consumed != token.size())
                    throw std::invalid_argument("trailing characters");
                if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
                    throw std::out_of_range("integer out of range");
                result.push_back(static_cast<int>(value));
            }
            catch (const std::exception&) {
                error = "'" + token + "' is not an integer";
                return false;
            }
        }
        if (result.empty()) {
            error = "integer list is empty";
            return false;
        }
        return true;
    }

    return atoms_to_ints(args, result, error);
}

inline c74::min::atoms ints_to_atoms(const std::vector<int>& values)
{
    c74::min::atoms result;
    result.reserve(values.size());
    for (const int value : values)
        result.emplace_back(value);
    return result;
}

/// Render an integer list as a single symbol. Used as the stored value for
/// list attributes whose real value is exposed through a getter.
inline c74::min::symbol ints_to_symbol(const std::vector<int>& values)
{
    std::string text;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0)
            text += ' ';
        text += std::to_string(values[i]);
    }
    return c74::min::symbol(text);
}

} // namespace flux::artnet
