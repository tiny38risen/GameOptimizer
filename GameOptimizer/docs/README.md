> Status: Reference  
> 이 문서는 설계 배경과 세부 내용을 보존하기 위한 참고 문서입니다. 현재 개인 프로젝트 기준의 핵심 요약은 `docs/PROJECT_SPEC.md`와 `docs/ARCHITECTURE_AND_SAFETY.md`를 따릅니다.

# GameOptimizer Docs

## 1. 핵심 문서

| 순서 | 문서 | 역할 |
|---|---|---|
| 1 | `docs/PROJECT_SPEC.md` | 프로젝트 목표, 범위, 현재 상태 |
| 2 | `docs/ARCHITECTURE_AND_SAFETY.md` | runtime flow, module owner, safety rule |
| 3 | `docs/VALIDATION_RUNBOOK.md` | build, test, evidence, RC 판단 |
| 4 | `docs/UI_SPEC.md` | UI 화면, 사용자 흐름, 안전 경계 |
| 5 | `docs/DOCUMENT_REGISTER.md` | 전체 문서 상태 등록부 |

## 2. 읽는 순서

1. `PROJECT_SPEC.md`
2. `ARCHITECTURE_AND_SAFETY.md`
3. `VALIDATION_RUNBOOK.md`
4. `UI_SPEC.md`
5. `DOCUMENT_REGISTER.md`

처음 보는 사람은 위 5개만 읽으면 프로젝트의 현재 범위와 안전 경계를 이해할 수 있다.

## 3. 기존 상세 문서

기존 상세 문서는 삭제하지 않는다.
대신 다음처럼 보존한다.

| Status | 사용 방식 |
|---|---|
| Reference | 구현 이해, 배경, 세부 계약 확인 |
| Proposal | 미래 설계, 아직 current implementation 아님 |
| Archived | 과거 계획, 이력, 기록 |
| Superseded | 새 핵심 문서나 다른 기준으로 대체됨 |
| NeedsReview | generator path, 권위, 중복 여부 확인 필요 |
| External | repo 내부 canonical 문서가 아님 |

## 4. 주의

- `PVD_v1.0.md`는 Product Vision이지 SRS가 아니다.
- canonical SRS Markdown은 아직 repo 내부에 없다.
- `Evidence_Schema.md`는 현재 evidence schema 참고 문서다.
- `EvidenceSpecification.md`는 미래 설계로 취급한다.
- `GameOptimizer.UI/README.md`는 참고 문서이며 최종 UI 기준은 `docs/UI_SPEC.md`다.
- Proposal 문서는 `DOCUMENT_REGISTER.md`에서 승격되기 전까지 현재 구현 동작으로 간주하지 않는다.
