File | Production build | Test build
BackgroundController.cpp | included | included when testing BackgroundController
docs/architecture/Architecture_Decision_Record.md | contract documentation | static gate contract input

Production excludes: *Tests.cpp
Test target excludes: main.cpp and unrelated *Tests.cpp

Patch release gate logging overhead pass:
- main.cpp
- docs/history/Patch_History.md
- docs/operations/ReleaseGateRunbook.md
- docs/operations/ReleasePerformanceChecklist.md


Operational Gate patch:
- main.cpp: added --max-runtime-seconds for bounded release-gate runs; fixed confirmed-main logging branch.
- docs/operations/ReleaseRegressionMatrix.md: required merge-blocking regression scenarios.
- docs/operations/OperationalSafetyRunbook.md: dry-run/soft-apply/apply validation order and rollback interpretation.
- docs/history/Patch_History.md: consolidated historical patch summaries.
- run_release_gate_smoke.bat: local smoke runner.
