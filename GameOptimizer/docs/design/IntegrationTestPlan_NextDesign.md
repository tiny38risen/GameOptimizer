# GameOptimizer Integration Test Plan - Next Design Pass

## IT-ND-1 Background log compact mode
Run apply mode without `--background-detail-log`.
Expected:
- Protected/user-protected per-process skip lines are not printed.
- `background restriction summary` is printed.
- Real apply failures remain visible.

## IT-ND-2 Background detail mode
Run apply mode with `--background-detail-log`.
Expected:
- Protected/user-protected skip lines are printed.
- Restricted success lines are printed.
- Summary is still printed exactly once per BG_RESTRICT_UP application.

## IT-ND-3 Policy feedback rearm cooldown
Force RTT jitter above trigger long enough to emit BG_RESTRICT_UP, then keep jitter above trigger after the feedback window ends.
Expected:
- First command arms feedback tracker.
- Same command rearm is suppressed during `rearmCooldown`.
- Log contains `policy feedback tracker suppressed rearm by cooldown`.

## IT-ND-4 ThreadTracker display path
Run a target with more than 8 active threads.
Expected:
- `observed top thread count` is at most 8.
- Threads are ordered by EMA descending.
- No allocation/sort path is used in getTopThreads().

## IT-ND-5 Shutdown rollback
Trigger BG_RESTRICT_UP, then exit with Ctrl+C.
Expected:
- Main thread rollback audit passes.
- Stale background identities are non-fatal.
- Alive same-identity rollback failures remain fatal.
