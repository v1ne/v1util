#include "v1util/callable/function.hpp"

#include "v1util/base/warnings.hpp"

#include "doctest/doctest.h"


namespace v1util::test::function {

namespace {

using Signature = int(int, const int&);

static int sStaticFunction(int arg, const int& arg2) {
  return 2 * arg + arg2;
}

static int sStaticFunction23() {
  return 23;
}

static int sStaticFunction42() {
  return 42;
}

class TriviallyCopyableCallable {
 public:
  TriviallyCopyableCallable(int x) : mMember(x) {}
  int operator()(int arg1, const int& arg2) { return arg1 - arg2 + mMember; }
  int operator()() { return 23; }
  int mMember;
};

struct HeapBasedCallable {
  // It's heap-based because it's non-trivially copyable

  struct OpCounters {
    int ctors = 0;
    int dtors = 0;
    int copies = 0;
    int moves = 0;

    friend bool operator==(const OpCounters& a, const OpCounters& b) {
      return a.ctors == b.ctors && a.dtors == b.dtors && a.copies == b.copies && a.moves == b.moves;
    }
  };

  HeapBasedCallable(OpCounters* pOpCounters) : mpOpCounters(pOpCounters), mAlive(true) {
    mpOpCounters->ctors++;
  }
  HeapBasedCallable(const HeapBasedCallable& other)
      : mpOpCounters(other.mpOpCounters), mAlive(true) {
    V1_ASSERT(other.mAlive);
    mpOpCounters->copies++;
  }
  HeapBasedCallable(HeapBasedCallable&& other) noexcept
      : mpOpCounters(other.mpOpCounters), mAlive(true) {
    V1_ASSERT(other.mAlive);
    other.mAlive = false;
    mpOpCounters->moves++;
  }
  ~HeapBasedCallable() {
    if(mAlive) mpOpCounters->dtors++;
    mAlive = false;
  }

  int operator()(int arg1, const int& arg2) {
    V1_ASSERT(mAlive);
    return arg1 * arg2;
  }
  int operator()() { return 23; }

  OpCounters* mpOpCounters = nullptr;
  bool mAlive = false;
};

struct CopyOnlyCallable {
  CopyOnlyCallable(int value) : mMember(value) {}
  CopyOnlyCallable(const CopyOnlyCallable&) = default;
  CopyOnlyCallable(CopyOnlyCallable&&) = delete;
  CopyOnlyCallable& operator=(const CopyOnlyCallable&) = default;
  // CopyOnlyCallable& operator=(CopyOnlyCallable&&) = delete;

  int operator()(int arg1, const int& arg2) { return arg1 + arg2 + mMember; }
  int operator()() { return 23; }

  int mMember;
};

struct MoveOnlyCallable {
  MoveOnlyCallable(int value) : mMember(value) {}
  MoveOnlyCallable(const MoveOnlyCallable&) = delete;
  MoveOnlyCallable(MoveOnlyCallable&&) = default;
  MoveOnlyCallable& operator=(const MoveOnlyCallable&) = delete;
  MoveOnlyCallable& operator=(MoveOnlyCallable&&) = default;

  int operator()(int arg1, const int& arg2) { return arg1 - arg2 + mMember; }
  int operator()() { return 23; }

  int mMember;
};

class Klaas {
 public:
  Klaas(int x) : mMember(x) {}

  int memberFunc(int arg1, const int& arg2) { return arg1 + arg2 + mMember; }
  int memberFunc23() { return 23; }
  int memberFunc42() { return 42; }

  int mMember;
};

}  // namespace


TEST_CASE("function-construction+call") {
  auto emptyFunction = Function<Signature>();
  CHECK(!emptyFunction);

  {
    const auto funcForStaticFunc = Function<Signature>(&sStaticFunction);
    CHECK(funcForStaticFunc);
    CHECK(funcForStaticFunc(23, -4) == 42);
    CHECK(!funcForStaticFunc.isHeapBased());
    CHECK(funcForStaticFunc._refCount() == 0);
  }

  {
    const auto staticLambda = [](int arg, const int& arg2) { return arg / 2 + arg2; };
    const auto funcForStaticLambda = Function<Signature>(staticLambda);
    CHECK(funcForStaticLambda);
    CHECK(funcForStaticLambda(42, 2) == 23);
    // trivially copyable, except on MSVC:
    CHECK(
        !std::is_trivially_copyable_v<decltype(staticLambda)> == funcForStaticLambda.isHeapBased());
  }

  {
    int capture = 0;
    auto capturingLambda = [&capture](
                               int arg, const int& arg2) -> int { return capture = arg - arg2; };
    const auto funcForCapturingLambda = Function<Signature>(capturingLambda);
    CHECK(funcForCapturingLambda);
    CHECK(funcForCapturingLambda(13, 8) == 5);
    CHECK(capture == 5);
    // trivially copyable, except on MSVC:
    CHECK(!std::is_trivially_copyable_v<decltype(
              capturingLambda)> == funcForCapturingLambda.isHeapBased());
  }

  {
    static_assert(std::is_trivially_copyable_v<TriviallyCopyableCallable>);
    auto callable = TriviallyCopyableCallable(123);
    const auto funcForEmbeddedCallable = Function<Signature>(callable);
    CHECK(funcForEmbeddedCallable);
    CHECK(funcForEmbeddedCallable(100, 23) == 200);
    CHECK(!funcForEmbeddedCallable.isHeapBased());
    CHECK(funcForEmbeddedCallable._refCount() == 0);
  }

  {
    static_assert(!std::is_trivially_copyable_v<HeapBasedCallable>);
    HeapBasedCallable::OpCounters counters;
    const auto funcForHeapBasedCallable = Function<Signature>(HeapBasedCallable(&counters));
    CHECK(counters == HeapBasedCallable::OpCounters{1, 0, 0, 1});
    CHECK(funcForHeapBasedCallable);
    CHECK(funcForHeapBasedCallable(-23, -2) == 46);
    CHECK(funcForHeapBasedCallable.isHeapBased());
    CHECK(funcForHeapBasedCallable._refCount() == 1);
  }

#if 0
  // std::function doesn't do it, I won't do it.
  {
#  if !defined(_MSC_VER)
    static_assert(std::is_trivially_copyable_v<CopyOnlyCallable>);
    bool isHeapBased = false;
    size_t refCount = 0;
#  else
    static_assert(!std::is_trivially_copyable_v<CopyOnlyCallable>,
        "MSVC has fixed a bug, please change the code.");
    bool isHeapBased = true;
    size_t refCount = 1;
#  endif
    const auto copyOnlyCallable = CopyOnlyCallable(1234);
    const auto funcForCopyOnlyCallable = Function<Signature>(copyOnlyCallable);
    CHECK(funcForCopyOnlyCallable);
    CHECK(funcForCopyOnlyCallable(-230, -4) == 1000);
    CHECK(funcForCopyOnlyCallable.isHeapBased() == isHeapBased);
    CHECK(funcForCopyOnlyCallable._refCount() == refCount);
  }
#endif

  {
    // not trivially copyable on MSVC, but trivially copyable on GCC:
    size_t refCount = !std::is_trivially_copyable_v<MoveOnlyCallable> ? 1 : 0;
    const auto funcForMoveOnlyCallable = Function<Signature>(MoveOnlyCallable(1234));
    CHECK(funcForMoveOnlyCallable);
    CHECK(funcForMoveOnlyCallable(-230, 4) == 1000);
    CHECK(funcForMoveOnlyCallable.isHeapBased() == !std::is_trivially_copyable_v<MoveOnlyCallable>);
    CHECK(funcForMoveOnlyCallable._refCount() == refCount);
  }

  {
    Klaas k{66};
    const auto funcForMemberFunc = Function<Signature>::bind<&Klaas::memberFunc>(&k);
    CHECK(funcForMemberFunc);
    CHECK(funcForMemberFunc(-1, 3) == 68);
    CHECK(!funcForMemberFunc.isHeapBased());
    CHECK(funcForMemberFunc._refCount() == 0);
  }
}

TEST_CASE("function-basic_ops") {
  const auto funcForStaticFunc = Function<int()>(&sStaticFunction23);

  const auto staticLambda = []() -> int { return 23; };
  const auto funcForStaticLambda = Function<int()>(staticLambda);


  int capture = 23;
  auto capturingLambda = [&capture]() -> int { return capture; };
  const auto funcForCapturingLambda = Function<int()>(capturingLambda);

  auto callable = TriviallyCopyableCallable(123);
  const auto funcForEmbeddedCallable = Function<int()>(callable);

  HeapBasedCallable::OpCounters counters;
  const auto funcForHeapBasedCallable = Function<int()>(HeapBasedCallable(&counters));

  const auto funcForMoveOnlyCallable = Function<int()>(MoveOnlyCallable(42));

  Klaas k{66};
  const auto funcForMemberFunc = Function<int()>::bind<&Klaas::memberFunc23>(&k);

  auto pFuncs = {&funcForStaticLambda, &funcForCapturingLambda, &funcForEmbeddedCallable,
      &funcForMoveOnlyCallable, &funcForHeapBasedCallable, &funcForMemberFunc};
  for(const Function<int()>* pFunc : pFuncs) {
    const auto refCounted = pFunc->isHeapBased() ? 1 : 0;
    CHECK(pFunc->_refCount() == 1 * refCounted);

    SUBCASE("copy+move") {
      // construction:
      auto x = *pFunc;
      auto y = v1util::Function<int()>([]() { return 42; });
      CHECK(x._refCount() == 2 * refCounted);

      v1util::Function<int()> copy(x);
      CHECK(copy() == 23);
      CHECK(x() == 23);
      CHECK(copy._refCount() == 3 * refCounted);

      v1util::Function<int()> movedIn(std::move(x));
      CHECK(movedIn() == 23);
      CHECK(movedIn._refCount() == 3 * refCounted);

      // assignment:
      auto copyTarget = y;
      CHECK(copyTarget() == 42);
      copyTarget = movedIn;
      CHECK(copyTarget() == 23);
      CHECK(copyTarget._refCount() == 4 * refCounted);

      auto moveTarget = y;
      moveTarget = std::move(copy);
      CHECK(moveTarget() == 23);
      CHECK(moveTarget._refCount() == 4 * refCounted);

      v1util::Function<int()> emptyAtStart;
      emptyAtStart = movedIn;
      CHECK(emptyAtStart() == 23);
      CHECK(emptyAtStart._refCount() == 5 * refCounted);

      // clear
      auto explicitClear = emptyAtStart;
      CHECK(explicitClear() == 23);
      CHECK(explicitClear._refCount() == 6 * refCounted);
      explicitClear = {};
      CHECK(!explicitClear);
      CHECK(emptyAtStart._refCount() == 5 * refCounted);

      auto selfAssignment = moveTarget;
      CHECK(selfAssignment._refCount() == 6 * refCounted);
      {
        V1_NO_WARN_SELF_ASSIGNMENT
        selfAssignment = selfAssignment;
        V1_RESTORE_WARNINGS
      }
      CHECK(selfAssignment);
      CHECK(selfAssignment() == 23);
      CHECK(selfAssignment._refCount() == 6 * refCounted);
    }

    CHECK(pFunc->_refCount() == 1 * refCounted);
  }
}


TEST_CASE("function-callable-refcounting") {
  /*
   * Ensure that the heap object (if present) is never moved or copied after storage
   * and only destroyed exactly once, at the end.
   */

  HeapBasedCallable::OpCounters counters;
  {
    HeapBasedCallable callable(&counters);
    {
      CHECK(counters == HeapBasedCallable::OpCounters{1, 0, 0, 0});

      auto func = v1util::Function<int()>(callable);
      CHECK(func._refCount() == 1);
      CHECK(counters == HeapBasedCallable::OpCounters{1, 0, 1, 0});

      // alive:
      CHECK(callable());
      CHECK(func() == 23);


      // copy + move of Function don't affect the stored object at all:
      auto tempCopy = func;
      CHECK(func._refCount() == 2);

      auto targetFunc = decltype(func)(std::move(tempCopy));
      CHECK(targetFunc._refCount() == 2);

      targetFunc = func;
      CHECK(targetFunc._refCount() == 2);

      targetFunc = std::move(func);
      CHECK(targetFunc._refCount() == 1);

      func = {};
      CHECK(targetFunc._refCount() == 1);
      CHECK(targetFunc() == 23);
      CHECK(counters == HeapBasedCallable::OpCounters{1, 0, 1, 0});
    }
    CHECK(counters == HeapBasedCallable::OpCounters{1, 1, 1, 0});
  }
  CHECK(counters == HeapBasedCallable::OpCounters{1, 2, 1, 0});
}

TEST_CASE("function-equality") {
  SUBCASE("empty") {
    CHECK(Function<int()>() == Function<int()>());
    CHECK(!(Function<int()>() != Function<int()>()));
  }

  SUBCASE("static") {
    auto f23 = Function<int()>(&sStaticFunction23);
    auto f42 = Function<int()>(&sStaticFunction42);

    CHECK(f23 == f23);
    CHECK(f23 != f42);

    CHECK(f23 == Function<int()>(&sStaticFunction23));
    CHECK(f42 != Function<int()>(&sStaticFunction23));

    auto f23Copy = f23;
    CHECK(f23 == f23Copy);
  }


  SUBCASE("fast member functions") {
    const auto fStatic = Function<int()>(&sStaticFunction23);

    Klaas k{66}, j{99};
    const auto fMemFnK = Function<int()>::bind<&Klaas::memberFunc23>(&k);
    const auto fMemFnJ = Function<int()>::bind<&Klaas::memberFunc23>(&j);

    CHECK(fMemFnK != fMemFnJ);
    CHECK(fMemFnK != fStatic);
    CHECK(fMemFnK == Function<int()>::bind<&Klaas::memberFunc23>(&k));
    CHECK(fMemFnK != Function<int()>::bind<&Klaas::memberFunc42>(&k));

    auto fMemFnKCopy = fMemFnK;
    CHECK(fMemFnK == fMemFnKCopy);
  }
}


/** Tests for the special case mentioned in Function::bindMemFn
 *
 * MSVC has an "interesting" ABI for member function calls. These tests ensure
 * that Function does the right thing even on MSVC with non-void/integral return types.
 */
namespace msvcSpecialCase {

struct PassByValue {
  int mMember;
};

class MsvcPassByValue2Object {
 public:
  MsvcPassByValue2Object() = default;
  intptr_t mMember = 0;
};

class MsvcPassByPointerObject {
 public:
  MsvcPassByPointerObject() = default;
  MsvcPassByPointerObject(intptr_t value) : mMember(value) {}
  intptr_t mMember = 0;
};

class IFoo {
 public:
  virtual int method(int arg) = 0;
};

class NonTrivial : public IFoo {
 public:
  NonTrivial() = default;
  int method(int) override { return -1; }

  intptr_t mMember[16] = {23, 42, 96, 1, 2, 3, 4, 5, 6};
};

// clang-format off
class Foo : public IFoo {
 public:
  int method(int arg) override { return 2 * arg; }
  PassByValue retByValue(int value) { PassByValue ret; ret.mMember = value + 5; return ret; }
  MsvcPassByValue2Object retByValue2(int value) { MsvcPassByValue2Object ret; ret.mMember = value + 12; return ret; }
  MsvcPassByPointerObject retByPtr(intptr_t value) { return {value}; }
  NonTrivial largeRetVal() { return {}; }
  static NonTrivial staticLargeRetVal() { return {}; }
};
// clang-format on

TEST_CASE("delegate-large_ret") {
  Foo foo;
  IFoo* pFoo = &foo;

  // The vtable is used as a canary because it's at the start of *pFoo. If it's clobbered, virtual
  // calls will fail.
  CHECK(pFoo->method(2) == 4);
  CHECK(foo.retByPtr(23).mMember == 23);

  auto staticFunc = v1util::Function<NonTrivial()>(&Foo::staticLargeRetVal);
  CHECK(staticFunc().mMember[2] == 96);

  {
    auto valGen = v1util::Function<PassByValue(int)>::bind<&Foo::retByValue>(&foo);
    auto retByValue = valGen(10);
    CHECK(retByValue.mMember == 15);
    CHECK(pFoo->method(2) == 4);

    retByValue = valGen(42);
    CHECK(retByValue.mMember == 47);
    CHECK(pFoo->method(256) == 512);
  }

  {
    auto valGen = v1util::Function<MsvcPassByValue2Object(int)>::bind<&Foo::retByValue2>(&foo);
    auto retByValue = valGen(10);
    CHECK(retByValue.mMember == 22);
    CHECK(pFoo->method(2) == 4);

    retByValue = valGen(42);
    CHECK(retByValue.mMember == 54);
    CHECK(pFoo->method(256) == 512);
  }


  {
    auto x = v1util::Function<MsvcPassByPointerObject(intptr_t)>::bind<&Foo::retByPtr>(&foo);
    auto retByPtr = x(0);
    CHECK(retByPtr.mMember == 0);
    CHECK(pFoo->method(2) == 4);

    retByPtr = x(42);
    CHECK(retByPtr.mMember == 42);
    CHECK(pFoo->method(256) == 512);
  }

  {
    auto y = v1util::Function<NonTrivial()>::bind<&Foo::largeRetVal>(&foo);
    auto largeVal = y();
    CHECK(largeVal.mMember[2] == 96);
    CHECK(pFoo->method(2) == 4);

    largeVal = y();
    CHECK(largeVal.mMember[2] == 96);
    CHECK(pFoo->method(2) == 4);
  }
}
}  // namespace msvcSpecialCase


}  // namespace v1util::test::function
