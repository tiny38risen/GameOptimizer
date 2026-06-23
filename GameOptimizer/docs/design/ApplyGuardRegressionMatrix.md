> Status: NeedsReview  
> 이 문서는 권위, 자동 생성 경로, 또는 현재 기준 여부 확인이 필요합니다. 현재 기준은 `docs/DOCUMENT_REGISTER.md`를 따릅니다.

# ApplyGuard Regression Matrix

## AG-1 Expected Bind Enforcement

Any `std::expected` value access must follow Assign -> Check -> Bind. Direct use such as `std::move(*openedThread)` or passing `*result` as an argument is forbidden.

Validation:

```bat
python run_release_gate_static_checks.py
```

## AG-2 Thread Apply Transaction

SchedulerController must save rollback state, construct `ApplyGuard::forThread`, arm it before WinAPI mutation, rollback on partial failure, and commit only after post-apply verification.

Validation:

```bat
python run_release_gate_static_checks.py
```

## AG-3 Process Apply Transaction

BackgroundController must save rollback state, construct `ApplyGuard::forProcess`, arm it before process mutation, rollback on priority-apply failure, and commit only after successful affinity/priority mutation.

Validation:

```bat
python run_release_gate_static_checks.py
```

## AG-4 Ownership Source of Truth

Rollback state ownership must be represented by `ApplyGuard::RollbackStateOwnership` and derived from `RollbackManager::SaveStateDisposition`. Boolean ownership flags such as `newlyCreatedRollbackState` are forbidden.

## AG-5 TOCTOU Blocker

BackgroundController must not use `hasProcessState()` to decide transaction ownership. The only allowed ownership decision source is the return value from `saveProcessState()`.

## AG-6 Move Assignment Blocker

ApplyGuard move assignment is deleted. An armed transaction guard must not be overwritten because that can blur rollback responsibility after a cleanup failure.

## AG-7 Rollback Failure Transfer

When `rollbackNow()` fails, ApplyGuard logs one BLOCKER-class failure, preserves RollbackManager state, transfers final retry responsibility to ShutdownPipeline/RollbackManager, and suppresses destructor duplicate retry.

## AG-8 Static Gate Limitation

`run_release_gate_static_checks.py` verifies required ApplyGuard primitives, forbidden ownership patterns, and minimum function-scope lexical sequence markers by source-pattern scanning. It does not prove full control-flow ordering. Code review must still confirm the exact sequence: save state -> construct ApplyGuard -> arm -> mutate -> rollback on failure or commit after verification.
