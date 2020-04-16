#include "v1util/callable/delegate.hpp"

#include "v1util/callable/function.hpp"

#include "doctest/doctest.h"

namespace v1util::test::delegate {

using Signature = int(int, const int&);
using Delegated = Delegate<Signature>;

using GenChar = Delegate<char()>;


static int sStaticFunction(int arg, const int& arg2) {
  return 2 * arg + arg2;
}

#ifdef V1_DELEGATE_BROKEN_COMPARISONS
static int sStaticFunction2(int, const int&) {
  return 2;
}
#endif

class Klaas {
 public:
  Klaas(int x) : mMember(x) {}

  int memberFunc(int arg1, const int& arg2) { return arg1 + arg2 + mMember; }
  int memberFunc2(int, const int&) { return 2; }

  float constMemberFunc() const { return -1.f; }

  static int staticFunc(int arg1) { return arg1 / 2; }
  static int staticFunc2(int) { return 2; }

  int mMember;
};

TEST_CASE("delegate-bind+call") {
  {
    const auto staticFuncDelegate = Delegated(&sStaticFunction);
    CHECK(staticFuncDelegate(23, -4) == 42);
  }

  {
    const auto staticLambdaDelegate =
        Delegated([](int arg, const int& arg2) { return arg / 2 + arg2; });
    CHECK(staticLambdaDelegate(42, 2) == 23);
  }

  {
    int capture = 0;
    auto capturingLambda = [&capture](
                               int arg, const int& arg2) -> int { return capture = arg - arg2; };
    const auto capturingLambdaDelegate = Delegated(capturingLambda);
    CHECK(capturingLambdaDelegate(13, 8) == 5);
    CHECK(capture == 5);
  }

  {
    Klaas k{66};

    const auto staticMemberFuncDelegate = Delegate<int(int)>(&Klaas::staticFunc);
    CHECK(staticMemberFuncDelegate(42) == 21);

    const auto memberFuncDelegate = Delegated::bind<&Klaas::memberFunc>(&k);
    CHECK(memberFuncDelegate(-1, 3) == 68);
    const auto memberFuncDelegateSafe = Delegated::bindMemFnSafe<Klaas, &Klaas::memberFunc>(&k);
    CHECK(memberFuncDelegateSafe(-1, 3) == 68);
    const auto memberFuncDelegateFast = Delegated::bindMemFnFast<Klaas, &Klaas::memberFunc>(&k);
    CHECK(memberFuncDelegateFast(-1, 3) == 68);

    const auto constMemFunDelegate = Delegate<float()>::bind<&Klaas::constMemberFunc>(&k);
    CHECK(constMemFunDelegate() == -1.f);
    const auto constMemFunDelegate2 =
        Delegate<float()>::bind<&Klaas::constMemberFunc>((const Klaas*)&k);
    CHECK(constMemFunDelegate2() == -1.f);

    const auto constMemFunDelegateSafe =
        Delegate<float()>::bindMemFnSafe<Klaas, &Klaas::constMemberFunc>(&k);
    CHECK(constMemFunDelegateSafe() == -1.f);
    const auto constMemFunDelegateSafe2 =
        Delegate<float()>::bindMemFnSafe<Klaas, &Klaas::constMemberFunc>((const Klaas*)&k);
    CHECK(constMemFunDelegateSafe2() == -1.f);

    const auto constMemFunDelegateFast =
        Delegate<float()>::bindMemFnFast<Klaas, &Klaas::constMemberFunc>(&k);
    CHECK(constMemFunDelegateFast() == -1.f);
    const auto constMemFunDelegateFast2 =
        Delegate<float()>::bindMemFnFast<Klaas, &Klaas::constMemberFunc>((const Klaas*)&k);
    CHECK(constMemFunDelegateFast2() == -1.f);
  }
}

TEST_CASE("delegate-cp+mv") {
  const auto ret23 = v1util::Delegate<int()>([]() { return 23; });
  const auto ret42 = v1util::Delegate<int()>([]() { return 42; });

  {
    v1util::Delegate<int()> copyConstructed(ret23);
    CHECK(copyConstructed() == 23);
    CHECK(ret23() == 23);
  }

  {
    auto copy23 = ret23;
    v1util::Delegate<int()> movedIn(std::move(copy23));
    CHECK(movedIn() == 23);
    CHECK(ret23() == 23);
  }

  {
    auto copyAssigned = ret23;
    copyAssigned = ret42;
    CHECK(copyAssigned() == 42);

    v1util::Delegate<int()> emptyAtStart;
    emptyAtStart = ret23;
    CHECK(emptyAtStart() == 23);
  }

  {
    auto copy42 = ret42;
    auto moveAssigned = ret23;
    moveAssigned = std::move(copy42);
    CHECK(moveAssigned() == 42);
  }
}


TEST_CASE("delegate-unbind") {
  auto ret23 = v1util::Delegate<int()>([]() { return 23; });
  CHECK(ret23);
  ret23 = {};
  CHECK(!ret23);
}

#ifdef V1_DELEGATE_BROKEN_COMPARISONS
TEST_CASE("delegate-comparisons") {
  const auto capturelessLambdaRet23 = v1util::Delegate<int()>([]() { return 23; });
  const auto capturelessLambdaRet42 = v1util::Delegate<int()>([]() { return 42; });
  {
    CHECK(capturelessLambdaRet23 == capturelessLambdaRet23);
    CHECK(capturelessLambdaRet42 == capturelessLambdaRet42);
    CHECK(capturelessLambdaRet23 != capturelessLambdaRet42);
    CHECK(capturelessLambdaRet42 != capturelessLambdaRet23);

    CHECK_FALSE(capturelessLambdaRet23 != capturelessLambdaRet23);
    CHECK_FALSE(capturelessLambdaRet42 != capturelessLambdaRet42);
    CHECK_FALSE(capturelessLambdaRet23 == capturelessLambdaRet42);
    CHECK_FALSE(capturelessLambdaRet42 == capturelessLambdaRet23);

    auto copy23 = capturelessLambdaRet23;
    CHECK(copy23 == capturelessLambdaRet23);
  }

  const auto staticFuncDelegate = Delegated(&sStaticFunction);
  const auto staticFuncDelegate2 = Delegated(&sStaticFunction2);
  {
    CHECK(staticFuncDelegate == staticFuncDelegate);
    CHECK(staticFuncDelegate != staticFuncDelegate2);
    auto copy = staticFuncDelegate;
    CHECK(copy == staticFuncDelegate);
  }

  int capture = 0;
  auto capturingLambda = [&capture](
                             int arg, const int& arg2) -> int { return capture = arg - arg2; };
  auto capturingLambda2 = [&capture](int, const int&) -> int { return capture; };
  const auto capturingLambdaDelegate = Delegated(capturingLambda);
  const auto capturingLambdaDelegate2 = Delegated(capturingLambda2);
  {
    CHECK(capturingLambdaDelegate == capturingLambdaDelegate);
    CHECK(capturingLambdaDelegate != capturingLambdaDelegate2);

    auto copy = capturingLambdaDelegate;
    CHECK(copy == capturingLambdaDelegate);
  }

  Klaas k{66};
  const auto memberFuncDelegate = Delegated::bind<&Klaas::memberFunc>(&k);
  const auto memberFuncDelegate2 = Delegated::bind<&Klaas::memberFunc2>(&k);
  {
    CHECK(memberFuncDelegate == memberFuncDelegate);
    CHECK(memberFuncDelegate != memberFuncDelegate2);

    auto copy = memberFuncDelegate;
    CHECK(copy == memberFuncDelegate);
  }

  {
    // equality between callbacks
    CHECK(staticFuncDelegate != memberFuncDelegate);
    CHECK(staticFuncDelegate != capturingLambdaDelegate);
    CHECK(capturingLambdaDelegate != memberFuncDelegate);
  }

  {
    // operator<
    const auto irreflexive = !(staticFuncDelegate < staticFuncDelegate);
    const auto trichotomy =
        (staticFuncDelegate < memberFuncDelegate) ^ (memberFuncDelegate < staticFuncDelegate);
    CHECK_EQ(irreflexive && trichotomy, true);
  }
}
#endif


class Base {
 public:
  virtual char virtualMethod() { return 'B'; }
};

class Unrelated {
 public:
  char nonvirtualBaseMethod() { return 'U'; }
};

class VirtualFirst : public Base, public Unrelated {
 public:
  char virtualMethod() override { return 'F'; }
  char nonvirtualMethod() { return 'M'; }
};

// use a different position of the base class, if that makes a difference:
class VirtualSecond : public Unrelated, public Base {
 public:
  char virtualMethod() override { return 'S'; }
};

TEST_CASE("delegate-inheritance+virtual_methods") {
  {
    VirtualFirst vf;
    Base b;
    Base& bViaVf = vf;

    // non-virtual:
    CHECK(GenChar::bind<&VirtualFirst::nonvirtualMethod>(&vf)() == 'M');
    CHECK(GenChar::bindMemFnFast<VirtualFirst, &VirtualFirst::nonvirtualMethod>(&vf)() == 'M');
    CHECK(GenChar::bindMemFnSafe<VirtualFirst, &VirtualFirst::nonvirtualMethod>(&vf)() == 'M');

    CHECK(GenChar::bind<&Unrelated::nonvirtualBaseMethod>(&vf)() == 'U');
    CHECK(GenChar::bindMemFnSafe<Unrelated, &Unrelated::nonvirtualBaseMethod>(&vf)() == 'U');
    CHECK(GenChar::bindMemFnFast<Unrelated, &Unrelated::nonvirtualBaseMethod>(&vf)() == 'U');

    CHECK(GenChar::bind<&VirtualFirst::nonvirtualBaseMethod>(&vf)() == 'U');
    /* doesn't compile:
    CHECK(GenChar::bindMemFnSafe<VirtualFirst, &VirtualFirst::nonvirtualBaseMethod>(&vf)() == 'U');
    CHECK(GenChar::bindMemFnFast<VirtualFirst, &VirtualFirst::nonvirtualBaseMethod>(&vf)() == 'U');
    */

    // virtual:
    CHECK(GenChar::bind<&Base::virtualMethod>(&b)() == 'B');
    CHECK(GenChar::bindMemFnFast<Base, &Base::virtualMethod>(&b)() == 'B');
    CHECK(GenChar::bindMemFnSafe<Base, &Base::virtualMethod>(&b)() == 'B');

    CHECK(GenChar::bind<&Base::virtualMethod>(&bViaVf)() == 'F');
    CHECK(GenChar::bindMemFnFast<Base, &Base::virtualMethod>(&bViaVf)() == 'F');
    CHECK(GenChar::bindMemFnSafe<Base, &Base::virtualMethod>(&bViaVf)() == 'F');

    CHECK(GenChar::bind<&Base::virtualMethod>(&vf)() == 'F');
    CHECK(GenChar::bindMemFnFast<Base, &Base::virtualMethod>(&vf)() == 'F');
    CHECK(GenChar::bindMemFnSafe<Base, &Base::virtualMethod>(&vf)() == 'F');

    CHECK(GenChar::bind<&VirtualFirst::virtualMethod>(&vf)() == 'F');
    CHECK(GenChar::bindMemFnFast<VirtualFirst, &VirtualFirst::virtualMethod>(&vf)() == 'F');
    CHECK(GenChar::bindMemFnSafe<VirtualFirst, &VirtualFirst::virtualMethod>(&vf)() == 'F');
  }

  {
    VirtualSecond vs;
    Base& bViaVs = vs;

    // non-virtual:
    CHECK(GenChar::bind<&Unrelated::nonvirtualBaseMethod>(&vs)() == 'U');
    CHECK(GenChar::bindMemFnSafe<Unrelated, &Unrelated::nonvirtualBaseMethod>(&vs)() == 'U');
    CHECK(GenChar::bindMemFnFast<Unrelated, &Unrelated::nonvirtualBaseMethod>(&vs)() == 'U');

    CHECK(GenChar::bind<&VirtualSecond::nonvirtualBaseMethod>(&vs)() == 'U');
    /* doesn't compile:
     CHECK(GenChar::bindMemFnSafe<VirtualSecond, &VirtualSecond::nonvirtualBaseMethod>(&vs)() ==
     'U'); CHECK(GenChar::bindMemFnFast<VirtualSecond, &VirtualSecond::nonvirtualBaseMethod>(&vs)()
     == 'U');
     */

    // virtual:
    CHECK(GenChar::bind<&Base::virtualMethod>(&bViaVs)() == 'S');
    CHECK(GenChar::bindMemFnFast<Base, &Base::virtualMethod>(&bViaVs)() == 'S');
    CHECK(GenChar::bindMemFnSafe<Base, &Base::virtualMethod>(&bViaVs)() == 'S');

    CHECK(GenChar::bind<&Base::virtualMethod>(&vs)() == 'S');
    CHECK(GenChar::bindMemFnFast<Base, &Base::virtualMethod>(&vs)() == 'S');
    CHECK(GenChar::bindMemFnSafe<Base, &Base::virtualMethod>(&vs)() == 'S');

    CHECK(GenChar::bind<&VirtualSecond::virtualMethod>(&vs)() == 'S');
    CHECK(GenChar::bindMemFnFast<VirtualSecond, &VirtualSecond::virtualMethod>(&vs)() == 'S');
    CHECK(GenChar::bindMemFnSafe<VirtualSecond, &VirtualSecond::virtualMethod>(&vs)() == 'S');
  }

#ifdef V1_DELEGATE_BROKEN_COMPARISONS
  {
    VirtualFirst vf;
    Base& bViaVf = vf;
    const auto viaBase = GenChar::bind<&Base::virtualMethod>(&bViaVf);
    const auto viaVf = GenChar::bind<&VirtualFirst::virtualMethod>(&vf);

    // If you compare methods from different base classes, you're on your own:
    const auto anythingGoes = (viaBase == viaVf) || (viaBase != viaVf);
    CHECK(anythingGoes);
  }
#endif
}


using DtorCallback = Function<void()>;
class BaseWithDtorCallback {
 public:
  BaseWithDtorCallback()
      : mFastBoundAtBaseConstructionTime(
          GenChar::bindMemFnFast<BaseWithDtorCallback, &BaseWithDtorCallback::method>(this))
      , mSafeBoundAtBaseConstructionTime(
            GenChar::bindMemFnSafe<BaseWithDtorCallback, &BaseWithDtorCallback::method>(this)) {}

  ~BaseWithDtorCallback() {
    CHECK(method() == 'B');
    mOnBaseDestruction();
  }
  virtual char method() { return 'B'; }
  void checkBase() { mOnBaseDestruction(); }

  DtorCallback mOnBaseDestruction;
  Delegate<char()> mFastBoundAtBaseConstructionTime;
  Delegate<char()> mSafeBoundAtBaseConstructionTime;
};
class DerivedWithDtorCallback : public BaseWithDtorCallback {
 public:
  ~DerivedWithDtorCallback() {
    CHECK(method() == 'D');
    mOnDerivedDestruction();
  }
  char method() override { return 'D'; }
  void checkDerived() { mOnDerivedDestruction(); }

  DtorCallback mOnDerivedDestruction;
};

/*! Delegates have special behaviour when classes w/ virtual methods are destroyed.
 *
 * Depending on your platform and optimization setting, the results for methods bound
 * at construction time and delegates called during destruction on virtual methods
 * will vary.
 */
TEST_CASE("delegate-virtual-destruction") {
  Delegate<char()> safeDelegate;
  Delegate<char()> fastDelegate;
  {
    DerivedWithDtorCallback instance;

    safeDelegate =
        Delegate<char()>::bindMemFnSafe<BaseWithDtorCallback, &BaseWithDtorCallback::method>(
            &instance);
    fastDelegate =
        Delegate<char()>::bindMemFnFast<BaseWithDtorCallback, &BaseWithDtorCallback::method>(
            &instance);

    CHECK(safeDelegate() == 'D');
    CHECK(fastDelegate() == 'D');

    CHECK(instance.mSafeBoundAtBaseConstructionTime() == 'D');
    const auto isCtorBoundBaseOrDerived = instance.mFastBoundAtBaseConstructionTime() == 'B'
                                          || instance.mFastBoundAtBaseConstructionTime() == 'D';
    CHECK(isCtorBoundBaseOrDerived);

    instance.mOnDerivedDestruction = DtorCallback([&]() {
      CHECK(safeDelegate() == 'D');
      CHECK(fastDelegate() == 'D');
    });

    instance.mOnBaseDestruction = DtorCallback([&]() {
      const auto safeCalledMethod = safeDelegate();
      const auto fastCalledMethod = fastDelegate();
      const auto isDtorCalledSafeBaseOrDerived = safeCalledMethod == 'B' || safeCalledMethod == 'D';
      const auto isDtorCalledFastBaseOrDerived =
          fastCalledMethod == 'B' || fastCalledMethod == 'D';  // XXX: Should be only 'B'
      CHECK(isDtorCalledSafeBaseOrDerived);
      CHECK(isDtorCalledFastBaseOrDerived);
    });

    // destructors run here:
  }
}


/*! Tests for the special case mentioned in Delegate::bindMemFn
 *
 * MSVC has an "interesting" ABI for member function calls. These tests ensure
 * that Delegate does the right thing even on MSVC with non-void/integral return types.
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

  auto staticFunc = v1util::Delegate<NonTrivial()>(&Foo::staticLargeRetVal);
  CHECK(staticFunc().mMember[2] == 96);

  {
    auto valGen = v1util::Delegate<PassByValue(int)>::bind<&Foo::retByValue>(&foo);
    auto retByValue = valGen(10);
    CHECK(retByValue.mMember == 15);
    CHECK(pFoo->method(2) == 4);

    retByValue = valGen(42);
    CHECK(retByValue.mMember == 47);
    CHECK(pFoo->method(256) == 512);
  }

  {
    auto valGen = v1util::Delegate<MsvcPassByValue2Object(int)>::bind<&Foo::retByValue2>(&foo);
    auto retByValue = valGen(10);
    CHECK(retByValue.mMember == 22);
    CHECK(pFoo->method(2) == 4);

    retByValue = valGen(42);
    CHECK(retByValue.mMember == 54);
    CHECK(pFoo->method(256) == 512);
  }


  {
    auto x = v1util::Delegate<MsvcPassByPointerObject(intptr_t)>::bind<&Foo::retByPtr>(&foo);
    auto retByPtr = x(0);
    CHECK(retByPtr.mMember == 0);
    CHECK(pFoo->method(2) == 4);

    retByPtr = x(42);
    CHECK(retByPtr.mMember == 42);
    CHECK(pFoo->method(256) == 512);
  }

  {
    auto y = v1util::Delegate<NonTrivial()>::bind<&Foo::largeRetVal>(&foo);
    auto largeVal = y();
    CHECK(largeVal.mMember[2] == 96);
    CHECK(pFoo->method(2) == 4);

    largeVal = y();
    CHECK(largeVal.mMember[2] == 96);
    CHECK(pFoo->method(2) == 4);
  }
}
}  // namespace msvcSpecialCase

}  // namespace v1util::test::delegate
