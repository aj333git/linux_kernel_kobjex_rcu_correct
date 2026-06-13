# KOBJX: Production-Grade SLAB + RCU + Shrinker Registry (Linux Kernel 5.15)

A complete Linux kernel module demonstrating how modern kernel subsystems combine:

* SLAB Allocators
* RCU (Read-Copy-Update)
* Reference Counting
* Hash Tables
* Linked Lists
* Shrinkers
* Per-CPU Statistics
* ProcFS Monitoring

The project implements a lockless registry capable of serving concurrent readers while safely reclaiming memory under pressure.

---

# Architecture Overview

The registry separates:

1. Structural visibility
2. Memory lifetime

Objects can disappear from global structures immediately while remaining alive for existing readers.

```text
                        WRITER THREAD
                     (Process Context)

                 kobjx_alloc() / delete()
                           |
                           v
                 +------------------+
                 |   spin_lock()    |
                 +------------------+
                           |
                           v
                    hash_del_rcu()
                    list_del_init()
                           |
                           v
                      kobjx_put()
                           |
                           v
                    refcount == 0 ?
                           |
                          YES
                           |
                           v
                      call_rcu()
                           |
                           v
                +--------------------+
                |  RCU Grace Period  |
                +--------------------+
                           |
                           v
                 rcu_free_callback()
                           |
                           v
                  kmem_cache_free()


------------------------------------------------------


                    READER THREADS

                  rcu_read_lock()
                           |
                           v
                    Hash Lookup
                           |
                           v
             refcount_inc_not_zero()
                           |
                           v
                 Safe Object Access
                           |
                           v
                 rcu_read_unlock()
                           |
                           v
                      kobjx_put()
```

---

# Why This Project Exists

A common kernel design problem:

```text
Reader CPU
     |
     v
Lookup Object
     |
     v
Returns Pointer
```

Meanwhile:

```text
Writer CPU
     |
     v
Deletes Object
     |
     v
Frees Memory
```

Reader now accesses invalid memory.

Result:

```text
Use After Free (UAF)
Kernel Oops
System Crash
```

This module demonstrates the correct production-grade solution using:

```text
RCU
+
Reference Counting
```

---

# Features

* Lockless reader lookups
* RCU protected hash table
* SLAB backed object allocation
* Reference counted lifecycle
* Automatic reclaim through shrinker
* ProcFS monitoring interface
* Per-CPU allocation statistics
* Safe asynchronous memory destruction

---

# Object Lifecycle

```text
ALLOC
  |
  v
VISIBLE IN HASH TABLE
  |
  v
LOOKUP BY READERS
  |
  v
UNLINK
  |
  v
REFCOUNT DROPS TO ZERO
  |
  v
call_rcu()
  |
  v
RCU GRACE PERIOD
  |
  v
SLAB FREE
```

---

# Core Data Structure

```c
struct kobjx {
    int id;
    char name[NAME_LEN];

    unsigned long created_jiffies;

    refcount_t refcnt;

    struct list_head list;
    struct hlist_node hnode;

    struct rcu_head rcu;
};
```

Field explanation:

| Field           | Purpose              |
| --------------- | -------------------- |
| id              | Registry key         |
| name            | Object name          |
| created_jiffies | Creation timestamp   |
| refcnt          | Lifetime protection  |
| list            | Global linked list   |
| hnode           | Hash table node      |
| rcu             | Deferred destruction |

---

# Memory Allocation

Objects are allocated from a dedicated SLAB cache.

```c
cache = kmem_cache_create(
        "kobjx_cache",
        sizeof(struct kobjx),
        0,
        SLAB_POISON | SLAB_RED_ZONE,
        ctor);
```

Benefits:

* Fast allocation
* Reduced fragmentation
* Cache locality
* Memory debugging support

---

# Constructor

The constructor initializes reused SLAB objects.

```c
static void ctor(void *obj)
{
    struct kobjx *k = obj;

    k->id = -1;

    strscpy(k->name, "init", NAME_LEN);

    INIT_LIST_HEAD(&k->list);
}
```

Why?

Without initialization, recycled objects may contain stale data.

---

# Object Allocation

```c
static struct kobjx *kobjx_alloc(
        int id,
        const char *name)
{
    struct kobjx *k;

    k = kmem_cache_alloc(
            cache,
            GFP_KERNEL);

    if (!k)
        return NULL;

    k->id = id;

    strscpy(k->name, name, NAME_LEN);

    refcount_set(&k->refcnt, 1);

    spin_lock(&lock);

    list_add_tail(
        &k->list,
        &global_list);

    hash_add_rcu(
        global_table,
        &k->hnode,
        id);

    spin_unlock(&lock);

    return k;
}
```

What happens?

```text
Allocate Memory
      |
      v
Initialize Fields
      |
      v
Insert Into List
      |
      v
Insert Into Hash Table
      |
      v
Visible To Readers
```

---

# Lockless RCU Lookup

```c
static struct kobjx *kobjx_lookup(int id)
{
    struct kobjx *k;

    rcu_read_lock();

    hash_for_each_possible_rcu(
            global_table,
            k,
            hnode,
            id) {

        if (k->id == id) {

            if (!refcount_inc_not_zero(
                    &k->refcnt))
                k = NULL;

            rcu_read_unlock();

            return k;
        }
    }

    rcu_read_unlock();

    return NULL;
}
```

Why is this safe?

The reference count is increased before leaving the RCU section.

```text
Reader CPU

rcu_read_lock()
       |
       v
find object
       |
       v
refcount++ 
       |
       v
rcu_read_unlock()

Object cannot disappear now.
```

---

# Object Deletion

```c
static void kobjx_delete(
        struct kobjx *k)
{
    spin_lock(&lock);

    list_del_init(&k->list);

    hash_del_rcu(&k->hnode);

    spin_unlock(&lock);

    kobjx_put(k);
}
```

Important:

```text
DELETE
does NOT mean
FREE MEMORY
```

Instead:

```text
UNLINK
    |
    v
Drop Reference
    |
    v
RCU decides when free is safe
```

---

# Reference Management

Centralized lifetime control:

```c
static void kobjx_put(
        struct kobjx *k)
{
    if (refcount_dec_and_test(
            &k->refcnt)) {

        call_rcu(
            &k->rcu,
            rcu_free_callback);
    }
}
```

When the last reference disappears:

```text
Refcount 1
     |
     v
Refcount 0
     |
     v
call_rcu()
```

---

# RCU Memory Reclamation

```c
static void rcu_free_callback(
        struct rcu_head *r)
{
    struct kobjx *k;

    k = container_of(
            r,
            struct kobjx,
            rcu);

    kmem_cache_free(
            cache,
            k);
}
```

Execution sequence:

```text
call_rcu()
      |
      v
Wait For Readers
      |
      v
Callback Invoked
      |
      v
SLAB Free
```

---

# Shrinker Integration

The kernel invokes shrinkers during memory pressure.

```c
static unsigned long scan_objects(
        struct shrinker *s,
        struct shrink_control *sc)
{
    ...
}
```

Purpose:

```text
Memory Pressure
        |
        v
Kernel Calls Shrinker
        |
        v
Old Objects Removed
        |
        v
RCU Reclaims Safely
```

This prevents registry growth from causing OOM situations.

---

# ProcFS Monitoring

Statistics are exposed through:

```text
/proc/kobjx_stats
```

Implementation:

```c
proc_create(
    "kobjx_stats",
    0444,
    NULL,
    &proc_fops);
```

View:

```bash
cat /proc/kobjx_stats
```

Example output:

```text
KOBJX SLAB RCU SYSTEM
Active Registry Nodes: 0
Total Allocations: 2
Total Frees: 2
```

---

# Build Instructions

## Build Module

```bash
make
```

What it does:

```text
Invokes Kbuild
      |
      v
Compiles Source
      |
      v
Produces .ko Module
```

Output:

```text
kobjx_slab_rcu_registry.ko
```

---

# Module Signing

Required on Secure Boot systems.

```bash
sudo /usr/src/linux-headers-$(uname -r)/scripts/sign-file \
sha256 \
~/kernel_keys/MOK.key \
~/kernel_keys/MOK.crt \
kobjx_slab_rcu_registry.ko
```

Explanation:

| Component | Purpose                |
| --------- | ---------------------- |
| sign-file | Kernel signing utility |
| sha256    | Hash algorithm         |
| MOK.key   | Private key            |
| MOK.crt   | Certificate            |
| .ko       | Module being signed    |

---

# Load Module

```bash
sudo insmod kobjx_slab_rcu_registry.ko
```

Explanation:

```text
insmod
   |
   v
Loads Kernel Module
   |
   v
Executes kobjx_init()
```

---

# Monitor Kernel Logs

```bash
dmesg -w
```

Explanation:

```text
dmesg
  |
  v
Kernel Ring Buffer

-w
  |
  v
Follow Output Live
```

Expected messages:

```text
[KOBJX] loading...
[KOBJX] alloc id=1 name=Object_A
[KOBJX] alloc id=2 name=Object_B
[KOBJX] lookup found item: Object_A
[KOBJX] Memory safely reclaimed via RCU callback
```

---

# View Statistics

```bash
cat /proc/kobjx_stats
```

Shows:

* Active objects
* Allocations
* Frees

---

# Remove Module

```bash
sudo rmmod kobjx_slab_rcu_registry
```

Explanation:

```text
rmmod
   |
   v
Calls kobjx_exit()
   |
   v
Unregister Shrinker
   |
   v
Wait For RCU Callbacks
   |
   v
Destroy SLAB Cache
```

---

# Production Concepts Demonstrated

This single module teaches:

* Kernel Memory Allocation
* SLAB Internals
* RCU Synchronization
* Lockless Readers
* Hash Tables
* Linked Lists
* Refcounting
* ProcFS
* Per-CPU Variables
* Shrinkers
* Safe Memory Reclamation
* Use-After-Free Prevention

---

# Real-World Kernel Examples

The same architectural pattern appears in:

* Netfilter Connection Tracking (nf_conntrack)
* VFS Dentry Cache
* Routing Tables
* Cgroup Registries
* IPC Registries
* Namespace Tracking
* Container Infrastructure

---

# Key Learning Outcome

The most important lesson from this project is:

```text
Visibility Lifetime
        !=
Memory Lifetime
```

RCU controls visibility.

Reference counting controls lifetime.

Combining both creates a high-performance, production-grade kernel registry capable of serving lockless readers while maintaining memory safety.

