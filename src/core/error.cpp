#include "termchart/core/error.hpp"

namespace termchart {

const char* to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok:
            return "Ok";
        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::OutOfRange:
            return "OutOfRange";
        case ErrorCode::Unsupported:
            return "Unsupported";
        case ErrorCode::NotImplemented:
            return "NotImplemented";
        case ErrorCode::NotInitialized:
            return "NotInitialized";
        case ErrorCode::MalformedUtf8:
            return "MalformedUtf8";
        case ErrorCode::ParseError:
            return "ParseError";
        case ErrorCode::IoError:
            return "IoError";
        case ErrorCode::TerminalUnavailable:
            return "TerminalUnavailable";
        case ErrorCode::PlatformError:
            return "PlatformError";
    }
    return "Unknown";
}

const char* describe(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok:
            return "success";
        case ErrorCode::InvalidArgument:
            return "argument is invalid for this operation";
        case ErrorCode::OutOfRange:
            return "index or coordinate is out of range";
        case ErrorCode::Unsupported:
            return "operation not supported by the current configuration";
        case ErrorCode::NotImplemented:
            return "operation is not implemented yet";
        case ErrorCode::NotInitialized:
            return "subsystem is not initialized (call quick::init_core first)";
        case ErrorCode::MalformedUtf8:
            return "input contains a malformed UTF-8 sequence";
        case ErrorCode::ParseError:
            return "input does not match the expected grammar";
        case ErrorCode::IoError:
            return "input/output error";
        case ErrorCode::TerminalUnavailable:
            return "no usable terminal on stdout/stdin";
        case ErrorCode::PlatformError:
            return "underlying platform call failed";
    }
    return "unknown error";
}

}  // namespace termchart
