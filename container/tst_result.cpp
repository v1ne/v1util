#include "result.hpp"

#include "doctest/doctest.h"

#include <iostream>
#include <string>


namespace v1util { namespace test {

TEST_CASE("result-construction") {
  SUBCASE("construction from error/value") {
  Result<int> emptyResult;
  CHECK(!emptyResult.hasValue());

  auto err = std::error_code{23, std::system_category()};
  Result<std::string> initialisedFromError{err};
  CHECK(!initialisedFromError.hasValue());
  CHECK(initialisedFromError.error() == err);

  const std::string constValue = "hello";
  Result<std::string> copyConstructed(constValue);
  CHECK(copyConstructed.value() == "hello");

  std::string value = "hello";
  Result<std::string> moveConstructed(std::move(value));
  CHECK(value.empty());
  CHECK(copyConstructed.value() == "hello");
  }

  SUBCASE("copy/move construction") {  
    auto int23 = Result<int>(23);
    auto int23Copy = int23;
    CHECK(int23Copy.value() == 23);

    auto int23MovedInto = std::move(int23);
    CHECK(!int23);
    CHECK(int23MovedInto.value() == 23);
  }
}

TEST_CASE("result-assignment") {
    const auto int23 = Result<int>(23);
    const auto int42 = Result<int>(42);

    auto copyTarget = int23;
    copyTarget = int42;
    CHECK(int42.value() == 42);
    CHECK(copyTarget.value() == 42);

    auto moveTarget = int23;
    auto moveSource = int42;
    moveTarget = std::move(moveSource);
    CHECK(!moveSource);
    CHECK(moveTarget.value() == int42.value());

  // TODO: Check value/error assignment
}

TEST_CASE("result-mutableValueAndError") {
    auto int23 = Result<int>(23);
    int23.value() = 42;
    CHECK(int23.value() == 42);

    auto erroredResult = Result<int>(std::error_code{23, std::system_category()});
    erroredResult.error().assign(42, std::system_category());
    CHECK(erroredResult.error().value() == 42);
}

TEST_CASE("result-apply") {
  {
    auto int23 = Result<int>(23);
    auto int46 = int23.apply([](auto&& x) { return x * 2; });
    CHECK(int46.value() == 46);

    auto int24 = int23.apply([](const int& x) { return x + 1; });
    CHECK(int24.value() == 24);

    auto int25 = int23.apply([](int& x) { return x + 2; });
    CHECK(int25.value() == 25);

    auto int26 = int23.apply([](int x) { return x + 3; });
    CHECK(int26.value() == 26);

    int23.apply([](int& x) { x += 2; });
    CHECK(int23.value() == 25);
  }

  {
    const auto int23 = Result<int>(23);
    const auto int30 = int23.apply([](auto&& x) { return x + 7; });
    CHECK(int30.value() == 30);

    const auto int31 = int23.apply([](const int& x) { return x + 8; });
    CHECK(int31.value() == 31);

    const auto int32 = int23.apply([](int x) { return x + 9; });
    CHECK(int32.value() == 32);

    int value = 0;
    int23.apply([&](const int& x) { value = x; });
    CHECK(value == 23);

    int23.apply([&](int x) { value = 42; });
    CHECK(value == 42);
  }

  {
    bool called = false;
    auto err = std::error_code{23, std::system_category()};

    auto errToo = Result<int>(err).apply([&](int& x) {
      called = true;
      return x * 2;
    });

    CHECK(errToo.error() == err);
    CHECK(!called);
  }
}

}}  // namespace v1util::test
