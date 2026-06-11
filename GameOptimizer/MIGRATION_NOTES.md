# GameOptimizer Documentation Migration Notes

## 기준

문서 기준 경로는 GameOptimizer 소스 디렉토리 안의 `docs/` 이다.
문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

## 이번 작업에서 이동한 문서

- `../docs/vision/PVD_v1.0.md` -> `docs/vision/PVD_v1.0.md`
- `../docs/performance/PerformanceEngineSpec.md` -> `docs/performance/PerformanceEngineSpec.md`

## 이번 작업에서 이동하지 않은 문서

- `docs/architecture/Architecture_Decision_Record.md`
- `docs/architecture/Contract_Enforcement_Matrix.md`
- `docs/contracts/Safety_Contract.md`
- `docs/contracts/Runtime_Contract.md`
- `docs/release/Release_Gate_Spec.md`
- `docs/release/Evidence_Schema.md`
- `docs/release/RC_Runbook.md`
- `docs/release/Release_Blocker_List.md`
- `docs/release/Release_Regression_Matrix.md`

위 문서들은 이미 문서 루트의 `docs/...` 아래에 존재하며, `run_release_gate_static_checks.py`, `verify_rc_candidate.py`, `create_rc_evidence_bundle.py`, release runbook 문서들이 `docs/...` 기준으로 참조한다. 이동하면 Release Gate 경로가 깨질 수 있으므로 제자리 유지한다.

## 경로 충돌 및 중복

- repository root의 `docs/architecture/Architecture_Decision_Record.md`는 문서 루트의 `docs/architecture/Architecture_Decision_Record.md`와 SHA-256이 동일한 중복 문서다. 기존 문서 삭제 금지 원칙에 따라 삭제하지 않았다.
- repository root의 `docs/architecture/Contract_Enforcement_Matrix.md`는 문서 루트의 `docs/architecture/Contract_Enforcement_Matrix.md`보다 짧고 SHA-256이 다르다. 어느 문서가 authoritative인지 release script 기준으로는 문서 루트의 `docs/architecture/Contract_Enforcement_Matrix.md`이므로, repository root의 중복 문서는 삭제하지 않고 충돌로 기록한다.
- repository root의 빈 `docs/vision/` 및 `docs/performance/` 디렉토리는 파일 이동 후 남을 수 있다. Git은 빈 디렉토리를 추적하지 않으므로 별도 삭제 작업을 하지 않았다.

## 향후 정리 제안

1. repository root의 legacy `docs/architecture/*` 중복 문서를 제거할지 별도 승인 후 결정한다.
2. Release Gate 스크립트가 참조하는 모든 문서 경로를 유지한 상태에서 v3.1 문서만 `docs/vision`, `docs/performance`, `docs/modules`로 확장한다.
3. `docs/requirements/SRS_Rev1_3.md`는 기존 SRS 원본 위치와 형식이 확인된 뒤 Markdown 변환본으로 추가한다.
4. `docs/adr/` 개별 ADR 분리는 기존 `docs/architecture/Architecture_Decision_Record.md`의 static gate 의존성이 분리된 뒤 진행한다.
