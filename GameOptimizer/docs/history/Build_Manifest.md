> Status: Archived  
> 이 문서는 과거 기록입니다. 현재 기준 문서가 아닙니다.
File | Production build | Test build
BackgroundController.cpp | included | included when testing BackgroundController
docs/architecture/Architecture_Decision_Record.md | contract documentation | static gate contract input

Production excludes: *Tests.cpp
Test target excludes: main.cpp and unrelated *Tests.cpp

Patch release gate logging overhead pass:
- main.cpp
- docs/history/Patch_History.md
- docs/release/Release_Gate_Spec.md
- docs/release/Release_Performance_Checklist.md


Operational Gate patch:
- main.cpp: added --max-runtime-seconds for bounded release-gate runs; fixed confirmed-main logging branch.
- docs/release/Release_Regression_Matrix.md: required merge-blocking regression scenarios.
- docs/release/Operational_Runbook.md: dry-run/soft-apply/apply validation order and rollback interpretation.
- docs/history/Patch_History.md: consolidated historical patch summaries.
- run_release_gate_smoke.bat: local smoke runner.
