#include "v1util/callable/signal-slot.hpp"

#include "doctest/doctest.h"


namespace v1util::test {
class SlotOwner : public SigSlotObserver {
 public:
  SlotOwner(int& fireCount) : mFireCount(fireCount) {}

  void slot(int val) {
    ++mFireCount;
    CHECK(val == 23);
  }

  void killerSlot(std::unique_ptr<SlotOwner>& a, std::unique_ptr<SlotOwner>& b) {
    ++mFireCount;
    CHECK(a);
    CHECK(b);

    INFO("x.reset() should not deadlock, regardless of whether x == this");
    WARN(false);
    if(a.get() != this) a.reset();
    if(b.get() != this) b.reset();
  }

  int& mFireCount;
};

TEST_CASE("signal-slot easy case") {
  Signal<void(int)> sig;
  int fireCount = 0;
  {
    SlotOwner slotOwner(fireCount);

    sig.connect<&SlotOwner::slot>(&slotOwner);
    sig.fire(23);
    sig.fire(23);

    CHECK(fireCount == 2);
  }
  sig.fire(42);
  CHECK(fireCount == 2);
}

TEST_CASE("signal-slot nested observer destruction (w/ member functions)") {
  Signal<void(std::unique_ptr<SlotOwner> & a, std::unique_ptr<SlotOwner> & b)> sig;

  int fireCount = 0;
  auto so1 = std::make_unique<SlotOwner>(fireCount);
  auto so2 = std::make_unique<SlotOwner>(fireCount);

  sig.connect<&SlotOwner::killerSlot>(so1.get());
  sig.connect<&SlotOwner::killerSlot>(so2.get());
  sig.fire(so1, so2);
  const auto exactlyOneDied = bool(so1) ^ bool(so2);
  CHECK(exactlyOneDied);
  CHECK(fireCount == 1);
  // and: did not crash
}

TEST_CASE("signal-slot nested disconnection (non-member functions)") {
  int fireCount = 0;

  Signal<void()> sig;

  auto disconnectingFirer1 = [&]() {
    ++fireCount;
    sig.disconnect_all();
  };
  auto disconnectingFirer2 = [&]() {
    ++fireCount;
    sig.disconnect_all();
  };

  sig.connect(disconnectingFirer1);
  sig.connect(disconnectingFirer2);
  sig.fire();

  INFO("SigSlot fires on disconnected listeners. :<");
  WARN(fireCount == 1);
}

}  // namespace v1util::test
