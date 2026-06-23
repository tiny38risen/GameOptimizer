# GameOptimizer Project Spec

## 1. 프로젝트 목표

GameOptimizer는 Windows 게임 프로세스의 스레드 동작, 토폴로지, 지연 변동을 관찰하고 안전 조건을 만족할 때만 제한적 최적화 정책을 적용하는 도구다.
핵심 목표는 성능 향상 보장이 아니라 안전한 적용, 롤백 가능성, 검증 가능한 Evidence 확보다.

## 2. 해결하려는 문제

| 문제 | 접근 |
|---|---|
| 게임의 메인 스레드 후보가 명확하지 않음 | Thread observation과 candidate tracking으로 추정 |
| CPU topology와 Processor Group 차이 | TopologyAnalyzer가 group-aware 정보를 제공 |
| 적용 실패 시 원복 위험 | ApplyGuard와 RollbackManager로 transaction 처리 |
| 성능 개선 주장 근거 부족 | Evidence 기반 PASS/WARN/BLOCKER 분류 |
| Anti-Cheat / Access Denied 경계 | public user-mode Win32 API 안에서 fallback 처리 |

## 3. 대상 환경

| 항목 | 기준 |
|---|---|
| OS | Windows 10 / 11 x64 |
| 언어 | C++23 |
| API | Win32 API |
| Toolchain | MSVC v143 / Visual Studio 2022 |
| 표준 옵션 | `/std:c++latest` |
| UI | 별도 UI layer, mutation owner 아님 |

## 4. 핵심 기능

### Core

- Thread observation
- Main thread candidate tracking
- Processor topology awareness
- SchedulerController-owned affinity / priority policy
- ApplyGuard transaction
- RollbackManager state preservation
- Runtime validation
- Evidence-based result classification

### Optional

- Background process restriction
- Timer resolution control
- Network latency monitoring
- Real-game validation

### Experimental / Deferred

- Aggressive Single-Core Mode
- Self-Tuning
- GameProfile expansion
- Advanced PerformanceEngine planner

## 5. 명시적 제외 사항

- 성능 향상 보장
- DLL injection
- kernel driver
- anti-cheat bypass
- game memory patch
- game process 내부 socket 강제 변경
- Evidence 없는 PASS 또는 성능 개선 주장
- UI의 직접 runtime mutation

## 6. 현재 상태

| 영역 | 상태 |
|---|---|
| Runtime safety 계약 | Reference 문서로 보존 |
| Thread observation / SchedulerController / RollbackManager | 구현 기반 존재 |
| Evidence schema | `docs/release/Evidence_Schema.md` 참고 |
| v3.1 성능 확장 | Proposal |
| canonical SRS Markdown | 없음 |
| UI 기준 | `docs/UI_SPEC.md`가 현재 요약 기준 |

## 7. 다음 작업

- canonical SRS Markdown 생성 여부 결정
- Proposal 문서의 구현 승격 범위 결정
- `Generated` 문서 자동 생성 경로 검증
- root `docs/architecture` 중복 문서 정리
- 실제 게임 검증 Evidence 보강
