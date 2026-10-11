#include "termchart/core/expected.hpp"

#include <stdexcept>

namespace termchart {

void throw_bad_expected_access(const char* what) {
    throw std::logic_error(what);
}

}  // namespace termchart
