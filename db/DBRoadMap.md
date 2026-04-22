Below is a development roadmap for a **PostgreSQL ORM on top of libpqxx** with a **connection pool**, **transaction system**, and the minimum set of features needed for a production-grade first release.

A few hard constraints from the underlying stack matter to the design:

* In `libpqxx`, SQL execution happens through a **transaction object**, not directly on the connection. A connection can have only **one main transaction** active at a time. Prepared statements are defined on the **connection**, and they survive transaction boundaries until the connection is closed or the statement is explicitly unprepared. `libpqxx` also advises treating a connection and related objects as a single “world” that should not be concurrently mutated from multiple threads. ([libpqxx.readthedocs.io][1])
* On the PostgreSQL side, **savepoints** are the right primitive for nested rollback semantics, and transaction isolation behavior should be designed explicitly. PostgreSQL’s default isolation level is **Read Committed**, and PostgreSQL implements three distinct internal isolation behaviors because `Read Uncommitted` behaves like `Read Committed`. ([PostgreSQL][2])

## 1. Project goal

Build a C++ persistence toolkit with these layers:

* low-level DB runtime over `libpqxx`
* thread-safe connection pool
* explicit transaction API with savepoints
* typed row/object mapping
* CRUD and query API
* optional higher-level ORM features like identity map and unit of work

The most important architectural choice is this:

**Do not start by building “magic ORM behavior.”**
Start by building a **correct database runtime** and then layer mapping and ORM features on top.

## 2. Recommended scope for v1

Keep v1 intentionally narrow.

### Must have

* PostgreSQL only
* `libpqxx` only
* synchronous execution model
* thread-safe connection pool
* explicit transactions
* savepoints
* typed mapping for common PostgreSQL types
* CRUD for simple entities
* query builder for common cases
* structured errors
* logging/metrics hooks
* integration tests against real PostgreSQL

### Should not be in v1

* transparent lazy loading
* full LINQ-style SQL DSL
* cross-database portability
* distributed transactions
* automatic schema migration DSL
* runtime proxy magic
* lock-free connection pool

## 3. Target architecture

```text
Application / Service layer
    ↓
Repository / Query API
    ↓
ORM Core
    ↓
Unit of Work (optional in v1.5/v2)
    ↓
Transaction Layer
    ↓
Connection Pool
    ↓
libpqxx
    ↓
PostgreSQL
```

## 4. Core modules to build

### A. `db_core`

Responsibilities:

* own `pqxx::connection`
* register prepared statements
* execute SQL/prepared SQL
* convert `pqxx::result` into internal result abstractions
* translate `pqxx`/Postgres exceptions into your error taxonomy

### B. `pool`

Responsibilities:

* bounded pool
* checkout/checkin
* timeout on acquire
* broken connection eviction
* idle/max lifetime policies
* graceful shutdown

### C. `tx`

Responsibilities:

* transaction lifecycle
* commit/rollback
* savepoints
* isolation level selection
* retry helpers for serialization failures

### D. `mapping`

Responsibilities:

* row-to-object conversion
* object-to-parameter conversion
* metadata for entities
* converters for optional/enum/time/uuid/jsonb

### E. `query`

Responsibilities:

* safe query construction
* filters/order/pagination
* join support
* projections
* typed parameters

### F. `orm`

Responsibilities:

* CRUD
* optimistic locking
* relationship loading
* optional change tracking later

### G. `obs`

Responsibilities:

* structured logging hooks
* metrics
* tracing hooks
* slow query reporting
* pool/tx counters

## 5. Essential design decisions before coding

You should write these down before implementation.

### API style

Use:

* RAII
* move-only connection/transaction handles
* explicit transaction boundaries
* exceptions or `expected<T, E>` consistently, not both

My recommendation:

* use **exceptions internally and in low-level API**
* optionally expose `expected` wrappers later if needed

### Threading model

Use this rule:

* pool is thread-safe
* a checked-out connection is owned by one thread at a time
* a transaction is bound to one checked-out connection
* ORM session / unit-of-work is thread-confined

That aligns with `libpqxx`’s “connection world” guidance. ([libpqxx.readthedocs.io][3])

### Mapping model

Use **Data Mapper**, not Active Record.

Why:

* clearer separation
* easier transaction boundaries
* less hidden DB access
* easier testing

### Query model

Support both:

* raw SQL + typed mapping
* typed builder for common CRUD/query cases

Do not try to model all of SQL in v1.

## 6. Development roadmap

## Phase 0 — Specification and project skeleton

**Duration:** 1 week

### Goals

* freeze scope
* define non-goals
* define public API shape
* create repo skeleton and CI

### Deliverables

* architecture RFC
* module boundaries
* coding standard
* exception/error taxonomy draft
* CI with:

  * formatting
  * clang-tidy
  * ASan/UBSan/TSan jobs where possible
  * PostgreSQL integration test container

### Exit criteria

* no ambiguity about v1 scope
* repository and CI ready

---

## Phase 1 — Low-level DB runtime

**Duration:** 2 weeks

### Goals

Build the thinnest correct abstraction over `libpqxx`.

### Tasks

* `Connection` wrapper
* execution helpers:

  * `exec(sql)`
  * `exec_params(sql, params...)`
  * `exec_prepared(name, params...)`
* result wrapper
* typed field extraction
* error translation

### Required support

* `int32/int64`
* `double`
* `bool`
* `std::string`
* `std::string_view` as input only
* `std::optional<T>`
* `std::chrono` timestamps
* UUID
* JSONB as string or JSON adapter

### Deliverables

* `db_core` module
* unit tests for conversions and errors
* integration tests for real query execution

### Exit criteria

* raw SQL path is stable
* error boundaries are known
* no leaks under sanitizer runs

---

## Phase 2 — Connection pool

**Duration:** 2 weeks

### Goals

Implement a reliable pool before touching high-level ORM behavior.

### Tasks

* pool config:

  * min size
  * max size
  * acquire timeout
  * idle timeout
  * max lifetime
* blocking acquire
* timed acquire
* broken connection replacement
* health check strategy
* pool shutdown/drain mode

### Implementation advice

Use:

* `std::mutex`
* `std::condition_variable`
* clear ownership semantics

Do not use lock-free structures here unless profiling proves need.

### Observability

Expose:

* active connections
* idle connections
* waiting threads
* acquire timeout count
* connection creation count
* broken connection count

### Deliverables

* `ConnectionPool`
* contention tests
* reconnect tests
* shutdown tests

### Exit criteria

* pool survives contention
* no deadlocks
* no leaked checkouts
* broken connections are evicted cleanly

---

## Phase 3 — Transaction layer

**Duration:** 2 weeks

### Goals

Make transaction semantics explicit and correct.

### Tasks

* `Transaction` abstraction
* auto-rollback on destructor
* explicit `commit()` / `rollback()`
* `Savepoint` abstraction
* isolation level enum
* retry helper for serialization errors

### Important design rules

* one transaction uses exactly one checked-out connection
* transaction cannot hop between connections
* failed transaction becomes poisoned until rollback
* commit failure semantics must be explicit

### PostgreSQL semantics to encode

* default isolation is `Read Committed`
* support at least:

  * read committed
  * repeatable read
  * serializable
* nested rollback behavior should use **savepoints**. ([PostgreSQL][4])

### Deliverables

* `Transaction`, `Savepoint`
* isolation tests
* rollback/commit tests
* concurrent conflict tests

### Exit criteria

* transaction behavior is deterministic
* nested rollback works
* retryable vs non-retryable failures are distinguishable

---

## Phase 4 — Metadata and type mapping

**Duration:** 2 weeks

### Goals

Create the foundation for ORM behavior.

### Tasks

* entity metadata registry
* column metadata
* key metadata
* converters
* `from_row<T>()`
* `to_params(T)` or field-based binders

### Recommended metadata style

Prefer a registration DSL like:

```cpp
entity<User>("users")
  .key("id", &User::id)
  .column("email", &User::email)
  .column("created_at", &User::created_at)
  .column("version", &User::version);
```

This is better for maintainability than scattered specializations everywhere.

### Deliverables

* metadata API
* type conversion registry
* row mapping tests

### Exit criteria

* simple entities map in both directions
* nullability and type conversion are predictable

---

## Phase 5 — CRUD mapper

**Duration:** 2 weeks

### Goals

Deliver first actual ORM value.

### Tasks

* `insert`
* `update`
* `delete`
* `find_by_id`
* `exists`
* batch insert basic version
* optimistic locking by `version` column
* support generated IDs

### Must support

* single-column primary key first
* composite keys can be v1.1 unless mandatory
* partial update strategy
* `ON CONFLICT` helper for selected cases

### Deliverables

* CRUD API
* example repository implementation
* optimistic locking tests

### Exit criteria

* common entities can be persisted and loaded safely
* stale updates are detectable

---

## Phase 6 — Query builder

**Duration:** 3 weeks

### Goals

Provide ergonomic queries without turning the project into a SQL compiler.

### Scope

Support:

* `select`
* `where`
* `and/or`
* `order_by`
* `limit/offset`
* `join`
* `count`
* projections into DTOs

### Example target API

```cpp
auto users = orm.select<User>()
    .where(eq(&User::tenant_id, tenant_id))
    .where(gt(&User::age, 18))
    .order_by(desc(&User::created_at))
    .limit(100)
    .fetch(tx);
```

### Hard requirements

* parameterized SQL only
* never interpolate user values into SQL text
* deterministic aliasing
* inspectable generated SQL

### Deliverables

* query AST or builder
* SQL generator
* parameter binder
* query builder tests

### Exit criteria

* 80% of normal service queries do not require raw SQL
* raw SQL remains available for advanced cases

---

## Phase 7 — Relationships and loading strategies

**Duration:** 2 weeks

### Goals

Support associations without introducing hidden performance disasters.

### Support first

* many-to-one
* one-to-many
* one-to-one
* explicit eager loading

### Avoid in v1

* transparent lazy loading
* implicit DB hits from property access

### Safer strategy

Explicit include/load operations:

```cpp
auto orders = repo.find_orders_with_items(customer_id, tx);
```

or

```cpp
orm.select<Order>()
   .include(&Order::items)
   .fetch(tx);
```

### Deliverables

* association metadata
* graph materialization
* eager loading tests

### Exit criteria

* graph loading is predictable
* no accidental N+1 behavior in default flows

---

## Phase 8 — Production hardening

**Duration:** 2–3 weeks

### Goals

Make the library deployable, not just functional.

### Tasks

* structured logs
* metrics hooks
* tracing hooks
* slow query threshold
* pool wait-time stats
* transaction duration stats
* benchmark suite
* graceful shutdown
* fault injection tests
* prepared statement cache policy per connection

### Important libpqxx-specific detail

Prepared statements live on the **connection**, not the transaction, so your prepared-statement cache/registry needs to be connection-aware. ([libpqxx.readthedocs.io][5])

### Deliverables

* metrics/tracing adapters
* benchmark results
* operational docs
* failure mode test report

### Exit criteria

* stable under load
* observable in production
* known behavior under DB restarts and pool exhaustion

---

## Phase 9 — Optional v1.5 / v2 features

Add these only after the above is stable.

### Candidates

* Unit of Work
* identity map
* dirty tracking
* schema validation at startup
* migration integration
* coroutine facade
* bulk operations API
* row-level lock helpers
* repository code generation

## 7. Suggested timeline

A realistic solo/small-team plan:

* Phase 0: 1 week
* Phase 1: 2 weeks
* Phase 2: 2 weeks
* Phase 3: 2 weeks
* Phase 4: 2 weeks
* Phase 5: 2 weeks
* Phase 6: 3 weeks
* Phase 7: 2 weeks
* Phase 8: 2–3 weeks

**Total:** about **18–19 weeks** for a solid v1.

If you try to compress this much further, what usually suffers is:

* transaction correctness
* pool behavior under contention
* integration testing
* observability

## 8. Essential backlog by workstream

## Workstream A — Infrastructure

* repo setup
* CMake targets
* packaging
* CI/CD
* sanitizer jobs
* dependency management
* Docker Postgres test env

## Workstream B — API design

* public headers
* namespace layout
* move/copy policy
* exceptions and error classes
* naming conventions

## Workstream C — Runtime correctness

* connection lifecycle
* pool lifecycle
* tx lifecycle
* savepoint lifecycle
* shutdown semantics

## Workstream D — Type system

* nullable support
* enums
* UUID
* timestamps/time zones
* JSONB
* arrays
* custom converters

## Workstream E — ORM features

* metadata
* CRUD
* query builder
* relationship loading
* optimistic locking

## Workstream F — Reliability

* timeout policies
* retry helpers
* connection health
* broken connection replacement
* commit ambiguity handling
* failure classification

## Workstream G — Observability

* logs
* metrics
* tracing
* slow query capture
* pool stats
* tx stats

## Workstream H — Documentation

* getting started
* transaction semantics
* pool semantics
* mapping guide
* query guide
* production tuning guide

## 9. Error taxonomy you should define early

At minimum:

* `connection_error`
* `pool_exhausted`
* `pool_timeout`
* `query_error`
* `constraint_violation`
* `serialization_failure`
* `deadlock_detected`
* `transaction_aborted`
* `mapping_error`
* `configuration_error`

Do not leak raw library exceptions throughout the whole public API. Translate them close to the DB boundary.

## 10. Testing strategy

## Unit tests

* metadata registration
* SQL generation
* parameter binding
* field conversion
* error mapping
* state transitions

## Integration tests

Against real PostgreSQL:

* connect/disconnect
* pool under concurrency
* savepoints
* isolation behavior
* deadlocks
* serialization conflicts
* prepared statements
* reconnect after DB restart

## Stress tests

* many threads fighting for a small pool
* long transactions
* frequent connection invalidation
* repeated startup/shutdown

## Fault injection

* kill DB during active tx
* break network during commit
* force bad credentials
* exhaust pool
* statement timeout

## 11. Definition of done for v1

Do not call it production-ready until all of these are true:

* pool is stable under contention
* no connection leaks under sanitizers
* transaction rollback/commit semantics are deterministic
* savepoints work
* CRUD works for real entities
* query builder covers the common 80%
* prepared statement lifecycle is handled correctly per connection
* integration test suite passes reliably
* metrics/log hooks exist
* benchmark numbers are collected and documented

## 12. Recommended repository layout

```text
/include/orm
  connection.hpp
  pool.hpp
  transaction.hpp
  savepoint.hpp
  result.hpp
  error.hpp
  entity.hpp
  mapper.hpp
  query.hpp
  repository.hpp
  observability.hpp

/src
  connection.cpp
  pool.cpp
  transaction.cpp
  savepoint.cpp
  mapper.cpp
  query.cpp
  repository.cpp
  error.cpp

/tests
  /unit
  /integration
  /stress
  /fault

/examples
  basic_crud
  transactions
  query_builder
  relations

/docs
  architecture.md
  transactions.md
  pooling.md
  mapping.md
  query_builder.md
```

## 13. Recommended MVP API surface

Keep MVP small and sharp:

* `ConnectionPool`
* `Transaction`
* `Savepoint`
* `EntityMetadata`
* `insert<T>()`
* `update<T>()`
* `remove<T>()`
* `find_by_id<T>()`
* `select<T>()`
* `exec_sql<T>()`
* `from_row<T>()`

That is enough to build real services without overcommitting to hard-to-change abstractions too early.

## 14. Final recommendation

Position the project as:

**“PostgreSQL persistence toolkit with ORM capabilities”**

not as:

**“full ORM from day one.”**

That framing will keep you honest about:

* explicit transactions
* predictable SQL
* performance visibility
* lower coupling
* production behavior

If you want, I can turn this into a **GitHub Projects backlog with epics/tasks**, or into a **technical design document with class diagrams and interfaces**.

[1]: https://libpqxx.readthedocs.io/stable/group__transactions.html "libpqxx: Transaction classes"
[2]: https://www.postgresql.org/docs/current/tutorial-transactions.html "PostgreSQL: Documentation: 18: 3.4. Transactions"
[3]: https://libpqxx.readthedocs.io/7.10.3/thread-safety.html "libpqxx: Thread safety"
[4]: https://www.postgresql.org/docs/current/transaction-iso.html "PostgreSQL: Documentation: 18: 13.2. Transaction Isolation"
[5]: https://libpqxx.readthedocs.io/stable/classpqxx_1_1connection.html "libpqxx: pqxx::connection Class Reference"
