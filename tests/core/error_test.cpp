#include "core/error.h"

#include <gtest/gtest.h>

namespace suzume {
namespace core {
namespace {

// =============================================================================
// errorCodeToString - all ErrorCode values
// =============================================================================

TEST(ErrorTest, ErrorCodeToStringAllValues) {
  EXPECT_EQ(errorCodeToString(ErrorCode::Success), "Success");
  EXPECT_EQ(errorCodeToString(ErrorCode::InvalidUtf8), "InvalidUtf8");
  EXPECT_EQ(errorCodeToString(ErrorCode::DictionaryLoadFailed), "DictionaryLoadFailed");
  EXPECT_EQ(errorCodeToString(ErrorCode::FileNotFound), "FileNotFound");
  EXPECT_EQ(errorCodeToString(ErrorCode::ParseError), "ParseError");
  EXPECT_EQ(errorCodeToString(ErrorCode::OutOfMemory), "OutOfMemory");
  EXPECT_EQ(errorCodeToString(ErrorCode::InvalidInput), "InvalidInput");
  EXPECT_EQ(errorCodeToString(ErrorCode::InternalError), "InternalError");
}

// =============================================================================
// Error struct construction
// =============================================================================

TEST(ErrorTest, ErrorConstructionWithMessage) {
  Error err(ErrorCode::InvalidUtf8, "bad input");
  EXPECT_EQ(err.code, ErrorCode::InvalidUtf8);
  EXPECT_EQ(err.message, "bad input");
}

TEST(ErrorTest, ErrorConstructionDefaultMessage) {
  Error err(ErrorCode::FileNotFound);
  EXPECT_EQ(err.code, ErrorCode::FileNotFound);
  EXPECT_EQ(err.message, "");
}

TEST(ErrorTest, ErrorIsSuccess) {
  Error success(ErrorCode::Success);
  EXPECT_TRUE(success.isSuccess());

  Error failure(ErrorCode::ParseError);
  EXPECT_FALSE(failure.isSuccess());
}

TEST(ErrorTest, ErrorBoolConversion) {
  Error success(ErrorCode::Success);
  // operator bool returns true when NOT success (i.e., error state)
  EXPECT_FALSE(static_cast<bool>(success));

  Error failure(ErrorCode::InternalError, "something broke");
  EXPECT_TRUE(static_cast<bool>(failure));
}

// =============================================================================
// Error code aliases
// =============================================================================

TEST(ErrorTest, ErrorCodeAliases) {
  EXPECT_EQ(kInvalidArgument, ErrorCode::InvalidInput);
  EXPECT_EQ(kFileNotFound, ErrorCode::FileNotFound);
}

// =============================================================================
// Expected<T, E> - value path
// =============================================================================

TEST(ErrorTest, ExpectedWithValue) {
  Expected<std::string, Error> result(std::string("hello"));
  EXPECT_TRUE(result.hasValue());
  EXPECT_TRUE(static_cast<bool>(result));
  EXPECT_EQ(result.value(), "hello");
}

TEST(ErrorTest, ExpectedWithMovedValue) {
  std::string val = "moved";
  Expected<std::string, Error> result(std::move(val));
  EXPECT_TRUE(result.hasValue());
  EXPECT_EQ(result.value(), "moved");
}

TEST(ErrorTest, ExpectedValueMutableAccess) {
  Expected<std::string, Error> result(std::string("original"));
  result.value() = "modified";
  EXPECT_EQ(result.value(), "modified");
}

TEST(ErrorTest, ExpectedValueRvalueAccess) {
  Expected<std::string, Error> result(std::string("rvalue"));
  std::string moved = std::move(result).value();
  EXPECT_EQ(moved, "rvalue");
}

// =============================================================================
// Expected<T, E> - error path
// =============================================================================

TEST(ErrorTest, ExpectedWithError) {
  Error err(ErrorCode::InvalidUtf8, "bad encoding");
  Expected<std::string, Error> result(err);
  EXPECT_FALSE(result.hasValue());
  EXPECT_FALSE(static_cast<bool>(result));
  const auto& ref = result;
  EXPECT_EQ(ref.error().code, ErrorCode::InvalidUtf8);
  EXPECT_EQ(ref.error().message, "bad encoding");
}

TEST(ErrorTest, ExpectedWithMovedError) {
  Error moved_err(ErrorCode::ParseError, "syntax");
  Expected<std::string, Error> result{std::move(moved_err)};
  EXPECT_FALSE(result.hasValue());
  const auto& ref = result;
  EXPECT_EQ(ref.error().code, ErrorCode::ParseError);
}

TEST(ErrorTest, ExpectedErrorMutableAccess) {
  Error init_err(ErrorCode::OutOfMemory);
  Expected<std::string, Error> result{init_err};
  result.error().message = "updated";
  EXPECT_EQ(result.error().message, "updated");
}

// =============================================================================
// Unexpected and makeUnexpected
// =============================================================================

TEST(ErrorTest, UnexpectedConstruction) {
  Unexpected<Error> unexp(Error(ErrorCode::FileNotFound, "missing"));
  EXPECT_EQ(unexp.error.code, ErrorCode::FileNotFound);
  EXPECT_EQ(unexp.error.message, "missing");
}

TEST(ErrorTest, MakeUnexpected) {
  auto unexp = makeUnexpected(Error(ErrorCode::InternalError, "crash"));
  EXPECT_EQ(unexp.error.code, ErrorCode::InternalError);
}

TEST(ErrorTest, ExpectedFromUnexpected) {
  auto unexp = makeUnexpected(Error(ErrorCode::InvalidInput, "wrong"));
  Expected<int, Error> result(unexp);
  EXPECT_FALSE(result.hasValue());
  const auto& ref = result;
  EXPECT_EQ(ref.error().code, ErrorCode::InvalidInput);
}

TEST(ErrorTest, ExpectedFromMovedUnexpected) {
  Expected<int, Error> result(makeUnexpected(Error(ErrorCode::ParseError, "bad")));
  EXPECT_FALSE(result.hasValue());
  const auto& ref = result;
  EXPECT_EQ(ref.error().code, ErrorCode::ParseError);
  EXPECT_EQ(ref.error().message, "bad");
}

// =============================================================================
// Result<T> helpers (getValuePtr, getErrorPtr, isSuccess)
// =============================================================================

TEST(ErrorTest, ResultSuccessPath) {
  Result<std::string> result = std::string("test value");
  EXPECT_TRUE(isSuccess(result));

  const auto* val_ptr = getValuePtr(result);
  ASSERT_NE(val_ptr, nullptr);
  EXPECT_EQ(*val_ptr, "test value");

  const auto* err_ptr = getErrorPtr(result);
  EXPECT_EQ(err_ptr, nullptr);
}

TEST(ErrorTest, ResultErrorPath) {
  Result<std::string> result = Error(ErrorCode::InvalidUtf8, "bad bytes");
  EXPECT_FALSE(isSuccess(result));

  const auto* val_ptr = getValuePtr(result);
  EXPECT_EQ(val_ptr, nullptr);

  const auto* err_ptr = getErrorPtr(result);
  ASSERT_NE(err_ptr, nullptr);
  EXPECT_EQ(err_ptr->code, ErrorCode::InvalidUtf8);
  EXPECT_EQ(err_ptr->message, "bad bytes");
}

TEST(ErrorTest, ResultMutableValuePtr) {
  Result<std::string> result = std::string("mutable");
  auto* val_ptr = getValuePtr(result);
  ASSERT_NE(val_ptr, nullptr);
  *val_ptr = "changed";
  EXPECT_EQ(*getValuePtr(result), "changed");
}

TEST(ErrorTest, ResultWithIntType) {
  Result<int> success = 42;
  EXPECT_TRUE(isSuccess(success));
  ASSERT_NE(getValuePtr(success), nullptr);
  EXPECT_EQ(*getValuePtr(success), 42);

  Result<int> failure = Error(ErrorCode::OutOfMemory);
  EXPECT_FALSE(isSuccess(failure));
  ASSERT_NE(getErrorPtr(failure), nullptr);
  EXPECT_EQ(getErrorPtr(failure)->code, ErrorCode::OutOfMemory);
}

}  // namespace
}  // namespace core
}  // namespace suzume
