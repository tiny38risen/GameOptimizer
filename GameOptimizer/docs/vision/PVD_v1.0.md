# GameOptimizer v3.1 Product Vision Document

## 1. 문서 개요

문서명: GameOptimizer v3.1 Product Vision Document

버전: v1.0

대상 프로젝트: GameOptimizer v3.1

작성 목적: 싱글코어 병목 게임 전용 성능 최적화 방향 정의

적용 범위: v3.1 제품 방향, 대상 게임군, 제외 범위, 성능 기능의 안전 조건, 성공 기준

기준 프로젝트 버전: GameOptimizer v3.0-rc1 안정화 기반 및 v3.1 성능 엔진 기획 단계

관련 문서 경로 기준: 문서 루트. 문서 내부 참조는 `docs/...` 형식을 우선 사용한다.

- 기존 GameOptimizer v3.0 SRS Rev 1.3
- `docs/README.md`
- `docs/architecture/Architecture_Decision_Record.md`
- `docs/architecture/Contract_Enforcement_Matrix.md`
- `docs/contracts/Safety_Contract.md`
- `docs/release/Release_Gate_Spec.md`
- `docs/release/Evidence_Schema.md`

본 문서는 구현 명세가 아니라 제품 방향 문서다. GameOptimizer가 앞으로 어떤 문제를 풀고, 어떤 게임군에 집중하며, 어떤 안전 조건 아래에서 성능 기능을 확장할지 정의한다.

## 2. 제품 비전

GameOptimizer는 최신 멀티코어 게임을 범용적으로 빠르게 만드는 프로그램이 아니다. GameOptimizer는 싱글코어 병목 구조를 가진 구형 또는 레거시 게임에서 메인 스레드 실행 안정성을 높이고, 백그라운드 간섭과 스케줄링 변동을 줄이는 Windows 기반 성능 안정화 도구다.

v3.1의 제품 방향은 범용 FPS Booster가 아니다. 목표는 모든 게임의 평균 FPS를 올리는 것이 아니라, 싱글코어 병목 게임에서 메인 스레드(Main Thread)가 더 안정적으로 실행되도록 돕는 것이다.

GameOptimizer는 체감 개선만을 성공으로 보지 않는다. 성능 향상은 메인 스레드 이동(Main Thread Migration), 코어 격리(Core Isolation), 네트워크 지터(RTT Jitter), DPC Spike, 롤백 결과(Rollback Result), 런타임 검증(Runtime Validation) 같은 성능 증거(Performance Evidence)로 설명될 수 있어야 한다.

성능보다 먼저 지켜야 할 기준은 안전한 적용(Safe Apply)과 복구 가능성(Recoverability)이다. 잘못된 정책을 적용하지 않는 것, 적용 전 상태를 보존하는 것, 실패 시 롤백(Rollback)하는 것이 성능 기능보다 우선한다.

## 3. 대상 게임 정의

### 3.1 주요 대상

GameOptimizer v3.1의 주요 대상은 싱글코어 또는 메인 스레드 병목(Main Thread Bottleneck)이 강한 게임이다. 특히 오래된 엔진, 레거시 구조, 제한적인 멀티스레드 설계 때문에 Windows Scheduler의 thread migration과 백그라운드 간섭에 민감한 게임을 우선한다.

대상 게임군은 다음 특성을 가진다.

- 싱글코어 또는 메인 스레드 병목이 강한 게임
- 오래된 엔진 또는 레거시 구조 게임
- 네트워크 지연(Network Latency)과 입력 반응성(Input Responsiveness)이 중요한 온라인 게임
- 메인 스레드 migration이 잦을 때 체감 지연이 발생하는 게임
- CPU topology, CCX, SMT, affinity 제어의 영향을 받을 수 있는 게임

### 3.2 예시 대상

다음 목록은 우선 검증 대상 후보다. 특정 게임에 대한 보증 또는 적용 가능성을 단정하지 않는다.

- 마비노기
- 메이플스토리
- 던전앤파이터
- 라그나로크 온라인
- 구형 MMORPG
- 구형 DirectX 기반 온라인 게임
- 일부 구버전 Unity/엔진 기반 게임

각 후보는 실제 적용 전에 dry-run, soft-apply, 성능 증거(Performance Evidence), 롤백(Rollback) 결과를 통해 개별 검증되어야 한다.

## 4. 비대상 게임 정의

GameOptimizer는 모든 게임을 빠르게 만들겠다는 제품이 아니다. 최신 게임 대부분은 이미 자체 스케줄링과 멀티코어 활용 구조가 존재하므로, GameOptimizer의 개입 여지가 제한적이다.

v3.1의 핵심 대상이 아닌 게임군은 다음과 같다.

- 최신 DX12/Vulkan 기반 게임
- 자체 Job System 또는 Task Graph 기반 멀티코어 최적화 게임
- GPU 병목(GPU Bottleneck)이 주요 원인인 게임
- 이미 CPU 스케줄링과 멀티스레드 처리가 잘 최적화된 게임
- Anti-Cheat 정책상 외부 스케줄링 제어가 위험한 게임
- GameOptimizer 적용 시 실질적 성능 이득보다 리스크가 큰 게임

비대상 게임에서는 GameOptimizer가 적용을 거부하거나 monitoring-only로 동작하는 것이 올바른 결과일 수 있다. 적용하지 않는 판단도 제품 안전성의 일부다.

## 5. 문제 정의

싱글코어 병목 게임은 최신 멀티코어 게임과 다른 방식으로 성능 문제가 드러난다. 전체 CPU 사용률이 낮아 보여도, 실제 병목은 하나의 메인 스레드(Main Thread)에 집중될 수 있다.

v3.1이 다루려는 핵심 문제는 다음과 같다.

1. 메인 스레드가 특정 코어에서 안정적으로 유지되지 않음
2. Windows Scheduler에 의해 thread migration 발생
3. 백그라운드 프로세스가 게임 실행 코어를 침범
4. SMT sibling 또는 다른 CCX 이동으로 캐시 효율 저하 가능
5. 네트워크 지터(RTT Jitter)와 DPC Spike로 입력/반응 지연 발생
6. 잘못된 스레드에 affinity/priority를 적용하면 오히려 성능 저하 발생
7. 최적화 적용 실패 시 원상 복구가 어려울 수 있음

이 문제들은 무조건 강한 정책으로 해결할 수 없다. 잘못된 대상에 강한 정책을 적용하면 성능 개선보다 리스크가 커진다.

## 6. 제품 목표

### 6.1 Main Thread Stability

v3.1은 메인 스레드 안정성(Main Thread Stability)을 핵심 목표로 둔다. 메인 스레드를 더 정확하게 식별하고, 신뢰도가 낮은 후보에는 affinity/priority 정책을 적용하지 않는다.

메인 스레드 식별은 단일 순간의 CPU 사용량이 아니라 지속 관찰, EMA 점수(EMA Score), CPU delta 안정성, migration 패턴, 후보 유지 시간, 스레드 수명 안정성을 함께 고려해야 한다.

### 6.2 Core Isolation

v3.1은 메인 스레드가 사용하는 코어 주변의 간섭을 줄이는 코어 격리(Core Isolation)를 목표로 한다. 게임의 핵심 실행 코어에 백그라운드 프로세스가 침범하는 상황을 줄이고, 필요 시 background restriction을 제한적으로 적용한다.

Core Isolation은 게임 성능을 보장하는 기능이 아니라 간섭 가능성을 낮추는 정책이다. 적용 전후는 성능 증거(Performance Evidence)로 기록되어야 한다.

### 6.3 CCX Stability

v3.1은 AMD Ryzen 계열처럼 CCX/L3 구조가 성능에 영향을 줄 수 있는 환경에서 CCX 안정성(CCX Stability)을 고려한다. 목표는 불필요한 CCX 이동을 줄이고, main thread와 관련 스레드가 더 예측 가능한 topology 안에서 실행되도록 돕는 것이다.

Intel Hybrid 구조나 Processor Group 1+ 환경은 별도 안전 조건이 필요하다. 안전한 경로가 확인되지 않으면 monitoring-only로 남긴다.

### 6.4 Controlled Aggressive Mode

v3.1은 무조건적인 aggressive mode를 제공하지 않는다. 강한 정책은 메인 스레드 신뢰도(Main Thread Confidence)가 충분하고, 런타임 관찰과 롤백 상태 저장이 성공했으며, ApplyGuard 검증이 가능한 경우에만 후보가 된다.

Controlled Aggressive Mode의 목적은 더 넓은 시스템 영역을 건드리는 것이 아니라, 더 정확한 대상에 제한된 정책을 적용하는 것이다.

### 6.5 Performance Evidence

v3.1은 성능 향상을 감각이 아니라 기록과 수치로 판단한다. Before/After 비교, 정책 활성화 기록, migration 변화, 네트워크 지표, DPC 관찰, 롤백 결과, 런타임 검증 요약이 함께 남아야 한다.

Evidence가 없는 개선 주장은 v3.1의 성공 기준이 될 수 없다.

## 7. 제품 비목표

v3.1의 목표는 더 많은 시스템 영역을 건드리는 것이 아니라, 더 정확한 대상에 더 안전하게 제한된 정책을 적용하는 것이다.

v3.1에서 하지 않을 일은 다음과 같다.

- 모든 게임 자동 최적화
- 무조건적인 aggressive mode
- Anti-Cheat 우회
- DLL Injection
- Kernel Driver
- Game Memory Patch
- 게임 내부 socket 강제 수정
- Raw Input 추정 기반 무리한 input thread pinning
- Processor Group 1+에 대한 안전하지 않은 process-wide affinity 강제 적용
- 검증 없는 registry 자동 변경
- AI/ML 기반 자동 튜닝
- Self-Learning 최적화 시스템

GameOptimizer는 Anti-Cheat를 우회하거나 게임 내부 동작을 변경하는 제품이 아니다. v3.1은 비침습적 Win32 API 기반(Non-invasive Win32 API based) 정책만 제품 범위로 둔다.

## 8. 성능 전략 방향

### 8.1 Main Thread Confidence

v3.1 성능 엔진은 메인 스레드 후보에 대해 메인 스레드 신뢰도(Main Thread Confidence)를 계산한다. Confidence는 정책 적용 가능성을 판단하는 핵심 기준이다.

Confidence 산정 시 고려할 요소는 다음과 같다.

- EMA 점수(EMA Score)
- CPU delta 안정성
- 관찰 지속 시간
- migration 빈도
- 후보 유지 시간
- 스레드 수명 안정성

정책 기준은 다음과 같다.

```text
Confidence가 High 미만이면 affinity/priority 적용 금지.
Confidence가 VeryHigh일 때만 aggressive single-core mode 후보가 될 수 있음.
```

이 기준은 잘못된 스레드에 정책이 적용되는 것을 막기 위한 제품 안전 조건이다.

### 8.2 Main Thread Isolation

메인 스레드 격리(Main Thread Isolation)는 메인 스레드가 배치될 core를 보호하는 전략이다.

```text
Main Thread Core를 다른 백그라운드 프로세스와 최대한 분리한다.
```

이 전략은 백그라운드 프로세스의 affinity/priority 제어를 포함할 수 있으나, 모든 제어는 rollback state 저장, ApplyGuard, 검증(Verify), Evidence 기록을 따라야 한다.

### 8.3 CCX Stability Policy

CCX 안정성 정책(CCX Stability Policy)은 같은 CCX/L3 범위 내에서 main thread와 관련 스레드의 이동을 최소화하는 방향이다.

이 정책은 특히 CCX/L3 경계 이동이 캐시 효율과 frame time 안정성에 영향을 줄 수 있는 CPU에서 의미가 있다. 단, topology 판단이 불확실하거나 Processor Group 정책이 안전하게 확인되지 않은 경우 강제 적용하지 않는다.

### 8.4 Aggressive Single-Core Mode

Aggressive Single-Core Mode는 v3.1의 기본값이 아니다. 다음 조건을 모두 만족할 때만 활성화 후보가 된다.

```text
Confidence >= VeryHigh
Runtime >= 30초
Migration 안정
Rollback State 저장 성공
ApplyGuard Verify 가능
```

조건 중 하나라도 충족되지 않으면 aggressive single-core mode는 적용하지 않는다. 이 경우 monitoring-only 또는 soft-apply가 더 적절한 동작이다.

### 8.5 Performance Evidence

v3.1은 Before/After 비교를 기록해야 한다. 필수 기록 항목은 다음과 같다.

- Main Thread Migration
- Main Thread Confidence History
- Policy Activation Count
- Core Relocation Count
- RTT Jitter
- DPC Spike
- Rollback Result
- Runtime Validation Summary

성능 증거(Performance Evidence)는 기능 홍보 자료가 아니라 정책 판단과 릴리즈 판단의 근거다. 개선이 작거나 불확실한 경우에도 그 결과를 있는 그대로 기록해야 한다.

## 9. 안전 원칙

모든 성능 기능은 다음 순서를 따라야 한다.

```text
Observe
↓
Analyze
↓
Decide
↓
Save
↓
Arm
↓
Apply
↓
Verify
↓
Commit
↓
Evidence
```

실패 시에는 다음 흐름을 따른다.

```text
Rollback
↓
Evidence
↓
Safe Shutdown 또는 Monitoring-only fallback
```

Rollback 없는 성능 기능은 v3.1에 포함될 수 없다.

Evidence 없는 성능 기능은 v3.1에 포함될 수 없다.

안전 원칙은 다음 제품 제한을 포함한다.

- 비침습적 Win32 API 기반(Non-invasive Win32 API based) 정책만 사용
- Anti-Cheat 우회 금지
- DLL Injection 금지
- Kernel Patch 금지
- Game Memory Patch 금지
- ApplyGuard 기반 트랜잭션(Transaction) 유지
- Rollback 필수
- Evidence 기반 검증 필수

Access Denied, Anti-Cheat 경계, unsupported processor topology, 불충분한 main thread confidence는 실패가 아니라 안전한 중단 또는 fallback 판단의 입력이다.

## 10. 성공 기준

### 10.1 정량 기준 후보

다음은 목표 후보이며, 실제 값은 Performance Validation Plan에서 확정한다.

```text
Main Thread Migration 감소
Main Thread Confidence 안정화
Background interference 감소
RTT jitter 안정화
DPC spike 관찰 가능
Rollback 성공률 100%
Runtime validation BLOCKER 0
```

정량 기준 후보는 v3.1 제품 방향을 검증하기 위한 측정 후보이며, 그 자체가 즉시 릴리즈 승인 기준은 아니다. 실제 목표값, 측정 구간, 샘플 수, 측정 도구, 통계 처리 방식, pass/fail threshold는 Performance Validation Plan에서 확정한다.

정량 기준은 모든 환경에서 동일한 수치를 요구하기보다, 대상 게임과 테스트 환경별 Before/After 비교가 가능해야 한다. 개선 폭이 작거나 불확실한 경우에도 안전 조건이 통과했는지, monitoring-only fallback이 적절했는지, 성능 증거(Performance Evidence)가 재현 가능한지 함께 판단한다.

### 10.2 정성 기준

v3.1의 정성 기준은 다음과 같다.

- 사용자가 적용 전후 차이를 로그로 확인 가능
- 잘못된 게임에는 적용하지 않거나 monitoring-only로 동작
- 오류 발생 시 시스템 상태를 복구 가능
- 성능 기능이 안정성 체계를 우회하지 않음

제품 성공은 높은 수치 하나로 판단하지 않는다. 안전하게 적용되고, 실패 시 복구되며, 결과가 Evidence로 설명될 때 성공으로 본다.

## 11. 릴리즈 방향

v3.1은 다음 순서로 개발되어야 한다.

```text
1. Product Vision 확정
2. Performance Engine Specification 작성
3. Policy Specification 작성
4. Architecture Design 갱신
5. Module Design Specification 작성
6. 구현
7. Performance Evidence 기록
8. 실제 싱글코어 게임 검증
9. RC Gate
10. Release Candidate
```

릴리즈 판단은 기존 v3.0 안정화 원칙을 계승한다. 성능 기능이 추가되더라도 dry-run, soft-apply, runtime validation, rollback result, evidence bundle, RC Gate를 우회하지 않는다.

## 12. Approval Criteria

본 Product Vision Document는 다음 조건을 만족할 때 승인 대상으로 본다. 이 승인은 v3.1 제품 방향에 대한 승인이지, 구현 완료 또는 릴리즈 승인으로 해석하지 않는다.

- GameOptimizer v3.1의 핵심 제품 범위가 싱글코어 병목 게임 전용 성능 안정화 도구로 명확히 정의됨
- 범용 FPS Booster, 모든 게임 자동 최적화, Anti-Cheat 우회, DLL Injection, Kernel Patch, Game Memory Patch가 비목표로 명확히 제외됨
- 대상 게임과 비대상 게임의 판단 기준이 구분되어 있고, 게임 목록이 공식 지원이 아닌 우선 검증 대상 후보로 표현됨
- Main Thread Confidence, Main Thread Isolation, CCX Stability Policy, Controlled Aggressive Mode, Performance Evidence의 방향이 안전 조건과 함께 정의됨
- 모든 성능 기능이 Observe, Analyze, Decide, Save, Arm, Apply, Verify, Commit, Evidence 흐름과 Rollback fallback 원칙을 따라야 함이 명시됨
- 정량 기준 후보가 릴리즈 승인 수치가 아니라 Performance Validation Plan에서 확정될 측정 후보로 설명됨
- Open Questions가 Blocking / Non-blocking으로 분류되어 다음 문서 작성 시 해결 순서가 드러남

## 13. 최종 요약

GameOptimizer v3.1은 모든 게임을 빠르게 만드는 범용 최적화 프로그램이 아니다.

v3.1은 싱글코어 병목 게임에서 메인 스레드 안정성, 코어 격리, 백그라운드 간섭 제어, 성능 Evidence를 통해 제한적이지만 검증 가능한 성능 개선을 목표로 한다.

안전성, 롤백, Evidence는 성능 기능보다 우선한다.

## Open Questions

### Blocking

다음 질문은 Performance Engine Specification 또는 Policy Specification 작성 전에 결정되어야 한다.

1. Main Thread Confidence의 High/VeryHigh 기준값은 몇으로 둘 것인가?
2. Aggressive Single-Core Mode의 최소 관찰 시간은 30초로 충분한가?
3. CCX 정책은 AMD 우선으로 둘 것인가, Intel Hybrid까지 포함할 것인가?
4. Performance Evidence에서 FPS/frame time을 직접 측정할 것인가, 외부 도구 결과를 입력받을 것인가?
5. Background Restriction의 기본 강도는 어느 수준으로 둘 것인가?
6. Apply Mode의 기본값은 계속 soft-apply로 유지할 것인가?

### Non-blocking

다음 질문은 PVD 승인 자체를 막지는 않지만, 실제 검증 계획과 RC Gate 이전에 결정되어야 한다.

1. v3.1의 첫 번째 실제 검증 게임은 무엇인가?
