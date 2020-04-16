#include "v1util/callable/delegate.hpp"

#include "doctest/doctest.h"

namespace v1util { namespace test { namespace cppvtable {

using CallableKlaasMemFn = int (*)(void*, int, const int&);
using CallableFoo = int (*)(void*);

class Klaas {
 public:
  Klaas(int x) : mMember(x) {}

  int memberFunc(int arg1, const int& arg2) { return arg1 + arg2 + mMember; }

  int mMember;
};

class Klaasson : public Klaas {
 public:
  Klaasson(int x) : Klaas(x) {}

  int memberFunc(int arg1, const int& arg2) { return 2 * Klaas::memberFunc(arg1, arg2); }
};


class VBase1 {
 public:
  virtual int foo() { return mFoo; }
  int mFoo = 5;
};

class VBase2 {
 public:
  virtual int foo2() { return mFoo2; }
  int mFoo2 = 7;
};


class Laars : public VBase1, public VBase2 {
 public:
  int foo() override { return -5 * mMember; };
  int foo2() override { return 5 * mMember; };
  int memberFunc(int arg1, const int& arg2) { return mMember * arg1 + arg2 + 44; }

  int mMember = -2;
};


TEST_CASE("resolveMemberFn-nonvirtual") {
  Klaas k(23);
  auto callK = resolveUntypedMemberFn(&k, &Klaas::memberFunc);
  CHECK(((CallableKlaasMemFn)callK.pMemFn)(callK.pObject, 1, 2) == 26);

  // inheritance:
  Klaasson k2(42);
  auto callKson = resolveUntypedMemberFn(&k2, &Klaasson::memberFunc);
  CHECK(((CallableKlaasMemFn)callKson.pMemFn)(callKson.pObject, 2, -23) == 2 * 21);
  auto callK2 = resolveUntypedMemberFn(&k2, &Klaas::memberFunc);
  CHECK(((CallableKlaasMemFn)callK2.pMemFn)(callK2.pObject, 2, 6) == 50);
}

TEST_CASE("resolveMemberFn-virtual") {
  Laars l;
  auto nonvirtCall = resolveUntypedMemberFn(&l, &Laars::memberFunc);
  CHECK(((CallableKlaasMemFn)nonvirtCall.pMemFn)(nonvirtCall.pObject, 2, -10)
        == ((&l)->*&Laars::memberFunc)(2, -10));

  auto callDirect = resolveUntypedMemberFn(&l, &Laars::foo);
  CHECK(((CallableFoo)callDirect.pMemFn)(callDirect.pObject) == ((&l)->*&Laars::foo)());

  auto callDirect2 = resolveUntypedMemberFn(&l, &Laars::foo2);
  CHECK(((CallableFoo)callDirect2.pMemFn)(callDirect2.pObject) == ((&l)->*&Laars::foo2)());

  auto virtualCall1 = resolveUntypedMemberFn(&l, &VBase1::foo);
  CHECK(((CallableFoo)virtualCall1.pMemFn)(virtualCall1.pObject) == ((&l)->*&VBase1::foo)());
  auto virtualCall2 = resolveUntypedMemberFn(&l, &VBase2::foo2);
  CHECK(((CallableFoo)virtualCall2.pMemFn)(virtualCall2.pObject) == ((&l)->*&VBase2::foo2)());

  auto callOnBaseObject = resolveUntypedMemberFn((VBase1*)&l, &VBase1::foo);
  CHECK(((CallableFoo)callOnBaseObject.pMemFn)(callOnBaseObject.pObject)
        == (((VBase1*)&l)->*&VBase1::foo)());

  auto narrowLaars = (VBase1)l;
  auto callOnNarrowed = resolveUntypedMemberFn(&narrowLaars, &VBase1::foo);
  CHECK(((CallableFoo)callOnNarrowed.pMemFn)(callOnNarrowed.pObject)
        == ((&narrowLaars)->*&VBase1::foo)());
}

}}}  // namespace v1util::test::cppvtable
