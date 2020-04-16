
#include "array_view.hpp"

#include "doctest/doctest.h"

#include <array>
#include <cstring>
#include <vector>

namespace v1util { namespace test {

TEST_CASE("array_view-construction") {
  const auto arr = std::array<int, 4>{1, 2, 3, 4};
  const auto vec = std::vector<int>{1, 2, 3, 4};
  const auto list = {1, 2, 3, 4};

  const auto arrView = make_array_view(arr);
  CHECK(arrView.begin() == &*arr.begin());
  CHECK(arrView.size() == arr.size());

  const auto vecView = make_array_view(vec);
  CHECK(vecView.begin() == &*vec.begin());
  CHECK(vecView.size() == vec.size());

  const auto constvecView = make_array_view((const std::vector<int>&)vec);
  CHECK(constvecView.begin() == &*vec.begin());
  CHECK(constvecView.size() == vec.size());

  const auto listView = make_array_view(list);
  CHECK(listView.begin() == &*list.begin());
  CHECK(listView.size() == list.size());

  const auto ptrSizeView = make_array_view(vec.data(), vec.size());
  CHECK(ptrSizeView.begin() == &*vec.begin());
  CHECK(ptrSizeView.size() == vec.size());

  const auto ptrPtrView = make_array_view(&*vec.begin(), (&*vec.begin()) + vec.size());
  CHECK(ptrPtrView.begin() == &*vec.begin());
  CHECK(ptrPtrView.size() == vec.size());

  const auto constptrPtrView = make_array_view(&*vec.cbegin(), (&*vec.cbegin()) + vec.size());
  CHECK(constptrPtrView.begin() == &*vec.cbegin());
  CHECK(constptrPtrView.size() == vec.size());

  const auto invalidView = ArrayView<int>((const int*)0x0, 23);
  CHECK(!invalidView.empty());
  CHECK(invalidView.size() == 23);
  CHECK(invalidView.begin() == nullptr);
  CHECK(invalidView.end() == (const int*)(23 * sizeof(int)));

  const auto emptyView = ArrayView<int>(vec.begin(), vec.begin());
  CHECK(emptyView.empty());
  const auto emptyView2 = ArrayView<int>((const int*)0x2342, 0);
  CHECK(emptyView2.empty());
  const auto emptyView3 = ArrayView<int>((const int*)0x2342, (const int*)0x2342);
  CHECK(emptyView3.empty());
}

TEST_CASE("array_view-copyAndMove") {
  const auto values = {1, 2, 3, 4};
  const auto values2 = {9, 8};
  const auto view = make_array_view(values);
  const auto view2 = make_array_view(values2);

  auto copyConstruct = view;
  CHECK(copyConstruct.begin() == view.begin());
  CHECK(copyConstruct.size() == view.size());

  const auto moveConstruct = ArrayView<int>(std::move(copyConstruct));
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

TEST_CASE("array_view-iterators") {
  const auto values = {1, 2, 3, 4};
  const auto view = make_array_view(values);

  CHECK(view.begin() == view.cbegin());
  CHECK(*view.begin() == *values.begin());
  CHECK(view.end() == view.cend());
  CHECK(view.end() - view.begin() == view.size());

  CHECK(view.rbegin() == view.crbegin());
  CHECK(*view.rbegin() == *std::prev(values.end()));
  CHECK(view.rend() == view.crend());
  CHECK(*std::prev(view.rend()) == *values.begin());
}

TEST_CASE("array_view-access") {
  const auto emptyView = ArrayView<int>();
  CHECK(emptyView.empty());
  CHECK(emptyView.size() == 0);
  CHECK(emptyView.data() == nullptr);

  const auto values = {1, 2, 3, 4};
  const auto view = make_array_view(values);
  CHECK(!view.empty());
  CHECK(view.size() == values.size());
  CHECK(view.data() == &*values.begin());

  CHECK(!(emptyView == view));
  CHECK(emptyView != view);

  CHECK(view == view);
  CHECK(emptyView == emptyView);

  CHECK(view == make_array_view(view.begin(), view.end()));
  CHECK(view != make_array_view(view.begin(), view.end() - 1));
  CHECK(view != make_array_view(view.begin() + 1, view.end()));

  CHECK(view < make_array_view(view.begin() + 1, view.end()));
  CHECK(make_array_view(view.begin(), view.end() - 1) < view);
  CHECK(!(view < view));
  CHECK(!(view < make_array_view(view.begin(), view.end() - 1)));
  CHECK(!(make_array_view(view.begin() + 1, view.end()) < view));

  CHECK(view.front() == *values.begin());
  CHECK(view.back() == *std::prev(values.end()));

  CHECK(view[0] == *values.begin());
  CHECK(view[view.size() - 1] == *std::prev(values.end()));
}

TEST_CASE("array_view-subset") {
  auto checkEq = [](const ArrayView<int>& a, const std::initializer_list<int>& b) {
    REQUIRE(a.size() == b.size());
    CHECK(!memcmp(a.data(), &*b.begin(), sizeof(int) * a.size()));
  };

  const auto values = {1, 2, 3, 4};
  const auto view = make_array_view(values);

  checkEq(view, {1, 2, 3, 4});
  checkEq(view.subview(0, 4), {1, 2, 3, 4});
  checkEq(view.subview(1, 3), {2, 3, 4});
  checkEq(view.subview(1, 2), {2, 3});
  checkEq(view.subview(2, 2), {3, 4});
  checkEq(view.subview(2, 0), {});
  checkEq(view.subview(4, 0), {});

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
