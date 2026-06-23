# GameOptimizer Architecture and Safety

## 1. Runtime Flow

```text
Observe
→ Analyze
→ Decide
→ Save
→ Arm
→ Apply
→ Verify
→ Commit or Rollback
→ Evidence
```

| 단계 | 의미 |
|---|---|
| Observe | mutation 없이 thread, process, topology, latency 정보를 수집 |
| Analyze | 후보, 신뢰도, 권한, 위험을 계산 |
| Decide | 적용 가능 policy 또는 monitoring-only 결정을 생성 |
| Save | mutation 전 원래 상태를 RollbackManager에 저장 |
| Arm | ApplyGuard를 transaction 경계로 활성화 |
| Apply | 정해진 mutation owner만 Win32 변경 수행 |
| Verify | 적용 후 상태와 대상 identity 재검증 |
| Commit or Rollback | 성공 시 commit, 실패 시 rollback 또는 failed state 보존 |
| Evidence | PASS/WARN/BLOCKER 근거 기록 |

## 2. Module Responsibility Map

| Module | Role | Must Not |
|---|---|---|
| ThreadTracker | 스레드 관측, CPU delta, 후보 추적 | affinity/priority 변경, rollback 소유 |
| MainThreadConfidenceAnalyzer | 메인 스레드 후보 신뢰도 계산 | mutation 실행 |
| TopologyAnalyzer | CPU topology와 Processor Group 정보 제공 | group 0 조용한 보정 |
| SchedulerController | thread affinity / priority mutation owner | 관측 전용 책임, UI 직접 호출 대상 |
| BackgroundController | background process restriction owner | ThreadTracker 책임 침범 |
| RollbackManager | 원래 상태 저장과 원복 | policy 결정 |
| ApplyGuard | save 이후 apply/verify/rollback transaction 정리 | rollback state 생성 |
| RuntimeOrchestrator | runtime lifecycle과 shutdown 흐름 | controller 우회 mutation |
| RuntimeValidationMonitor | runtime validation summary 기록 | policy 생성, rollback 실행 |
| Release evidence scripts | RC evidence 생성과 검증 | production runtime mutation |
| UI layer | 실행 모드 선택과 상태 표시 | runtime mutation 소유 |

## 3. Mutation Ownership

| Mutation | Owner | Required Boundary |
|---|---|---|
| Thread affinity | SchedulerController | Save → Arm → Apply → Verify |
| Thread priority | SchedulerController | Rollback state + ApplyGuard |
| Process background restriction | BackgroundController | explicit filter + rollback state |
| Rollback | RollbackManager | identity 재검증 |
| UI action | RuntimeOrchestrator 또는 공개 application-level API | controller 직접 호출 금지 |

PerformanceEngine, PolicySpecification, GameProfile 확장 문서는 Proposal이다.
이 문서들이 승격되기 전까지 현재 runtime owner로 간주하지 않는다.

## 4. Safety Rules

- ThreadTracker는 관측 전용이다.
- SchedulerController가 Affinity / Priority 변경을 소유한다.
- Runtime mutation은 Save → Arm → Apply → Verify → Commit or Rollback 순서를 따른다.
- `std::expected` 결과는 사용 전에 반드시 검사한다.
- Target process/thread identity는 apply와 rollback 전에 재검증한다.
- Evidence 없이 PASS 또는 성능 개선을 주장하지 않는다.
- DLL injection, kernel driver, game memory patch, anti-cheat bypass는 금지한다.
- UI는 직접 runtime mutation을 소유하지 않는다.

## 5. Rollback Rules

| 규칙 | Severity |
|---|---|
| mutation 전 rollback state 저장 | BLOCKER |
| ApplyGuard 우회 금지 | BLOCKER |
| verify 전 commit 금지 | BLOCKER |
| rollback 실패 은폐 금지 | BLOCKER |
| failed rollback state 폐기 금지 | BLOCKER |
| shutdown retry를 위한 상태 보존 | BLOCKER |

Rollback 실패는 WARN/INFO로 낮출 수 없다.

## 6. Processor Group Rules

- Processor Group은 명시적으로 저장하고 전달한다.
- group-aware 경로에서 조용한 group 0 fallback은 금지한다.
- thread-level group affinity는 SchedulerController가 소유한다.
- group 1+ process-wide background restriction은 안전한 backend가 없으면 monitoring-only다.
- unsupported topology를 성공처럼 기록하면 BLOCKER다.

## 7. Shutdown Rules

- ShutdownRequested 이후 신규 runtime mutation은 금지한다.
- 진행 중 transaction은 commit, rollback, failed state preservation 중 하나로 닫는다.
- shutdown reason과 rollback summary는 Evidence에 남긴다.
- preserved rollback state가 shutdown 이후 남으면 release 판단에서 BLOCKER다.

## 8. Current / Proposal Module Split

| 분류 | 문서 / 모듈 | 의미 |
|---|---|---|
| Current | ThreadTracker, SchedulerController, RollbackManager, ApplyGuard | 현재 안전 경계의 핵심 |
| Current / Partial | TopologyAnalyzer, BackgroundController, RuntimeValidationMonitor | 구현 기반이 있으나 제한 존재 |
| Reference | Safety_Contract, Runtime_Contract, SAD, Module Ownership | 상세 배경과 계약 보존 |
| Proposal | PerformanceEngine, PolicySpecification, GameProfile expansion | 구현 완료 전 current behavior 아님 |
| Archived | PatchPlan Phase 문서, history 문서 | 과거 기록 |
