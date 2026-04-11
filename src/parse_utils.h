#pragma once
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cctype>

namespace utils {

    // 1. trim (in-place)
    inline void trim(std::string& s) {
        auto not_space = [](unsigned char c) {
            return !std::isspace(c);
        };

        auto start = std::find_if(s.begin(), s.end(), not_space);
        auto end   = std::find_if(s.rbegin(), s.rend(), not_space).base();

        if (start >= end) {
            s.clear();
        } else {
            s = std::string(start, end);
        }
    }

    // 2. strip comentário (#)
    inline void strip_comment(std::string& s) {
        auto pos = s.find('#');
        if (pos != std::string::npos) {
            s.erase(pos);
        }
    }

    // 3. to_lower (in-place)
    inline void to_lower(std::string& s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::tolower(c); });
    }

    // 4. split key:value
    inline bool split_kv(const std::string& line,
                        std::string& key,
                        std::string& val) {
        auto pos = line.find(':');
        if (pos == std::string::npos) return false;

        key = line.substr(0, pos);
        val = line.substr(pos + 1);

        trim(key);
        trim(val);
        return true;
    }

    inline int parse_int(const std::string& s) {
    try {
        return std::stoi(s);
    } catch (const std::invalid_argument&) {
        throw std::runtime_error("invalid numeric value: " + s);
    } catch (const std::out_of_range&) {
        throw std::runtime_error("out of range value: " + s);
    }
}
}
