#include "linear_regression.hpp"

#include "v1util/container/array_view.hpp"

#include "doctest/doctest.h"

#include <cmath>

namespace v1util { namespace stats { namespace test {

TEST_CASE("linearRegression-simple") {
  const auto dataXf = {-1.f, 1.f, 5.f};
  const auto dataYf = {-3.f, 7.f, 27.f};
  const auto coefficientsF = linearRegression(make_array_view(dataXf), make_array_view(dataYf));
  CHECK(std::abs(coefficientsF.a - 5) < 1e-6);
  CHECK(std::abs(coefficientsF.b - 2) < 1e-6);

  const auto dataXi = {-1, 1, 6, 10};
  const auto dataYi = {4, 2, -3, -7};
  const auto coefficientsI = linearRegression(make_array_view(dataXi), make_array_view(dataYi));
  CHECK(coefficientsI == StraitCoefficients<int>{-1, 3});
}


TEST_CASE("linearSeriesEstimator-simple") {
  LinearSeriesEstimator<float> lse;
  lse.setAlpha(alphaForExpAvgFromStepsToAmount(3.f, 0.9f));

  lse.feed(1, 5 + 2);
  lse.feed(2, 10 + 2);
  lse.feed(3, 15 + 2);
  lse.feed(4, 20 + 2);

  auto coeff = lse.currentCoefficients();
  CHECK(coeff.a == doctest::Approx(5).epsilon(1 - 0.9f));
  CHECK(coeff.b == doctest::Approx(2).epsilon(1 - 0.9f));

  // And now, totally change the parameters:
  lse.feed(200, -20 + 7);
  lse.feed(190, -19 + 7);
  lse.feed(180, -18 + 7);
  lse.feed(170, -17 + 7);
  lse.feed(160, -16 + 7);

  auto coeff2 = lse.currentCoefficients();
  CHECK(coeff2.a == doctest::Approx(-.1f).epsilon(1 - 0.9f));
  CHECK(coeff2.b == doctest::Approx(7.f).epsilon(1 - 0.9f));
}

TEST_CASE("linearSeriesEstimator-many") {
  LinearSeriesEstimator<float> lse;
  const auto targetAmount = 0.9f;
  lse.setAlpha(alphaForExpAvgFromStepsToAmount(3.f, targetAmount));

  lse.feed(1, 5 + 2);
  lse.feed(2, 10 + 2);
  lse.feed(3, 15 + 2);
  lse.feed(4, 20 + 2);

  auto coeff = lse.currentCoefficients();
  CHECK(coeff.a == doctest::Approx(5).epsilon(1 - targetAmount));
  CHECK(coeff.b == doctest::Approx(2).epsilon(1 - targetAmount));

  // And now, totally change the parameters:
  lse.feed(200, -20 + 7);  // needs a step more to recover from the step in value ranges
  lse.feed(190, -19 + 7);
  lse.feed(180, -18 + 7);
  lse.feed(170, -17 + 7);
  lse.feed(160, -16 + 7);

  auto coeff2 = lse.currentCoefficients();
  CHECK(coeff2.a == doctest::Approx(-.1f).epsilon(1 - targetAmount));
  CHECK(coeff2.b == doctest::Approx(7.f).epsilon(1 - targetAmount));
}


}}}  // namespace v1util::stats::test
