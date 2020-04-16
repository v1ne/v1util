#include "ringbuffer.hpp"

#include "array_view.hpp"

#include "doctest/doctest.h"

namespace v1util { namespace container { namespace test {

const auto sSomeInts = {5, 23, 42};

TEST_CASE("ringIterator-otherOps") {
  auto view = make_array_view(sSomeInts);
  auto iRing = RingIterator(view.begin(), view.end());

  // equality
  CHECK(iRing == iRing);
  CHECK(iRing + 1 == iRing + 1);

  CHECK(iRing + 1 != iRing);
  CHECK(iRing != iRing + 1);
  CHECK(iRing - 1 != iRing + 1);

  // ring inequality
  CHECK(iRing + view.size() != iRing);
  CHECK(iRing - view.size() != iRing);
}

TEST_CASE("ringIterator-addsub") {
  auto view = make_array_view(sSomeInts);
  auto iRing = RingIterator(view.begin(), view.end());
  auto iStop = iRing + view.size();

  // operator+ behaves properly
  CHECK(*iRing == 5);
  CHECK(*(iRing + 1) == 23);
  CHECK(*(iRing + 2) == 42);

  // operator- stays within ring
  CHECK(*iRing == 5);
  CHECK(*(iRing - 1) == 42);
  CHECK(*(iRing - 2) == 23);

  // add
  CHECK(iStop - iRing == view.size());
  CHECK(iStop - (iRing + 0) == 3);
  CHECK(iStop - (iRing + 1) == 2);
  CHECK(iStop - (iRing + 2) == 1);
  CHECK(iStop - (iRing + 3) == 0);
  CHECK(iStop - (iRing + 4) == -1);
  CHECK(iStop - (iRing + 5) == -2);
  CHECK(iStop - (iRing + 6) == -3);

  // sub
  CHECK(iStop - (iRing - 0) == 3 + 0);
  CHECK(iStop - (iRing - 1) == 3 + 1);
  CHECK(iStop - (iRing - 2) == 3 + 2);
  CHECK(iStop - (iRing - 3) == 3 + 3);
  CHECK(iStop - (iRing - 4) == 3 + 4);
  CHECK(iStop - (iRing - 5) == 3 + 5);
  CHECK(iStop - (iRing - 6) == 3 + 6);

  // +=/-= wrap around properly
  CHECK(*(iRing += 2) == 42);
  CHECK(*(iRing += 2) == 23);
  CHECK(*(iRing += 1) == 42);
  CHECK(*(iRing += 1) == 5);
  CHECK(*(iRing -= 2) == 23);
  CHECK(*(iRing -= 2) == 42);
  CHECK(*(iRing -= 1) == 23);
  CHECK(*(iRing -= 1) == 5);
}

TEST_CASE("ringIterator-incdec") {
  auto view = make_array_view(sSomeInts);
  auto iRing = RingIterator(view.begin(), view.end());

  // inc
  auto iRingInc = iRing;
  CHECK(*iRingInc == view[0]);
  CHECK(*++iRingInc == view[1]);
  CHECK(*++iRingInc == view[2]);
  CHECK(*++iRingInc == view[0]);
  CHECK(*++iRingInc == view[1]);
  CHECK(*++iRingInc == view[2]);
  CHECK(*++iRingInc == view[0]);
  CHECK(*++iRingInc == view[1]);
  CHECK(*++iRingInc == view[2]);
  CHECK(iRingInc.base() == view.begin() + 2);

  // dec
  auto iRingDec = iRing;
  CHECK(*iRingDec == view[0]);
  CHECK(*--iRingDec == view[2]);
  CHECK(*--iRingDec == view[1]);
  CHECK(*--iRingDec == view[0]);
  CHECK(*--iRingDec == view[2]);
  CHECK(*--iRingDec == view[1]);
  CHECK(*--iRingDec == view[0]);
  CHECK(*--iRingDec == view[2]);
  CHECK(iRingDec.base() == view.begin() + 2);

  // post-inc/dec
  auto iRingPostOp = iRing;
  CHECK(*iRingPostOp++ == view[0]);
  CHECK(*iRingPostOp++ == view[1]);
  CHECK(*iRingPostOp++ == view[2]);
  CHECK(*iRingPostOp++ == view[0]);
  CHECK(*iRingPostOp == view[1]);
  CHECK(*iRingPostOp-- == view[1]);
  CHECK(*iRingPostOp-- == view[0]);
  CHECK(*iRingPostOp-- == view[2]);
  CHECK(*iRingPostOp-- == view[1]);
  CHECK(*iRingPostOp == view[0]);
}

TEST_CASE("FixedSizeDeque") {
  FixedSizeDeque<int> deque;
  deque.setCapacity(2);
  CHECK(deque.empty());

  {
    deque.push_front(23);
    CHECK(deque.front() == 23);
    CHECK(deque.back() == 23);
    CHECK(!deque.empty());

    deque.push_front(42);
    CHECK(deque.front() == 42);
    CHECK(deque.back() == 23);
    CHECK(!deque.empty());

    deque.pop_front();
    CHECK(deque.front() == 23);
    CHECK(deque.back() == 23);
    CHECK(!deque.empty());

    deque.pop_front();
    CHECK(deque.empty());
  }

  {
    deque.push_back(1);
    CHECK(deque.front() == 1);
    CHECK(deque.back() == 1);

    deque.push_back(2);
    CHECK(deque.front() == 1);
    CHECK(deque.back() == 2);

    deque.pop_back();
    CHECK(deque.front() == 1);
    CHECK(deque.back() == 1);

    deque.pop_back();
    CHECK(deque.empty());
  }

  {
    deque.push_back(10);
    CHECK(deque.front() == 10);
    CHECK(deque.back() == 10);

    deque.push_front(20);
    CHECK(deque.front() == 20);
    CHECK(deque.back() == 10);

    deque.pop_back();
    CHECK(deque.front() == 20);
    CHECK(deque.back() == 20);

    deque.pop_back();
    CHECK(deque.empty());
  }

  {
    deque.push_back(5);
    deque.push_back(6);
    CHECK(deque.front() == 5);
    CHECK(deque.back() == 6);

    deque.pop_front();
    deque.push_back(7);
    deque.pop_front();
    deque.push_back(8);
    CHECK(deque.front() == 7);
    CHECK(deque.back() == 8);

    deque.pop_front();
    deque.pop_back();
    CHECK(deque.empty());
  }

}

}}}  // namespace v1util::container::test
