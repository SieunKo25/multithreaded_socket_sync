# Multithreaded Synchronization & Socket Communication System
*(A Linux-based concurrency and synchronization demo using POSIX threads)*<br/><br/>

This project demonstrates a complete multithreaded synchronization system in C using POSIX threads on Linux.
It implements three different queue architectures, multiple producer–consumer pipelines, and a multithreaded TCP client–server model.
The system highlights key operating system concepts including:

- Thread creation & lifecycle management
- Mutex, semaphore, and condition variable synchronization
- Race-free producer–consumer pipelines
- Thread-safe bounded queues
- Inter-thread communication
- Socket-based concurrency

This repository serves as a practical reference for **multithreading, synchronization primitives, and system-level programming,** suitable for systems software engineering interviews and lab environments.


## Synchronization Documentation
Multiple processing pipelines demonstrate thread coordination and data flow:





This document explains the use of mutex, semaphore, and queue implementations in the multithreading project.

### Table of Contents
1. [Queue Implementations](###queue-implementations)
2. [Synchronization Mechanisms](#synchronization-mechanisms)
3. [Usage Patterns](#usage-patterns)

---

### Queue Implementations

#### 1. frameq (Frame Queue)
**File**: `common/frameq.c`, `common/frameq.h`

**Purpose**: Thread-safe bounded queue for frame pointers used in encoding/decoding pipeline.

**Capacity**: 536 items (FRAMEQ_CAP)

**Data Structure**:
```c
typedef struct {
    void* buf[FRAMEQ_CAP];      // Circular buffer of frame pointers
    int head;                   // Next pop index  
    int tail;                   // Next push index
    int count;                  // Current number of items 
    
    pthread_mutex_t mtx;        // Protects queue operations
    sem_t items;                // Counts available items (producer signals)
    sem_t slots;                // Counts available slots (consumer signals)
} frameq_t;
```

**Synchronization Strategy**: Semaphores for counting + Mutex for critical section

**Key Operations**:
- `frameq_push()`: Wait for slot → Lock → Insert → Unlock → Signal item
- `frameq_pop()`: Wait for item → Lock → Remove → Unlock → Signal slot

---

#### 2. strq (String Queue)
**File**: `common/strq.c`, `common/strq.h`

**Purpose**: Thread-safe bounded queue for string pointers used in vowel counting pipeline.

**Capacity**: 5 items (STRQ_CAP)

**Data Structure**:
```c
typedef struct {
    char* buf[STRQ_CAP];        // Circular buffer of string pointers
    int head;                   // Next pop index  
    int tail;                   // Next push index
    int count;                  // Current number of items 
    
    pthread_mutex_t mtx;        // Protects queue operations
    pthread_cond_t not_empty;   // Signals when queue has items
    pthread_cond_t not_full;    // Signals when queue has space
} strq_t;
```

**Synchronization Strategy**: Condition Variables + Mutex

**Key Operations**:
- `strq_push()`: Lock → Wait while full → Insert → Signal not_empty → Unlock
- `strq_pop()`: Lock → Wait while empty → Remove → Signal not_full → Unlock

---

#### 3. countq (Count Queue)
**File**: `common/countq.c`, `common/countq.h`

**Purpose**: Thread-safe bounded queue for vowel count requests.

**Capacity**: 16 items (COUNTQ_CAP)

**Data Structure**:
```c
typedef struct {
    int idx;        // Vowel index (0-4 for a,e,i,o,u; -1 for end signal)
    int amount;     // Number of vowels counted
} count_req_t;

typedef struct {
    count_req_t buf[COUNTQ_CAP];    // Circular buffer
    int head;
    int tail;
    int count;
    pthread_mutex_t mtx;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} countq_t;
```

**Synchronization Strategy**: Condition Variables + Mutex (same as strq)

**Key Operations**:
- `countq_push()`: Lock → Wait while full → Insert → Signal not_empty → Unlock
- `countq_pop()`: Lock → Wait while empty → Remove → Signal not_full → Unlock

---

### Synchronization Mechanisms

#### Mutex (pthread_mutex_t)
**Purpose**: Protects critical sections to prevent race conditions on shared data.

**Usage in Queues**:
- Guards access to queue state (head, tail, count)
- Ensures atomic read-modify-write operations
- Must be locked before accessing queue buffers

**Pattern**:
```c
pthread_mutex_lock(&q->mtx);
// Critical section: modify queue state
pthread_mutex_unlock(&q->mtx);
```

---

#### Semaphores (sem_t)
**Used in**: frameq only

**Two Semaphores Pattern** (Producer-Consumer):
1. **`items` semaphore**: 
   - Initial value: 0
   - Counts number of available items to consume
   - Producer calls `sem_post(&items)` after adding item
   - Consumer calls `sem_wait(&items)` before removing item

2. **`slots` semaphore**:
   - Initial value: FRAMEQ_CAP (536)
   - Counts number of available slots to fill
   - Consumer calls `sem_post(&slots)` after removing item
   - Producer calls `sem_wait(&slots)` before adding item

**Why Semaphores for frameq?**
- Large capacity (536) makes semaphores efficient
- Blocks threads outside mutex critical section
- Reduces contention on mutex lock

**Initialization**:
```c
sem_init(&q->items, 0, 0);          // No items initially
sem_init(&q->slots, 0, FRAMEQ_CAP); // All slots available
```

---

#### Condition Variables (pthread_cond_t)
**Used in**: strq, countq

**Two Condition Variables Pattern**:
1. **`not_empty` condition**:
   - Signaled when item is added to queue
   - Consumers wait on this when queue is empty

2. **`not_full` condition**:
   - Signaled when item is removed from queue
   - Producers wait on this when queue is full

**Why Condition Variables for strq/countq?**
- Smaller capacity (5 and 16) doesn't justify semaphore overhead
- Condition variables integrate naturally with mutex
- More flexible for complex waiting conditions

**Pattern**:
```c
pthread_mutex_lock(&q->mtx);
while(q->count == STRQ_CAP) {
    pthread_cond_wait(&q->not_full, &q->mtx);  // Releases mutex, waits, reacquires
}
// Add item
pthread_cond_signal(&q->not_empty);
pthread_mutex_unlock(&q->mtx);
```

---

### Usage Patterns

#### 1. Frame Pipeline (helper service)
**File**: `helper/helper_pipeline.c`

**Encoding Pipeline**:
```
Input → q_char_bin → charToBin → q_parity → addParity → q_parity_enc → Output
```

**Decoding Pipeline**:
```
Input → q_parity_dec → removeParity → q_bin_char → binToChar → q_deframe → Output
```

**Queue Type**: frameq (uses semaphores)
**Threads**: One per stage (createFrame, charToBin, addParity, etc.)

**Coordination**:
- Each stage is a thread that pops from input queue, processes, pushes to output queue
- `NULL` frame signals end of pipeline
- Semaphores block when queues are full/empty

---

#### 2. Vowel Counting Pipeline (server)
**File**: `server/vowel_pipeline.c`

**Pipeline**:
```
Input → qA → count 'a' → qE → count 'e' → qI → count 'i' → qO → count 'o' → qU → count 'u'
                ↓             ↓             ↓             ↓             ↓
              count_q ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ←
```

**Queue Types**:
- **strq** (5 queues: qA, qE, qI, qO, qU) - uses condition variables
- **countq** (1 queue: count_q) - uses condition variables

**Threads**:
- 5 worker threads (one per vowel) - scan string and send counts to count_q
- 1 count thread - accumulates totals from count_q

**Coordination**:
- Worker threads pass strings through pipeline
- Each worker counts its vowel and pushes to count_q
- `NULL` string signals end for next stage
- `{idx: -1}` count request signals end for count thread

---

#### 3. Helper Service (multithreaded)
**File**: `helper/helper_service.c`

**Pattern**: Thread-per-client

**Synchronization**:
- Each client handled by detached thread
- Threads share helper pipeline queues (frameq)
- Socket I/O serializes requests naturally
- No explicit synchronization between client threads needed

**Thread Creation**:
```c
pthread_t thread;
pthread_create(&thread, NULL, handle_client, (void*)csock);
pthread_detach(thread);  // No need to join, cleans up automatically
```

---

### Design Decisions Summary

| Component | Queue Type | Sync Mechanism | Reason |
|-----------|-----------|----------------|---------|
| Frame Pipeline | frameq | Semaphore + Mutex | Large capacity (536), semaphores efficient for blocking |
| String Pipeline | strq | Condition Variable + Mutex | Small capacity (5), simpler with CV |
| Count Queue | countq | Condition Variable + Mutex | Small capacity (16), message-passing pattern |

**Key Principle**: Bounded queues prevent unbounded memory growth and provide natural backpressure in producer-consumer scenarios.

**Thread Safety**: All queue operations are atomic - no external locking required by callers.

**Termination Protocol**: `NULL` pointers or sentinel values (-1 idx) signal pipeline shutdown gracefully.
