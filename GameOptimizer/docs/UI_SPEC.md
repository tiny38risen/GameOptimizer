# GameOptimizer UI Spec

## 1. UI 목적

UI는 GameOptimizer 실행을 쉽게 시작하고 상태를 이해하게 하는 제어 화면이다.
UI는 mutation owner가 아니며 runtime 정책 적용은 C++ runtime 경계가 담당한다.

## 2. 주요 화면

| 화면 | 목적 |
|---|---|
| Target 선택 | 최적화 대상 게임 또는 프로세스 선택 |
| Mode 선택 | 테스트 모드, 안전 적용 모드, 강력 적용 모드 선택 |
| Runtime 상태 | 적용 중, 검증 중, 롤백 중, 실패 상태 표시 |
| Evidence / Log | 최신 진단 리포트와 기록 폴더 열기 |
| Warning / Blocker | Access Denied, Anti-Cheat 위험, BLOCKER 강조 |

## 3. 사용자 흐름

1. 대상 게임 또는 프로세스를 선택한다.
2. 기본값인 안전 적용 모드를 확인한다.
3. 시작 버튼을 눌러 RuntimeOrchestrator 또는 공개 application-level API를 호출한다.
4. UI는 상태와 Evidence 위치를 표시한다.
5. 중지 버튼은 shutdown 요청과 rollback 흐름을 시작한다.

## 4. 상태 표시 기준

| 상태 | UI 표시 |
|---|---|
| 준비 | 대상과 mode 확인 가능 |
| 적용 중 | runtime mutation 진행 중임을 표시 |
| 검증 중 | verify / runtime validation 중임을 표시 |
| 롤백 중 | rollback 진행 중임을 숨기지 않음 |
| WARN | 제한, fallback, Access Denied를 노란색 계열로 표시 |
| BLOCKER | release 불가 또는 안전 문제를 명확히 표시 |
| 실패 | 실패 원인과 log 위치 표시 |

## 5. 시작 / 중지 버튼 동작

- 시작: 선택된 target과 mode를 application-level API로 전달한다.
- 강력 적용 모드: 확인창 후 실행한다.
- 중지: ShutdownRequested를 요청한다.
- 중지 후: 신규 mutation을 요청하지 않는다.
- 종료 중 rollback 실패가 있으면 사용자에게 숨기지 않는다.

## 6. 오류 / 경고 표시

- Access Denied는 정상 fallback 가능성과 제한을 함께 표시한다.
- Anti-Cheat 위험은 명확한 경고로 표시한다.
- Evidence 누락은 성공처럼 표시하지 않는다.
- RuntimeValidation BLOCKER는 PASS보다 우선한다.

## 7. 한글 표시 기준

- 주요 버튼과 상태는 한국어로 표시한다.
- 파일명, 경로, status 값은 영어를 유지한다.
- WARN, BLOCKER, PASS 같은 판정어는 영어를 유지한다.
- 상세 로그는 원문 경로와 함께 표시한다.

## 8. UI 안전 경계

- UI는 제어 화면이지 mutation owner가 아니다.
- UI는 RuntimeOrchestrator 또는 공개 application-level API를 호출한다.
- UI에서 SchedulerController를 직접 호출하지 않는다.
- UI는 affinity, priority, timer, IRQ, input pinning을 직접 제어하지 않는다.
- 적용 중 / 검증 중 / 롤백 중 / 실패 상태를 사용자에게 숨기지 않는다.
