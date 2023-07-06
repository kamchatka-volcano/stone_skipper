#pragma once
#include <stdexcept>

namespace stone_skipper {

struct Error : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

} //namespace stone_skipper