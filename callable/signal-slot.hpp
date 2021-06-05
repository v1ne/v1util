#pragma once

#include "v1util/base/warnings.hpp"

V1_NO_WARNINGS
#include "v1util/third-party/nano-signal-slot/nano_function.hpp"
#include "v1util/third-party/nano-signal-slot/nano_mutex.hpp"
#include "v1util/third-party/nano-signal-slot/nano_observer.hpp"
#include "v1util/third-party/nano-signal-slot/nano_signal_slot.hpp"
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
