#include "span.hpp"

#include "doctest/doctest.h"

#include <array>
#include <cstring>
#include <vector>

namespace v1util { namespace test {

TEST_CASE("span-construction") {
  auto arr = std::array<int, 4>{1, 2, 3, 4};
  auto vec = std::vector<int>{1, 2, 3, 4};

  const auto arrSpan = make_span(arr);
  CHECK(arrSpan.begin() == &*arr.begin());
  CHECK(arrSpan.size() == arr.size());

  const auto vecSpan = make_span(vec);
  CHECK(vecSpan.begin() == &*vec.begin());
  CHECK(vecSpan.size() == vec.size());

  const auto ptrSizeSpan = make_span(vec.data(), vec.size());
  CHECK(ptrSizeSpan.begin() == &*vec.begin());
  CHECK(ptrSizeSpan.size() == vec.size());

  const auto ptrPtrSpan = make_span(&*vec.begin(), (&*vec.begin()) + vec.size());
  CHECK(ptrPtrSpan.begin() == &*vec.begin());
  CHECK(ptrPtrSpan.size() == vec.size());

  const auto invalidSpan = Span<int>((int*)0x0, 23);
  CHECK(!invalidSpan.empty());
  CHECK(invalidSpan.size() == 23);
  CHECK(invalidSpan.begin() == nullptr);
  CHECK(invalidSpan.end() == (int*)(23 * sizeof(int)));

  const auto emptySpan = Span<int>(vec.begin(), vec.begin());
  CHECK(emptySpan.empty());
  const auto emptySpan2 = Span<int>((int*)0x2342, 0);
  CHECK(emptySpan2.empty());
  const auto emptySpan3 = Span<int>((int*)0x2342, (int*)0x2342);
  CHECK(emptySpan3.empty());
}

TEST_CASE("span-copyAndMove") {
  auto values = std::array<int, 4>{1, 2, 3, 4};
  auto values2 = std::array<int, 2>{9, 8};
  const auto view = make_span(values);
  const auto view2 = make_span(values2);

  auto copyConstruct = view;
  CHECK(copyConstruct.begin() == view.begin());
  CHECK(copyConstruct.size() == view.size());

  const auto moveConstruct = Span<int>(std::move(copyConstruct));
  CHECK(moveConstruct.begin() == view.begin());
  CHECK(moveConstruct.size() == view.size());

  auto copyAssign = view;
  copyAssign = view2;
  CHECK(copyAssign.begin() == view2.begin());
  CHECK(copyAssign.size() == view2.size());

  copyAssign = nullptr;
  CHECK(copyAssign.begin() == nullptr);
  CHECK(copyAssign.size() == 0);

  copyAssign = view;
  auto moveAssign = view2;
  moveAssign = std::move(copyAssign);
  CHECK(moveAssign.begin() == view.begin());
  CHECK(moveAssign.size() == view.size());

  auto x = view;
  auto y = view2;
  x.swap(y);
  CHECK(x == view2);
  CHECK(y == view);
}

TEST_CASE("span-iterators") {
  auto values = std::array<int, 4>{1, 2, 3, 4};
  const auto view = make_span(values);

  CHECK(view.begin() == view.cbegin());
  CHECK(*view.begin() == *values.begin());
  CHECK(view.end() == view.cend());
  CHECK(view.end() - view.begin() == view.size());

  CHECK(view.rbegin() == view.crbegin());
  CHECK(*view.rbegin() == *std::prev(values.end()));
  CHECK(view.rend() == view.crend());
  CHECK(*std::prev(view.rend()) == *values.begin());
}

TEST_CASE("span-access") {
  const auto emptySpan = Span<int>();
  CHECK(emptySpan.empty());
  CHECK(emptySpan.size() == 0);
  CHECK(emptySpan.data() == nullptr);

  auto values = std::array<int, 4>{1, 2, 3, 4};
  const auto view = make_span(values);
  CHECK(!view.empty());
  CHECK(view.size() == values.size());
  CHECK(view.data() == &*values.begin());

  CHECK(!(emptySpan == view));
  CHECK(emptySpan != view);

  CHECK(view == view);
  CHECK(emptySpan == emptySpan);

  CHECK(view == make_span(view.begin(), view.end()));
  CHECK(view != make_span(view.begin(), view.end() - 1));
  CHECK(view != make_span(view.begin() + 1, view.end()));

  CHECK(view < make_span(view.begin() + 1, view.end()));
  CHECK(make_span(view.begin(), view.end() - 1) < view);
  CHECK(!(view < view));
  CHECK(!(view < make_span(view.begin(), view.end() - 1)));
  CHECK(!(make_span(view.begin() + 1, view.end()) < view));

  CHECK(view.front() == *values.begin());
  CHECK(view.back() == *std::prev(values.end()));

  CHECK(view[0] == *values.begin());
  CHECK(view[view.size() - 1] == *std::prev(values.end()));
}

TEST_CASE("span-subset") {
  auto checkEq = [](const Span<int>& a, std::initializer_list<int> b) {
    REQUIRE(a.size() == b.size());
    if(b.size()) CHECK(!memcmp(a.data(), &*b.begin(), sizeof(int) * a.size()));
  };

  auto values = std::array<int, 4>{1, 2, 3, 4};
  const auto view = make_span(values);

  checkEq(view, {1, 2, 3, 4});
  checkEq(view.subspan(0, 4), {1, 2, 3, 4});
  checkEq(view.subspan(1, 3), {2, 3, 4});
  checkEq(view.subspan(1, 2), {2, 3});
  checkEq(view.subspan(2, 2), {3, 4});
  checkEq(view.subspan(2, 0), {});
  checkEq(view.subspan(4, 0), {});

  checkEq(view.first(0), {});
  checkEq(view.first(1), {1});
  checkEq(view.first(2), {1, 2});
  checkEq(view.first(4), {1, 2, 3, 4});

  checkEq(view.skip(0), {1, 2, 3, 4});
  checkEq(view.skip(1), {2, 3, 4});
  checkEq(view.skip(2), {3, 4});
  checkEq(view.skip(4), {});

  checkEq(view.last(0), {});
  checkEq(view.last(1), {4});
  checkEq(view.last(2), {3, 4});
  checkEq(view.last(4), {1, 2, 3, 4});

  checkEq(view.shrink(0), {1, 2, 3, 4});
  checkEq(view.shrink(1), {1, 2, 3});
  checkEq(view.shrink(2), {1, 2});
  checkEq(view.shrink(4), {});
}

}}  // namespace v1util::test
