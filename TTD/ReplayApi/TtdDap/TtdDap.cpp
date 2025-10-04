// Minimal TTD trace info tool: print PC and try to print source location (needs PDB + dbghelp)
#include <TTD/IReplayEngineStl.h>
#include <TTD/ErrorReporting.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <windows.h>
#include <dbghelp.h>

#include <string>
#include <locale>
#include <codecvt>

#include "Formatters.h"

#pragma comment(lib, "dbghelp.lib")

using namespace TTD;
using namespace TTD::Replay;

int ttdproc() {
    // 基本設定
    std::filesystem::path traceFile = "D:\\prog\\windbg-vscode\\sampleWorkspace\\MyApp\\x64\\Debug\\MyApp01.run";
    std::filesystem::path pdbDir = traceFile.parent_path();
    std::filesystem::path exeFile = "D:\\prog\\windbg-vscode\\sampleWorkspace\\MyApp\\x64\\Debug\\MyApp.exe";

    // dbghelp の初期化
    HANDLE process = GetCurrentProcess();
    if (!SymInitialize(process, pdbDir.string().c_str(), FALSE)) {
        std::cerr << "Failed to initialize dbghelp\n";
        return 1;
    }

    // TTD リプレイエンジンの起動
    auto [engine, res] = MakeReplayEngine();
    if (res != 0 || !engine) {
        std::cerr << "Failed to create replay engine (" << res << ")\n";
        return 1;
    }

    // トレースファイルを指定して初期化
    if (!engine->Initialize(traceFile.c_str())) {
        std::cerr << "Failed to initialize engine with trace\n";
        return 1;
    }

    // モジュール一覧を出力し、シンボルを読み込む
    std::cout << std::format("Module loading activity:\n");

    auto const* moduleInstances = engine->GetModuleInstanceList();
    DWORD64 base = 0;
    size_t moduleSize = 0;

    // 各モジュールを列挙
    for (int i = 0; i < engine->GetModuleInstanceCount(); i++) {
        ModuleInstance const& moduleInstance = moduleInstances[i];

        // モジュールのアドレスと名前を出力
        std::cout << "0x" << std::hex << (DWORD64)moduleInstance.pModule->Address << " ";
        std::wcout << ModuleName(*moduleInstance.pModule) << std::endl;

        // モジュール名をマルチバイト文字列に変換
        std::string moduleName = [&moduleInstance]() {
            std::wstring oWString = ModuleName(*moduleInstance.pModule);
            int iBufferSize = WideCharToMultiByte(CP_OEMCP, 0, oWString.c_str(), -1, (char*)NULL, 0, NULL, NULL);
            CHAR* cpMultiByte = new CHAR[iBufferSize];
            WideCharToMultiByte(CP_OEMCP, 0, oWString.c_str(), -1, cpMultiByte, iBufferSize, NULL, NULL);
            std::string oRet(cpMultiByte, cpMultiByte + iBufferSize - 1);
            delete[] cpMultiByte;
            return oRet;
            }();

        // シンボル読み込みオプション設定
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);

        // シンボルをロード
        DWORD64 baseLoaded = SymLoadModuleEx(
            process,
            NULL,
            moduleName.c_str(),
            NULL,
            static_cast<DWORD64>(moduleInstance.pModule->Address),
            moduleInstance.pModule->Size,
            NULL,
            0
        );

        if (baseLoaded != 0) {
            std::cout << "  SymLoadModuleEx succeeded: base = 0x" << std::hex << baseLoaded << "\n";
        }
        else {
            std::cout << "  SymLoadModuleEx FAILED.\n";
        }

        // 対象モジュールを特定
        if (moduleName == exeFile) {
            std::cout << "Found target module\n";
            base = (DWORD64)moduleInstance.pModule->Address;
            moduleSize = moduleInstance.pModule->Size;
        }
    }

    // カーソル作成と PC 取得
    auto cursor = engine->NewCursor();
    auto lifetime = engine->GetLifetime();
    cursor->SetPosition(lifetime.Min);

    // カーソル位置を出力
    Position pos = cursor->GetPosition();
    printf("Pos: %llu:%llu\n", pos.Sequence, pos.Steps);

    // 実行開始時のPCを取得
    uint64_t pc = (uint64_t)cursor->GetProgramCounter();
    std::cout << "PC at start: 0x" << std::hex << pc << std::endl;

    // 任意のアドレスを指定して情報を確認
    DWORD64 addr = 0x00007ff6f0e42260;
    std::cout << "Looking for source info for addr 0x" << std::hex << addr << std::endl;

    // 対象アドレスがモジュール範囲内か確認
    if (addr >= base && addr < base + moduleSize) {
        std::cout << "  (in module)\n";
    }
    else {
        std::cout << "  (not in module)\n";
    }

    // モジュールベースアドレスを表示
    std::cout << "ModuleBase: 0x" << std::hex << SymGetModuleBase64(process, addr) << std::endl;

    // 関数名の取得
    SYMBOL_INFO* symInfo = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
    symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    symInfo->MaxNameLen = MAX_SYM_NAME;

    // シンボル情報を解決
    if (SymFromAddr(process, addr, 0, symInfo)) {
        std::cout << "SymFromAddr: " << symInfo->Name << " @ 0x" << std::hex << symInfo->Address << std::endl;
    }
    else {
        DWORD err = GetLastError();
        std::cout << "  SymFromAddr: Function info not found (PDB or symbols not loaded)\n";
        std::cout << "  SymFromAddr: GetLastError: " << err << std::endl;
    }

    // ソースコード位置の解決
    DWORD displacement = 0;
    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(line);

    // アドレスからソース行番号を特定
    if (SymGetLineFromAddr64(process, addr, &displacement, &line)) {
        std::cout << "Source: " << line.FileName << " @ line " << std::dec << line.LineNumber << std::endl;
    }
    else {
        DWORD err = GetLastError();
        std::cout << "Source info not found (PDB or symbols not loaded)\n";
        std::cout << "GetLastError: " << err << std::endl;
    }

    // dbghelp の終了処理
    SymCleanup(process);
    return 0;
}

int main(int argc, char* argv[]) {
    ttdproc();
}
