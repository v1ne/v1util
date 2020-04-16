#include "math.hpp"

#include "warnings.hpp"

#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace v1util::test {

TEST_CASE("expAvgAlphas") {
  SUBCASE("1 step -> 1-1/e, approx. 63%") {
    CHECK(alphaForExpAvgFromSteps(1.f) == doctest::Approx(1 - 1 / std::exp(1.f)));
  }

  SUBCASE("1 step -> 63% (steps + amount)") {
    CHECK(alphaForExpAvgFromStepsToAmount(1., 0.63) == doctest::Approx(.63));
  }

  SUBCASE("1 step -> 95% (steps + amount)") {
    CHECK(alphaForExpAvgFromStepsToAmount(1., 0.95) == doctest::Approx(.95));
  }
}

TEST_CASE("expAvg-stepping") {
  auto checkFor = [](float initialValue, float targetValue, float targetAmount, int numSteps) {
    float delta = std::abs(targetValue - initialValue);
    float alpha = alphaForExpAvgFromStepsToAmount(float(numSteps), targetAmount);
    float value = initialValue;
    if(numSteps > 1) {
      // Smoothing may not reach target value before numSteps:
      for(int i = 0; i < numSteps - 1; ++i) applyExpAvg(&value, alpha, targetValue);
      CHECK(std::abs(value - targetValue) > doctest::Approx((1.f - targetAmount) * delta));

      applyExpAvg(&value, alpha, targetValue);
    } else
      for(int i = 0; i < numSteps; ++i) applyExpAvg(&value, alpha, targetValue);

    CHECK(std::abs(value - targetValue) <= doctest::Approx((1.f - targetAmount) * delta));
  };

  SUBCASE("5 steps for 0 -> 99% of 1") { checkFor(0.f, 1.f, 0.99f, 5); }
  SUBCASE("5 steps for 0 -> 99% of 1,000,000") { checkFor(0.f, 1e6f, 0.99f, 5); }
  SUBCASE("5 steps for 1,000,000 -> 0 (99%)") { checkFor(1e6f, 0.f, 0.99f, 5); }

  SUBCASE("1000 steps for 0 -> 90% of 1") { checkFor(0.f, 1.f, 0.90f, 1000); }
}


TEST_CASE("intDiv") {
  SUBCASE("round") {
    CHECK(roundIntDiv(0U, 1U) == 0U);
    CHECK(roundIntDiv(0U, 2U) == 0U);
    CHECK(roundIntDiv(3U, 3U) == 1U);
    CHECK(roundIntDiv(4U, 3U) == 1U);
    CHECK(roundIntDiv(5U, 3U) == 2U);

    CHECK(roundIntDiv(0, 1) == 0);
    CHECK(roundIntDiv(0, 2) == 0);
    CHECK(roundIntDiv(0, -1) == 0);
    CHECK(roundIntDiv(0, -2) == 0);

    CHECK(roundIntDiv(3, 3) == 1);
    CHECK(roundIntDiv(4, 3) == 1);
    CHECK(roundIntDiv(5, 3) == 2);

    CHECK(roundIntDiv(-3, 3) == -1);
    CHECK(roundIntDiv(-4, 3) == -1);
    CHECK(roundIntDiv(-5, 3) == -2);

    CHECK(roundIntDiv(3, -3) == -1);
    CHECK(roundIntDiv(4, -3) == -1);
    CHECK(roundIntDiv(5, -3) == -2);

    CHECK(roundIntDiv(-3, -3) == 1);
    CHECK(roundIntDiv(-4, -3) == 1);
    CHECK(roundIntDiv(-5, -3) == 2);
  }

  SUBCASE("ceil") {
    CHECK(ceilIntDiv(0U, 1U) == 0U);
    CHECK(ceilIntDiv(0U, 2U) == 0U);
    CHECK(ceilIntDiv(3U, 3U) == 1U);
    CHECK(ceilIntDiv(4U, 3U) == 2U);
    CHECK(ceilIntDiv(5U, 3U) == 2U);

    CHECK(ceilIntDiv(0, 1) == 0);
    CHECK(ceilIntDiv(0, 2) == 0);
    CHECK(ceilIntDiv(0, -1) == 0);
    CHECK(ceilIntDiv(0, -2) == 0);

    CHECK(ceilIntDiv(3, 3) == 1);
    CHECK(ceilIntDiv(4, 3) == 2);
    CHECK(ceilIntDiv(5, 3) == 2);

    CHECK(ceilIntDiv(-4, 3) == -1);
    CHECK(ceilIntDiv(-5, 3) == -1);
    CHECK(ceilIntDiv(-6, 3) == -2);

    CHECK(ceilIntDiv(4, -3) == -1);
    CHECK(ceilIntDiv(5, -3) == -1);
    CHECK(ceilIntDiv(6, -3) == -2);

    CHECK(ceilIntDiv(-3, -3) == 1);
    CHECK(ceilIntDiv(-4, -3) == 2);
    CHECK(ceilIntDiv(-5, -3) == 2);
  }
}


TEST_CASE("ringDistance") {
  V1_NO_WARN_TRUNCATE_CONSTS
  CHECK(ringDistance(uint16_t(32768U + 16384U), uint16_t(32768)) == 16384);
  CHECK(ringDistance(uint16_t(65000U + 2345U), uint16_t(65000)) == 2345);
  CHECK(ringDistance(uint16_t(1234U + 32767U), uint16_t(1234)) == 32767);
  CHECK(ringDistance(uint16_t(1234U + 32768U), uint16_t(1234)) == -32768);
  CHECK(ringDistance(uint16_t(1234U + 32769U), uint16_t(1234)) == -32767);
  CHECK(ringDistance(uint16_t(1234U + 32770U), uint16_t(1234)) == -32766);

  CHECK(ringDistance(uint16_t(50000U - 2342U), uint16_t(50000)) == -2342);
  CHECK(ringDistance(uint16_t(1234U - 2345U), uint16_t(1234)) == -2345);
  CHECK(ringDistance(uint16_t(1234U - 2345U), uint16_t(1234)) == -2345);
  CHECK(ringDistance(uint16_t(1234U - 32767U), uint16_t(1234)) == -32767);
  CHECK(ringDistance(uint16_t(1234U - 32768U), uint16_t(1234)) == -32768);
  CHECK(ringDistance(uint16_t(1234U - 32769U), uint16_t(1234)) == 32767);
  CHECK(ringDistance(uint16_t(1234U - 32770U), uint16_t(1234)) == 32766);
  V1_RESTORE_WARNINGS
}

TEST_CASE("ringRelations") {
  SUBCASE("isAtOrAfterInRing") {
    CHECK(!isAtOrAfterInRing(uint8_t(128), uint8_t(0)));
    CHECK(!isAtOrAfterInRing(uint8_t(128), uint8_t(67)));
    CHECK(!isAtOrAfterInRing(uint8_t(128), uint8_t(127)));
    CHECK(isAtOrAfterInRing(uint8_t(128), uint8_t(128)));
    CHECK(isAtOrAfterInRing(uint8_t(128), uint8_t(199)));
    CHECK(isAtOrAfterInRing(uint8_t(128), uint8_t(255)));

    constexpr const auto shift = 23;
    CHECK(isAtOrAfterInRing(uint8_t(shift), uint8_t(shift)));
    CHECK(isAtOrAfterInRing(uint8_t(shift), uint8_t(shift + 1)));
    CHECK(isAtOrAfterInRing(uint8_t(shift), uint8_t(shift + 127)));
    CHECK(!isAtOrAfterInRing(uint8_t(shift), uint8_t(shift + 128)));
    CHECK(!isAtOrAfterInRing(uint8_t(shift), uint8_t(shift + 129)));
    V1_NO_WARN_TRUNCATE_CONSTS
    CHECK(!isAtOrAfterInRing(uint8_t(shift), uint8_t(shift + 255)));
    V1_RESTORE_WARNINGS

    // interesting: For the element on the opposing side of the ring, the relation is never true:
    CHECK(!isAtOrAfterInRing(uint8_t(shift), uint8_t(shift + 128)));
    CHECK(!isAtOrAfterInRing(uint8_t(shift + 128), uint8_t(shift)));
  }

  for(unsigned int shift : {0, 23}) {
    SUBCASE("<=") {
      CHECK(ringLessEq(uint8_t(shift), uint8_t(shift)));
      CHECK(ringLessEq(uint8_t(shift), uint8_t(shift + 1)));
      CHECK(ringLessEq(uint8_t(shift), uint8_t(shift + 127)));
      CHECK(!ringLessEq(uint8_t(shift), uint8_t(shift + 128)));
      CHECK(!ringLessEq(uint8_t(shift), uint8_t(shift + 129)));
      V1_NO_WARN_TRUNCATE_CONSTS
      CHECK(!ringLessEq(uint8_t(shift), uint8_t(shift + 255)));
      V1_RESTORE_WARNINGS

      CHECK(!ringLessEq(uint8_t(shift), uint8_t(shift + 128)));
      CHECK(!ringLessEq(uint8_t(shift + 128), uint8_t(shift)));
    }

    SUBCASE("<") {
      CHECK(!ringLess(uint8_t(shift), uint8_t(shift)));
      CHECK(ringLess(uint8_t(shift), uint8_t(shift + 1)));
      CHECK(ringLess(uint8_t(shift), uint8_t(shift + 127)));
      CHECK(!ringLess(uint8_t(shift), uint8_t(shift + 128)));
      CHECK(!ringLess(uint8_t(shift), uint8_t(shift + 129)));
      V1_NO_WARN_TRUNCATE_CONSTS
      CHECK(!ringLess(uint8_t(shift), uint8_t(shift + 255)));
      V1_RESTORE_WARNINGS

      CHECK(!ringLess(uint8_t(shift), uint8_t(shift + 128)));
      CHECK(!ringLess(uint8_t(shift + 128), uint8_t(shift)));
    }

    SUBCASE(">=") {
      CHECK(ringGreaterEq(uint8_t(shift), uint8_t(shift)));
      CHECK(ringGreaterEq(uint8_t(shift), uint8_t(shift - 1)));
      CHECK(ringGreaterEq(uint8_t(shift), uint8_t(shift - 127)));
      CHECK(!ringGreaterEq(uint8_t(shift), uint8_t(shift - 128)));
      CHECK(!ringGreaterEq(uint8_t(shift), uint8_t(shift - 129)));
      V1_NO_WARN_TRUNCATE_CONSTS
      CHECK(!ringGreaterEq(uint8_t(shift), uint8_t(shift - 255)));
      V1_RESTORE_WARNINGS

      CHECK(!ringGreaterEq(uint8_t(shift), uint8_t(shift - 128)));
      CHECK(!ringGreaterEq(uint8_t(shift - 128), uint8_t(shift)));
    }

    SUBCASE(">") {
      CHECK(!ringGreater(uint8_t(shift), uint8_t(shift)));
      CHECK(ringGreater(uint8_t(shift), uint8_t(shift - 1)));
      CHECK(ringGreater(uint8_t(shift), uint8_t(shift - 127)));
      CHECK(!ringGreater(uint8_t(shift), uint8_t(shift - 128)));
      CHECK(!ringGreater(uint8_t(shift), uint8_t(shift - 129)));
      V1_NO_WARN_TRUNCATE_CONSTS
      CHECK(!ringGreater(uint8_t(shift), uint8_t(shift - 255)));
      V1_RESTORE_WARNINGS

      CHECK(!ringGreater(uint8_t(shift), uint8_t(shift - 128)));
      CHECK(!ringGreater(uint8_t(shift - 128), uint8_t(shift)));
    }
  }
}

}  // namespace v1util::test
