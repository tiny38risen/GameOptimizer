// Build: cl /std:c++latest /O2 /W4 /WX /permissive- main.cpp RuntimeOrchestrator.cpp
// MODULE: main
// ERROR-POLICY: expected
// Reason: application boundary delegates runtime ownership to RuntimeOrchestrator.

#include "RuntimeOrchestrator.h"

int wmain(int argc, wchar_t* argv[])
{
    RuntimeOrchestrator orchestrator(argc, argv);
    return orchestrator.run();
}
