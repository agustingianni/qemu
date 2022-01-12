import cpp

// The type `struct rcu_head` is used to protect other types.
class StructRCUHead extends Struct {
  StructRCUHead() { this.getName() = "rcu_head" }
}

// Finds all the types that have an RCU head field.
class RCUProtected extends Struct {
  RCUProtected() { this.getAField().getType() instanceof StructRCUHead }
}

class RCULockFunction extends Function {
  RCULockFunction() {
    exists(string name | name = this.getName() |
      name = "rcu_read_lock" or name = "rcu_read_auto_lock"
    )
  }
}

class RCULockCall extends FunctionCall {
  RCULockCall() { this.getTarget() instanceof RCULockFunction }
}

class NeedsRCULockFunction extends Function {
  NeedsRCULockFunction() {
    (
      // Access something that is RCUProtected.
      exists(RCUProtected protected |
        this = protected.getAField().getAnAccess().getEnclosingFunction()
      )
      or
      // Calls something that needs RCULock
      exists(NeedsRCULockFunction call |
        this = call.getACallToThisFunction().getEnclosingFunction()
      )
    ) and
    not exists(RCULockCall lock | this = lock.getEnclosingFunction())
  }
}

// RCU notes:
//
// - RCU supports concurrency between a single updater and multiple readers.
// - Each reader has a copy of the data
// - They are not freed up until all pre-existing read-side critical sections complete.
// - They are freed when there is an update to the resource (publish).
//
// Publish
//    rcu_assign_pointer(shared resource, some pointer)
// Subscribe
//    rcu_read_lock() &&
//    some_pointer = rcu_dereference(shared resource) &&
//    rcu_read_unlock()
//
// https://www.kernel.org/doc/Documentation/RCU/whatisRCU.txt
//
// rcu_read_lock
//    Enter read critical section.
//    Shared resource cannot be reclaimed.
//
// rcu_read_unlock
//    Exit critical section
//    Let the reclaimer know it can reclaim the shared resource
//
// synchronize_rcu
//    Marks the end of updater code
//    Marks the beginning of reclaimer code
//    Blocks until all the pre-existing read critical sections are over.
//    New rcu_read_lock() can happen
//
// call_rcu
//    Same as synchronize_rcu but it does it by calling a callback when the readers are done
//    Used to avoid blocking
//
// Auditing notes:
// RCU by definition makes readers possibly work on data that does not reflect the current
// state of the system. This is of course a bug. Say we have a struct handled by RCU and it
// contains a pointer, if things aren't properly handled.
//
// Found Bugs:
//
// # Log read without using RCU api.
// void hexagon_translate_init(void)
// {
//     int i;
//     opcode_init();
//     if (HEX_DEBUG) {
//         if (!qemu_logfile) {
//             qemu_set_log(qemu_loglevel);
//         }
//     }
// }
// 1. Anything RCU protected will be a pointer.
// 2. Find reads to that pointer without qatomic_rcu_read
query predicate isRCUType(Type type) { type instanceof RCUProtected }

query predicate findAccessToRCUProtected(Function function, VariableAccess access) {
  isRCUType(access.getTarget().getType().stripType()) and
  function = access.getEnclosingFunction()
}

// Find an access to an RCU protected field.
// Find if there is a call to RCULock.
from RCUProtected protected, VariableAccess access
where
  access = protected.getAField().getAnAccess() and
  protected.getName() = "QemuLogFile" and
  not exists(RCULockCall lock | lock.getEnclosingFunction() = access.getEnclosingFunction()) and
  not access.getFile().getAbsolutePath().matches("%test%")
select access.getEnclosingFunction(), protected, access

