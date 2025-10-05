
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

#include "utils.hpp"

#pragma comment(lib, "dbghelp.lib")

using namespace TTD;
//using namespace Replay;
using namespace TTD::Replay;


int TtdBasic(int argc, char* argv[]) {
    //if (argc < 4) {
    //    std::cerr << "Usage: MinimalTTDTraceWithSource <trace.run> <pdb_dir>\n";
    //    return 1;
    //}

    //std::filesystem::path traceFile = argv[1];
    //std::string pdbDir = argv[2];

    std::filesystem::path traceFile = "D:\\prog\\windbg-vscode\\sampleWorkspace\\MyApp\\x64\\Debug\\MyApp01.run";
    std::filesystem::path pdbDir = traceFile.parent_path();
    std::filesystem::path exeFile = "D:\\prog\\windbg-vscode\\sampleWorkspace\\MyApp\\x64\\Debug\\MyApp.exe";

    // 1. dbghelp初期化（PDBパス指定）
    HANDLE process = GetCurrentProcess();
    //HANDLE process = static_cast<HANDLE>(this)
    //const DWORD options = SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES;
    if (!SymInitialize(process, pdbDir.string().c_str(), FALSE)) {
        //if (!SymInitialize(process, NULL, TRUE)) {
        std::cerr << "Failed to initialize dbghelp\n";
        return 1;
    }


    // 2. TTDリプレイエンジン起動
    auto [engine, res] = MakeReplayEngine();
    if (res != 0 || !engine) {
        std::cerr << "Failed to create replay engine (" << res << ")\n";
        return 1;
    }

    // This allows the tool to get any messages from the initialization of the engine
    BasicErrorReporting errorReporting;
    engine->RegisterDebugModeAndLogging(DebugModeType::None, &errorReporting);

    auto systemInfo = engine->GetSystemInfo();
    auto lifetime = engine->GetLifetime();

    if (!engine->Initialize(traceFile.c_str())) {
        std::cerr << "Failed to initialize engine with trace\n";
        return 1;
    }

    // Print the module list, showing when and where each module was loaded
    std::cout << std::format("Module loading activity:\n");
    //for (ModuleInstance const& moduleInstance : engine->GetModuleInstanceList()) {
    auto const* moduleInstances = engine->GetModuleInstanceList();
    DWORD64 base = 0;
    size_t moduleSize = 0;
    for (int i = 0; i < engine->GetModuleInstanceCount(); i++) {
        ModuleInstance const& moduleInstance = moduleInstances[i];
        //std::wcout << std::format(L"[{}]  0x{:016X} 0x{:08X} {}\n",
        //    PositionRange(Position(moduleInstance.LoadTime), Position(moduleInstance.UnloadTime)),
        //    moduleInstance.pModule->Address,
        //    moduleInstance.pModule->Size,
        //    ModuleName(*moduleInstance.pModule));


        //std::wcout << PositionRange(Position(moduleInstance.LoadTime), Position(moduleInstance.UnloadTime));
        std::cout << "0x" << std::hex << (DWORD64)moduleInstance.pModule->Address << " ";
        std::wcout << ModuleName(*moduleInstance.pModule) << std::endl;
        //std::wcout << moduleInstance.pModule->Size << std::endl;

        //std::wstring_convert<std::codecvt_utf8<wchar_t> > converter;
        //std::string mn = converter.to_bytes(ModuleName(*moduleInstance.pModule));

        std::string mn = WStringToString(ModuleName(*moduleInstance.pModule));

        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
        DWORD64 baseLoaded = SymLoadModuleEx(
            process,      // Process handle of the current process
            NULL,     // Handle to the image file (not used here)   
            mn.c_str(),  // Path to the module
            NULL,   // Module name (can be NULL to use the file name)
            static_cast<DWORD64>(moduleInstance.pModule->Address), // Base address of the module
            0,		  // Size of the module (0 means the entire file)
            NULL,   // Optional data (not used here)
            0		// Flags (not used here)
        );

        if (baseLoaded != 0) {
            std::cout << "  SymLoadModuleEx succeeded: base = 0x" << std::hex << baseLoaded << "\n";
        }
        else {
            std::cout << "  SymLoadModuleEx FAILED.\n";
        }

        //if (mn == "D:\\prog\\WinDbg-Samples\\TTD\\LiveRecorderApiSample\\out\\x64-Debug\\LiveRecorderApiSample.exe") {
        if (mn == exeFile) {
            std::cout << "Found target module\n";
            ////std::cout << "  Base: 0x" << std::hex << moduleInstance.pModule->Address << "\n";
            ////std::cout << "  Size: 0x" << std::hex << moduleInstance.pModule->Size << "\n";
            //std::cout << "  Load time: " << std::dec << moduleInstance.LoadTime.Sequence << ":" << moduleInstance.LoadTime.Steps << "\n";
            //std::cout << "  Unload time: " << std::dec << moduleInstance.UnloadTime.Sequence << ":" << moduleInstance.UnloadTime.Steps << "\n";

            // モジュールのベースアドレスとサイズを保存
            base = (DWORD64)moduleInstance.pModule->Address;
            moduleSize = moduleInstance.pModule->Size;
        }
    }


    SymEnumSymbols(process, base, NULL, [](PSYMBOL_INFO pSymInfo, ULONG, PVOID) -> BOOL {
        std::cout << "Symbol: " << pSymInfo->Name << " @ 0x" << std::hex << pSymInfo->Address << std::endl;
        return TRUE;
        }, NULL);

    //HMODULE hMod = LoadLibrary(L"D:\\prog\\WinDbg-Samples\\TTD\\LiveRecorderApiSample\\out\\x64-Debug\\LiveRecorderApiSample.exe");
    //base = (DWORD64)hMod;

    // Load all modules (for symbol resolution)
    //SymSetOptions(options);
    //SymLoadModuleEx(process, NULL, "D:\\prog\\WinDbg - Samples\\TTD\\LiveRecorderApiSample\\out\\x64-Debug\\LiveRecorderApiSample.exe", NULL, base, 0, NULL, 0);
    //SymLoadModuleEx(process, NULL, "D:\\prog\\WinDbg-Samples\\TTD\\LiveRecorderApiSample\\out\\x64-Debug\\LiveRecorderApiSample.exe", NULL, base, 0, NULL, 0);


    //engine->GetKeyframeCount()


    try
    {
        ProcessTrace(*engine);
    }
    catch (std::exception const& e)
    {
        std::cout << std::format("Error: {}\n", e.what());
        return -1;
    }



    // 3. カーソル作成 & PC取得
    auto cursor = engine->NewCursor();
    //cursor->SetPosition(Position::Min);
    cursor->SetPosition(lifetime.Min);
    Position pos = cursor->GetPosition();
    printf("%llu:%llu", pos.Sequence, pos.Steps);

    uint64_t pc = (uint64_t)cursor->GetProgramCounter();
    std::cout << "PC at start: 0x" << std::hex << pc << std::endl;

    DWORD64 addr = 0;
    //addr = 0x00007fff4cdc8353;
    //addr = 0x00007ff7c7a05e8b;
    addr = 0x00007ff6f0e42260;
    //addr = pc;


    std::cout << "Looking for source info for addr 0x" << std::hex << addr << std::endl;
    for (int i = 0; i < 100; i++) {
        pc = (uint64_t)cursor->GetProgramCounter();

        std::cout << "PC: 0x" << std::hex << pc << std::endl;
        if (addr >= base && addr < base + moduleSize) {
            std::cout << "  (in module)\n";
        }
        else {
            std::cout << "  (not in module)\n";
        }

        std::cout << "ModuleBase: " << "0x" << std::hex << SymGetModuleBase64(process, pc) << std::endl;;

        SYMBOL_INFO* symInfo = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
        symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
        symInfo->MaxNameLen = MAX_SYM_NAME;

        if (SymFromAddr(process, addr, 0, symInfo)) {
            std::cout << "SymFromAddr: " << symInfo->Name << " @ 0x" << std::hex << symInfo->Address << std::endl;
        }
        else {
            std::cout << "  SymFromAddr: Function info not found (PDB or symbols not loaded)\n";
            // GetLastError()でエラーコードを取得可能
            DWORD err = GetLastError();
            std::cout << "  SymFromAddr: GetLastError: " << err << std::endl;
        }


        // 4. dbghelpでソース位置を解決
        DWORD displacement = 0;
        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        if (SymGetLineFromAddr64(process, addr, &displacement, &line)) {
            //if (SymGetLineFromAddr64(process, 0x00007ff7c7a05e8b, &displacement, &line)) {
            std::cout << "Source: " << line.FileName << " @ line " << std::dec << line.LineNumber << std::endl;
        }
        else {
            std::cout << "Source info not found (PDB or symbols not loaded)\n";
            // GetLastError()でエラーコードを取得可能
            DWORD err = GetLastError();
            std::cout << "GetLastError: " << err << std::endl;
        }

        cursor->ReplayForward(StepCount{ 1 });
    }





    SymCleanup(process);
    return 0;
}


