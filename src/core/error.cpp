#include "error.h"

namespace suzume::core {

std::string_view errorCodeToString(ErrorCode code) {
  switch (code) {
    case ErrorCode::Success:
      return "Success";
    case ErrorCode::InvalidUtf8:
      return "InvalidUtf8";
    case ErrorCode::DictionaryLoadFailed:
      return "DictionaryLoadFailed";
    case ErrorCode::FileNotFound:
      return "FileNotFound";
    case ErrorCode::ParseError:
      return "ParseError";
    case ErrorCode::OutOfMemory:
      return "OutOfMemory";
    case ErrorCode::InvalidInput:
      return "InvalidInput";
    case ErrorCode::InternalError:
    default:
      return "InternalError";
  }
}

}  // namespace suzume::core
