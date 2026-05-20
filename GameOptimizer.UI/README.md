# GameOptimizer 한국어 UI

이 프로젝트는 기존 C++ `GameOptimizer.exe`를 직접 수정하지 않는 얇은 WinForms 런처입니다.

## 역할

- 대상 프로세스 선택
- 드라이런, 소프트 적용, 실제 적용 실행
- stdout/stderr 로그 실시간 표시
- WARN/BLOCKER/PASS 상태 강조
- `release_gate_logs` 폴더와 최신 evidence report 열기

## 안전 규칙

- 기본 모드는 `드라이런`입니다.
- `실제 적용`은 확인창을 거친 뒤 실행합니다.
- UI는 affinity, priority, timer, IRQ, input pinning을 직접 제어하지 않습니다.
- 모든 실제 제어는 기존 C++ 엔진이 담당합니다.
