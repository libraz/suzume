#ifndef SUZUME_CORE_ERROR_H_
#define SUZUME_CORE_ERROR_H_

#include <string>
#include <string_view>
#include <variant>

namespace suzume::core {

/**
 * @brief Error codes
 */
enum class ErrorCode {
  Success = 0,
  InvalidUtf8,           // Invalid UTF-8 input
  DictionaryLoadFailed,  // Dictionary load failed
  FileNotFound,          // File not found
  ParseError,            // Parse error
  OutOfMemory,           // Out of memory
  InvalidInput,          // Invalid input
  InternalError          // Internal error
};

/**
 * @brief Error information
 */
struct Error {
  ErrorCode code;
  std::string message;

  Error(ErrorCode err_code, std::string msg = "")
      : code(err_code), message(std::move(msg)) {}

  bool isSuccess() const { return code == ErrorCode::Success; }
  explicit operator bool() const { return !isSuccess(); }
};

/**
 * @brief Result type representing success or failure
 *
 * Usage:
 *   Result<std::string> normalize(std::string_view input);
 *   auto result = normalize("test");
 *   if (auto* value = std::get_if<std::string>(&result)) {
 *     // Success: use *value
 *   } else {
 *     // Failure: std::get<Error>(result)
 *   }
 */
template <typename T>
using Result = std::variant<T, Error>;

/**
 * @brief Get value pointer from Result (on success)
 */
template <typename T>
const T* getValuePtr(const Result<T>& result) {
  return std::get_if<T>(&result);
}

/**
 * @brief Get mutable value pointer from Result (on success)
 */
template <typename T>
T* getValuePtr(Result<T>& result) {
  return std::get_if<T>(&result);
}

/**
 * @brief Get error pointer from Result (on failure)
 */
template <typename T>
const Error* getErrorPtr(const Result<T>& result) {
  return std::get_if<Error>(&result);
}

/**
 * @brief Check if Result is success
 */
template <typename T>
bool isSuccess(const Result<T>& result) {
  return std::holds_alternative<T>(result);
}

/**
 * @brief Convert error code to string
 */
std::string_view errorCodeToString(ErrorCode code);

// Error code aliases for consistency
constexpr ErrorCode kInvalidArgument = ErrorCode::InvalidInput;
constexpr ErrorCode kFileNotFound = ErrorCode::FileNotFound;

/**
 * @brief Expected type for error handling (similar to std::expected)
 *
 * Usage:
 *   Expected<std::string, Error> normalize(std::string_view input);
 *   auto result = normalize("test");
 *   if (result.hasValue()) {
 *     // use result.value()
 *   } else {
 *     // use result.error()
 *   }
 */
template <typename E>
struct Unexpected;

template <typename T, typename E>
class Expected {
 public:
  Expected(const T& value) : data_(value), has_value_(true) {}
  Expected(T&& value) : data_(std::move(value)), has_value_(true) {}
  Expected(const E& error) : error_(error), has_value_(false) {}
  Expected(E&& error) : error_(std::move(error)), has_value_(false) {}

  // Conversion from Unexpected
  Expected(const Unexpected<E>& unexp) : error_(unexp.error), has_value_(false) {}
  Expected(Unexpected<E>&& unexp)
      : error_(std::move(unexp).error), has_value_(false) {}

  bool hasValue() const { return has_value_; }
  explicit operator bool() const { return has_value_; }

  const T& value() const& { return data_; }
  T& value() & { return data_; }
  T&& value() && { return std::move(data_); }

  const E& error() const& { return error_; }
  E& error() & { return error_; }

 private:
  T data_{};
  E error_{ErrorCode::Success};
  bool has_value_;
};

/**
 * @brief Create unexpected (error) result
 */
template <typename E>
struct Unexpected {
  E error;
  explicit Unexpected(E err) : error(std::move(err)) {}
};

template <typename E>
Unexpected<E> makeUnexpected(E error) {
  return Unexpected<E>(std::move(error));
}

}  // namespace suzume::core

#endif  // SUZUME_CORE_ERROR_H_
