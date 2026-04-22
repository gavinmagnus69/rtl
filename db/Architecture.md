# DB Module Architecture

## Purpose

The `db` module is a PostgreSQL persistence toolkit built on top of `libpqxx`.
For v1, the priority is runtime correctness:

`db_core -> pool -> tx -> mapping -> crud -> query`

Higher-level ORM behavior is layered on top of a correct connection, pool, and
transaction runtime. We do not start with "magic ORM" features.

## V1 Scope

### In scope

- PostgreSQL only
- `libpqxx` only
- synchronous API
- explicit transactions
- savepoints
- typed mapping for common PostgreSQL types
- CRUD for simple entities
- query builder for common cases
- structured errors
- logging and metrics hooks
- integration tests against real PostgreSQL

### Out of scope

- cross-database portability
- transparent lazy loading
- distributed transactions
- automatic migration DSL
- runtime proxy magic
- lock-free connection pool
- unit of work and identity map in v1

## Architectural Order

The implementation order is fixed:

1. `db_core`
2. `pool`
3. `tx`
4. `mapping`
5. `orm` CRUD
6. `query`
7. `obs`

The first production milestone is a stable raw SQL path with typed result
mapping, explicit transactions, and a reliable connection pool. ORM features
must not be built before the runtime layers are tested against a real database.

## API Style

- RAII for owned resources
- move-only handles for connection and transaction objects
- explicit `commit()` and `rollback()`
- no hidden database access from entity objects
- exceptions used consistently in the low-level API

The mapping style is Data Mapper, not Active Record. Entities remain plain data
types and do not own persistence behavior.

## Threading Model

- `ConnectionPool` is thread-safe
- a checked-out connection is owned by one thread at a time
- a transaction owns exactly one checked-out connection
- a transaction never hops between connections
- ORM session or unit-of-work concepts are thread-confined

This matches the practical `libpqxx` constraint that a connection and its
related objects form one mutable "world" and must not be shared for concurrent
mutation.

## Error Strategy

Raw `pqxx` and PostgreSQL exceptions must be translated close to the DB
boundary. The public DB API should expose domain-specific DB errors instead of
leaking vendor exception types through the full stack.

Initial error taxonomy:

- `connection_error`
- `pool_exhausted`
- `pool_timeout`
- `query_error`
- `constraint_violation`
- `serialization_failure`
- `deadlock_detected`
- `transaction_aborted`
- `mapping_error`
- `configuration_error`

## Initial Public Surface

The first public API should stay small:

- `ConnectionConfig`
- `Connection`
- `ConnectionPool`
- `Transaction`
- `Savepoint`
- `Result`
- `Row`
- `Field`
- raw execution helpers:
  - `exec(sql)`
  - `exec_params(sql, params...)`
  - `exec_prepared(name, params...)`
- typed mapping helpers:
  - `from_row<T>()`
  - binder support for parameter conversion

Prepared statements are connection-scoped, so any prepared statement registry or
cache must also be connection-scoped.

## Planned Module Layout

The repository layout should follow the conventions already used in this repo:
small module directories with their own `CMakeLists.txt`, public headers in the
module directory, and tests split by purpose.

Planned structure under `src/libs/db`:

- `core`
  - connection wrapper
  - result/row/field abstractions
  - execution helpers
  - error translation
- `pool`
  - pool config
  - checkout/checkin handles
  - health and lifetime policies
- `tx`
  - transaction lifecycle
  - savepoints
  - isolation levels
  - retry helpers
- `mapping`
  - entity metadata
  - converters
  - row-to-object and object-to-params mapping
- `query`
  - query AST or builder
  - SQL generation
  - parameter binding
- `orm`
  - CRUD operations
  - optimistic locking
  - repository-oriented helpers
- `obs`
  - logging hooks
  - metrics hooks
  - tracing hooks
- `test/unit`
  - conversion, metadata, SQL generation, state transitions
- `test/integration`
  - PostgreSQL-backed runtime and transaction tests
- `test/stress`
  - contention and repeated startup/shutdown tests
- `test/fault`
  - failure injection and recovery tests

## Current Starting Point

The current `db` module is still a stub:

- `src/libs/db/orm/Drivers.hpp` contains only a placeholder result-row type
- `src/libs/db/orm/CMakeLists.txt` is empty
- `src/libs/db/CMakeLists.txt` only adds the test target
- `src/libs/db/test/main.cpp` is a direct `libpqxx` connection smoke test

That means Phase 0 should focus on finalizing structure and contracts before any
runtime implementation begins.
