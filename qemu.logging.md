# Qemu Logging Implementation

We are reading this because it has to do with RCU and it is simple.

## Initialization

There is a global `qemu_logfile` that I'm guessing is the shared pointer and will be accessed via some kind of `rcu` mechanism.

```c++
typedef struct QemuLogFile {
    struct rcu_head rcu;
    FILE *fd;
} QemuLogFile;

QemuLogFile *qemu_logfile;
```

We also have a `mutex` called `qemu_logfile_mutex` that is taken in two functions `qemu_set_log` and `qemu_log_close`. Basically when there is a need to update `qemu_logfile` this lock needs to be taken:

```cpp
static QemuMutex qemu_logfile_mutex;
``

The function `qemu_set_log` besides setting the log level it initializes the `qemu_logfile` pointer, therefore it needs to lock `qemu_logfile_mutex`.

```cpp
void qemu_set_log(int log_flags) {
    QEMU_LOCK_GUARD(&qemu_logfile_mutex);

    logfile = g_new0(QemuLogFile, 1);
    logfile->fd = fopen(logfilename, log_append ? "a" : "w");

    qatomic_rcu_set(&qemu_logfile, logfile);
}
```

In order to let the possible readers of `qemu_logfile`, the `RCU API` call `qatomic_rcu_set` is used. This changes `qemu_logfile` atomically , since it is a wrapper of `__atomic_store_n`, so any reader will either get an old version of the shared resource or the new one.

The other thing that `qemu_set_log` is destroying a `QemuLogFile`. To do so, it takes `qemu_logfile_mutex` because it needs to change the value of the pointer `qemu_logfile`.

One may ask why isn't `qatomic_rcu_read` used to read the value of `qemu_logfile`? The answer is that we will not be protecting `qemu_logfile` from another call to `qemu_set_log`.

```cpp
QEMU_LOCK_GUARD(&qemu_logfile_mutex);

logfile = qemu_logfile;
qatomic_rcu_set(&qemu_logfile, NULL);
call_rcu(logfile, qemu_logfile_free, rcu);
```

After locking and setting `qemu_logfile` to `NULL`, a call to `call_rcu` is made that will execute the callback `qemu_logfile_free` when there are no more reads to the old version of `qemu_logfile` stored in `logfile`.