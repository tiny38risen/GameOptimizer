# GameOptimizer 한국어 UI

이 프로젝트는 기존 C++ `GameOptimizer.exe`를 직접 수정하지 않는 얇은 WinForms 런처입니다.

## 역할

- 최적화 대상 게임 선택
- 테스트 모드, 안전 적용 모드, 강력 적용 모드 실행
- 실시간 점검 기록 기반 상태 표시
- WARN/BLOCKER/PASS 상태 강조
- 기록 폴더와 최신 진단 리포트 열기

## 안전 규칙

- 기본 모드는 `안전 적용 모드`입니다.
- `강력 적용 모드`는 확인창을 거친 뒤 실행합니다.
- UI는 affinity, priority, timer, IRQ, input pinning을 직접 제어하지 않습니다.
- 모든 실제 제어는 기존 C++ 엔진이 담당합니다.
