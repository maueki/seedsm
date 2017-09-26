#pragma once

#include <string>

template <typename T>
std::string to_string(T st) {
    return std::to_string(static_cast<int>(st));
}
