#pragma once

#if 1
#  include "v1util/base/warnings.hpp"

V1_NO_WARNINGS
#  include "v1util/third-party/nano-signal-slot/nano_function.hpp"
#  include "v1util/third-party/nano-signal-slot/nano_mutex.hpp"
#  include "v1util/third-party/nano-signal-slot/nano_observer.hpp"
#  include "v1util/third-party/nano-signal-slot/nano_signal_slot.hpp"
V1_RESTORE_WARNINGS


namespace v1util {

namespace detail {
/** By making the mutex a no-op, the SigSlot mechanism becomes single-threaded,
 * but safe to use on dying class instances. Still, disconnecting an observer
 * won't take effect immediately, but only after any current emission has ended.
 */
class SigSlot_NoMutex {
 public:
  inline void lock() noexcept {}
  inline void unlock() noexcept {}
};

}  // namespace detail


/*********************************************************
 * TODO: Make my own signal-slot with sane semantics
 *
 * Wanted:
 * - single-threaded
 * - auto-disconnection when slot owner dies
 * - non-owning connections
 * - new connections from within an emission of a signal are not called in that emission
 * - deleted connections from within an emission take effect immediately (bc. the emission is on the
 *   same thread and not queued, in contrast to queued singals)
 * - nested connection/disconnection does not crash, but may have degraded semantics
 *
 * stretch goal:
 * - output iterator for return values
 **********************************************************/

/** single-threaded signal (reentrant-safe for dying class instances)
 *
 * Danger: During the delivery of a signal, disconnecting a slot has no effect.
 * The signal will still be delivered to that slot. It's only disconnected for the
 * next delivery. Only if the slot is in a class instance destroyed during delivery,
 * the slot won't be called.
 *
 * Keep in mind: No copy. no move
 */
template <typename Signature>
using Signal = Nano::Signal<Signature, Nano::TS_Policy_Safe<detail::SigSlot_NoMutex>>;

/** signle-threaded slot owner w/ automatic disconnect on destruction
 * (reentrant-safe for dying class instances)
 *
 * Danger: During the delivery of a signal, disconnecting a slot has no effect.
 * The signal will still be delivered to that slot. It's only disconnected for the
 * next delivery. Only if the slot is in a class instance destroyed during delivery,
 * the slot won't be called.
 *
 * Keep in mind: No copy. no move
 */
using SigSlotObserver = Nano::Observer<Nano::TS_Policy_Safe<detail::SigSlot_NoMutex>>;

}  // namespace v1util
#else

#  include "function.hpp"
#  include "v1util/utility/scope.hpp"

#  include <memory>
#  include <vector>

namespace v1util {
namespace detail {
  struct SignalSlotConnection {
    // TODO: Point to non-templated base signal and use some stable reference for the connection
  };
}

class SlotLifetimeTracker {
 private:
  std::vector<detail::SignalSlotConnection> mSlotConnections;
};

/* Emitter of an object where listeners (Slots) can register
 */
template <typename Signature>
class Signal;

template <typename RetT, typename... Args>
class Signal<RetT(Args...)> {
 private:
  using FuncStorageT = Function<RetT(Args...)>;

  struct Slot {
    FuncStorageT slotFunction; 
    SlotLifetimeTracker* pOwner = nullptr;
    //TODO: some stable reference that identifies this connection so that pOwner can find it
  };

 public:
  V1_NO_CP_DEFAULT_MV_CTOR(Signal);

  // TODO: rename -> emit
  void fire(Args... args) {
    {
      ValueScope<bool>(mInEmission, true);

      for(const auto& slot : mSlots) {
        if(slot) slot(args...);
      }
    }

    if(!mSlotsToAdd.empty()) {
      mSlots.insert(mSlots.end(),
          std::make_move_iterator(mSlotsToAdd.begin()),
          std::make_move_iterator(mSlotsToAdd.end()));
      mSlotsToAdd.clear();
    }
  }

  template <auto pMemFn, typename Owner>
  void connect(Owner* pSlotOwner) {
    connect(FuncStorageT::template bind<pMemFn>(pSlotOwner),
        std::is_base_of_v<SlotLifetimeTracker, Owner>
            ? static_cast<SlotLifetimeTracker*>(pSlotOwner)
            : nullptr);
  }

  template <typename Functor>
  void connect(Functor&& f) {
    connect(FuncStorageT(f), nullptr);
  }


  template <auto pMemFn, typename Arg>
  void disconnect(Arg pSlotOwner) {}
  void disconnect_all(){};

 private:
  void connect(Function<RetT(Args...)>&& function, SlotLifetimeTracker* pOwner) {
    if(!mInEmission) {
      mSlots.emplace_back(std::move(function));
    } else {
      mSlotsToAdd.emplace_back(std::move(function));
    }

    if(pOwner)
      notifyOwnerAboutConnection(
          pOwner, nullptr /* TODO: Bad idea. Track connection differently. */);
  }

  void notifyOwnerAboutConnection(SlotLifetimeTracker* pOwner, void* pMemFn) {}
  void notifyOwnerAboutDisconnection(SlotLifetimeTracker* pOwner, void* pMemFn) {}

  std::vector<Slot> mSlots;
  std::vector<Slot> mSlotsToAdd;
  bool mInEmission = false;
};

#  define SigSlotObserver SlotLifetimeTracker
}  // namespace v1util


#endif
