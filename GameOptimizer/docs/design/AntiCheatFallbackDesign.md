# Anti-Cheat Fallback Design

## Goal

Anti-cheat products can block normal Win32 scheduling APIs. GameOptimizer must treat those
boundaries as recoverable limitations when no verified partial mutation has escaped.

## Recoverable Access Limitations

1. `ERROR_ACCESS_DENIED` maps to `ErrorCode::AccessDenied`.
2. `OpenThread` failure and `AccessDenied` keep main-thread scheduling in monitoring-only fallback.
3. `OpenProcess` failure and `AccessDenied` skip that background process without failing the whole run.
4. Rollback open failures caused by access boundaries are logged as WARN and kept non-fatal.

## Non-Recoverable Apply Failures

1. Generic thread/process affinity apply failures remain failures unless they map to `AccessDenied`.
2. Generic thread/process priority apply failures remain failures unless they map to `AccessDenied`.
3. Partial apply paths must still invoke `ApplyGuard::rollbackNow()` before returning.

## Release Gate Contract

The static gate and regression tests verify:

1. Access denied is mapped explicitly.
2. Scheduler and background controllers expose recoverable access predicates.
3. Access-denied fallback logs mention monitoring-only or recoverable access limitation.
4. Anti-cheat fallback regression tests are built and run.
