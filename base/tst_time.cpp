#include "time.hpp"

#include "thread.hpp"

#include "doctest/doctest.h"

namespace v1util { namespace test {

TEST_CASE("tscScaling" * doctest::skip()) {
  auto startTime = tscNow();
  sleepMs(100);
  auto endTime = tscNow();
  auto delta = toDblS(endTime - startTime);
  CHECK(delta < 0.13);
  CHECK(delta > 0.07);
}

TEST_CASE("TscDiff_operators") {
  const auto iota = TscDiff(1);

  const auto dt = TscDiff{23};
  const auto dt1 = dt + iota;

  CHECK((dt - iota).raw() == dt.raw() - iota.raw());
  CHECK((dt1 + dt1).raw() == 2 * dt1.raw());
  CHECK(dt1 + dt1 == 2 * dt1);
  CHECK(dt1 + dt1 == dt1 * 2);
  CHECK((2 * dt) / 2 == dt);

  CHECK(dt == dt);
  CHECK(!(dt == dt1));
  CHECK(!(dt != dt));
  CHECK(dt1 != dt);

  CHECK(!(dt < dt));
  CHECK(dt < dt1);
  CHECK(!(dt1 < dt));

  CHECK(dt <= dt);
  CHECK(dt <= dt1);
  CHECK(!(dt1 <= dt));

  CHECK(!(dt > dt));
  CHECK(!(dt > dt1));
  CHECK(dt1 > dt);

  CHECK(dt >= dt);
  CHECK(!(dt >= dt1));
  CHECK(dt1 >= dt);

  CHECK(dt - dt == 0);
  CHECK(dt != 0);
  CHECK(dt > 0);
  CHECK(dt - dt >= 0);
  CHECK(dt - dt <= 0);
  CHECK(-dt < 0);
  CHECK(!(dt < 0));
  CHECK(!(-dt > 0));
}

TEST_CASE("TscStamp_operators") {
  const auto t = TscStamp(1);
  const auto iota = TscDiff(1);
  const auto halfValueRange = TscDiff((-9223372036854775807LL) - 1 /* make GCC happy */);
  const auto halfValueRangeMinusOne = TscDiff(9223372036854775807LL);
  const auto halfValueRangePlusOne = TscDiff(-9223372036854775807LL);
  CHECK(halfValueRangeMinusOne + iota == halfValueRange);
  CHECK(halfValueRangePlusOne - iota == halfValueRange);
  CHECK((t + halfValueRange + halfValueRange).raw() == t.raw());

  const auto t1 = t + iota;
  CHECK(t == t);
  CHECK(!(t == t1));
  CHECK(!(t != t));
  CHECK(t1 != t);

  CHECK(!(t < t));
  CHECK(t < t1);
  CHECK(!(t1 < t));

  CHECK(t <= t);
  CHECK(t <= t1);
  CHECK(!(t1 <= t));

  CHECK(!(t > t));
  CHECK(!(t > t1));
  CHECK(t1 > t);

  CHECK(t >= t);
  CHECK(!(t >= t1));
  CHECK(t1 >= t);

  const auto almostHalfPastT = t + halfValueRangeMinusOne;
  const auto halfPastT = t + halfValueRange;
  const auto wellHalfPastT = t + halfValueRangePlusOne;

  CHECK(!(almostHalfPastT == t));
  CHECK(!(halfPastT == t));
  CHECK(almostHalfPastT != t);
  CHECK(halfPastT != t);

  // operator<= must match isAtOrAfterInRing:
  CHECK(t <= almostHalfPastT);
  CHECK(!(almostHalfPastT <= t));
  CHECK(!(t <= halfPastT));
  CHECK(!(halfPastT <= t));
  CHECK(!(t <= wellHalfPastT));
  CHECK(wellHalfPastT <= t);

  // operator>= must match !isAtOrAfterInRing:
  CHECK(!(t >= almostHalfPastT));
  CHECK(almostHalfPastT >= t);
  CHECK(!(t >= halfPastT));
  CHECK(!(halfPastT >= t));
  CHECK(t >= wellHalfPastT);
  CHECK(!(wellHalfPastT >= t));

  CHECK(t < almostHalfPastT);
  CHECK(!(almostHalfPastT < t));
  CHECK(!(t < halfPastT));
  CHECK(!(halfPastT < t));
  CHECK(!(t < wellHalfPastT));
  CHECK(wellHalfPastT < t);

  CHECK(!(t > almostHalfPastT));
  CHECK(almostHalfPastT > t);
  CHECK(!(t > halfPastT));
  CHECK(!(halfPastT > t));
  CHECK(t > wellHalfPastT);
  CHECK(!(wellHalfPastT > t));

  CHECK((t1 - t).raw() == t1.raw() - t.raw());
  auto t2 = t;
  t2 += iota * 2;
  CHECK(t2 == t + iota + iota);
  t2 -= iota;
  CHECK(t2 == t + iota);
}

}}  // namespace v1util::test
