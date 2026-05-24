# Runtime Validation 설계

## 목적
AppliedPolicyTracker가 정책 효과를 검증한 뒤, 본 실행 루프에서 실제 Runtime Metrics와 정책 명령 흐름이 안정적으로 동작하는지 관찰한다.

## 위치
RuntimeValidationMonitor는 제어 모듈이 아니라 관찰 모듈이다.

Metrics → AppliedPolicyTracker → LatencyDecisionLayer → PolicyDispatcher → RuntimeValidationMonitor

## 책임
- Watchdog Runtime Phase마다 validation sample 기록
- main thread 탐지 여부 기록
- main thread policy 적용 여부 기록
- Decision command 수 기록
- Feedback command 수 기록
- Dispatch 실패 수 기록
- Rollback 요청 수 기록
- RTT/DPC/Migration 고위험 cycle 수 기록
- 종료 시 validation summary 출력

## 금지
- RuntimeValidationMonitor는 PolicyCommand를 생성하지 않는다.
- RuntimeValidationMonitor는 rollback을 직접 실행하지 않는다.
- RuntimeValidationMonitor는 scheduler, background, thread tracker 상태를 변경하지 않는다.

## 성공 기준
- dispatch failure가 threshold 이하
- rollback request가 threshold 이하
- main thread 미탐지 연속 cycle이 threshold 이하
- 종료 시 runtime validation summary 출력

## 빌드 분리
본 실행 빌드 포함:
- main.cpp
- RuntimeValidationMonitor.cpp
- AppliedPolicyTracker.cpp
- 기존 모듈 cpp

테스트 빌드 포함:
- RuntimeValidationMonitorTests.cpp
- RuntimeValidationMonitor.cpp

테스트 빌드 제외:
- main.cpp
- AppliedPolicyTrackerTests.cpp
- LatencyDecisionLayerTests.cpp
