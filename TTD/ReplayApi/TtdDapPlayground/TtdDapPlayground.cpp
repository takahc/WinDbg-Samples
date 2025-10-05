
// Minimal TTD trace info tool: print PC and try to print source location (needs PDB + dbghelp)
#include <TTD/IReplayEngineStl.h>
#include <TTD/ErrorReporting.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <windows.h>
#include <dbghelp.h>

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <string>
#include <locale>
#include <codecvt>

#include <Formatters.h>


#pragma comment(lib, "dbghelp.lib")

#include "utils.hpp"
#include "TtdDapPlayground.h"

using namespace TTD;
//using namespace Replay;
using namespace TTD::Replay;


int main(int argc, char* argv[]) {
	TtdBasic(argc, argv);

	return 0;
}
