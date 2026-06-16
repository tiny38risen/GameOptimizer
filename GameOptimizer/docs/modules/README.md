# Module Design Specifications

## 목적

`docs/modules/`는 GameOptimizer v3.1의 모듈별 상세 설계 문서(Module Design Specification, MDS)를 두는 위치다.

Module Design 문서는 구현 직전 계약이다. 각 문서는 모듈의 책임, 입력, 출력, 상태, 소유권, 금지사항, 실패 처리, 테스트 기준을 정의해야 한다.

## 작성 규칙

1. 문서 경로는 `docs/...` 기준으로 쓴다.
2. 관련 PVD, Performance Engine Specification, Policy Specification, Safety Contract를 먼저 확인한다.
3. 모듈 소유권과 금지사항은 `docs/architecture/Module_Ownership_Matrix.md`와 충돌하면 안 된다.
4. Runtime mutation을 다루는 모듈은 `docs/contracts/Safety_Contract.md`의 ApplyGuard/Rollback 계약을 따라야 한다.
5. observation-only 모듈은 Win32 제어 API와 RollbackManager를 직접 호출하지 않는다.

## Active Documents

- `docs/modules/MDS-001_ThreadTracker.md`
- `docs/modules/MDS-002_MainThreadConfidenceAnalyzer.md`
- `docs/modules/MDS-003_TopologyAnalyzer.md`
- `docs/modules/MDS-004_SchedulerController.md`
- `docs/modules/MDS-005_RollbackManager.md`
- `docs/modules/MDS-006_BackgroundController.md`
- `docs/modules/MDS-007_PerformanceEvidencePlanner.md`
- `docs/modules/MDS-008_PolicyDispatcher.md`
- `docs/modules/MDS-009_RuntimeOrchestrator.md`

## 문서 상태

현재 MDS-001부터 MDS-009까지 v1.0 문서가 생성되어 Active 상태다. 신규 MDS는 필요할 때만 생성하며, 빈 MDS 파일은 만들지 않는다.
