
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

#include "Formatters.h"


#pragma comment(lib, "dbghelp.lib")

using namespace TTD;
//using namespace Replay;
using namespace TTD::Replay;



/*
    wstringをstringへ変換
*/
std::string WStringToString
(
    std::wstring oWString
)
{
    // wstring → SJIS
    int iBufferSize = WideCharToMultiByte(CP_OEMCP, 0, oWString.c_str()
        , -1, (char*)NULL, 0, NULL, NULL);

    // バッファの取得
    CHAR* cpMultiByte = new CHAR[iBufferSize];

    // wstring → SJIS
    WideCharToMultiByte(CP_OEMCP, 0, oWString.c_str(), -1, cpMultiByte
        , iBufferSize, NULL, NULL);

    // stringの生成
    std::string oRet(cpMultiByte, cpMultiByte + iBufferSize - 1);

    // バッファの破棄
    delete[] cpMultiByte;

    // 変換結果を返す
    return(oRet);
}

// A simple error reporting class that prints errors to the console, as required
// by RegisterDebugModeAndLogging().
class BasicErrorReporting : public ErrorReporting
{
public:
    BasicErrorReporting() = default;

    void __fastcall VPrintError(_Printf_format_string_ char const* const pFmt, _In_ va_list argList) override
    {
        char buffer[2048];

        vsprintf_s(buffer, pFmt, argList);

        std::cout << std::format("Error: {}\n", buffer);
    }
};

// Print the exception information in a formatted manner.
static void PrintException(ExceptionEvent const& exception)
{
    // Most of this is standard Win32 exception information, but pThreadInfo
    // is a TTD-specific structure that contains the thread information. In this case, we will
    // print the UniqueId of the thread (unlike Win32 thread IDs, this value is guaranteed to
    // be unique in the trace).
    std::cout << std::format(
        "UTID: {:<6} Code: 0x{:08X} Flags: 0x{:04X} RecordAddress: 0x{:08X} PC: 0x{:08X} Parameters: ",
        exception.pThreadInfo->UniqueId,
        exception.Code,
        exception.Flags,
        exception.RecordAddress,
        exception.ProgramCounter
    );

    if (exception.ParameterCount == 0)
    {
        std::cout << std::format("None\n");
    }
    else
    {
        for (size_t i = 0; i < exception.ParameterCount; ++i)
        {
            if (i == 0)
            {
                std::cout << std::format("(0x{:X}", exception.Parameters[i]);
            }
            else
            {
                std::cout << std::format(", 0x{:X}", exception.Parameters[i]);
            }
        }
        std::cout << std::format(")\n");
    }
}

// This function presents information about the loaded trace file, to give a sense of
// how to use the replay API and the types of information contained in a trace file.
static void ProcessTrace(IReplayEngineView const& replayEngineView) {
    SystemInfo const& systemInfo = replayEngineView.GetSystemInfo();

    std::cout << std::format("Version             : 1.{:02}.{:02}\n",
        systemInfo.MajorVersion, systemInfo.MinorVersion);

    std::wcout << std::format(L"Index               : {}\n", GetIndexStatusName(replayEngineView.GetIndexStatus()));
    std::cout << std::format("PID                 : 0x{:04X}\n", systemInfo.ProcessId);
    std::cout << std::format("PEB                 : 0x{:X}\n", replayEngineView.GetPebAddress());

    std::cout << std::format("Lifetime            : {}\n", replayEngineView.GetLifetime());

    std::cout << std::format("Threads             : {:>11}\n", replayEngineView.GetThreadCount());
    std::cout << std::format("Modules             : {:>11}\n", replayEngineView.GetModuleCount());
    std::cout << std::format("ModuleInstances     : {:>11}\n", replayEngineView.GetModuleInstanceCount());
    std::cout << std::format("Exceptions          : {:>11}\n", replayEngineView.GetExceptionEventCount());
    std::cout << std::format("Keyframes           : {:>11}\n", replayEngineView.GetKeyframeCount());

    // Print the system information
    // (see https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-system_info for more details)
    std::cout << std::format("System              :\n");

    std::cout << std::format("  OS                : {}.{}.{}\n",
        systemInfo.System.MajorVersion,
        systemInfo.System.MinorVersion,
        systemInfo.System.BuildNumber);

    std::cout << std::format("  Product Type      : {}\n", systemInfo.System.ProductType);
    std::cout << std::format("  Suite Mask        : {}\n", systemInfo.System.SuiteMask);
    std::cout << std::format("  Processors        : {}\n", systemInfo.System.NumberOfProcessors);
    std::cout << std::format("  Platform ID       : {}\n", systemInfo.System.PlatformId);
    std::cout << std::format("  Processsor Level  : {}\n", systemInfo.System.ProcessorLevel);
    std::cout << std::format("  Processor Revision: {}\n", systemInfo.System.ProcessorRevision);

    // If the live recorder was used, print the recording information (see TTDLiveRecorder.h for more details)
    if (replayEngineView.GetRecordClientCount() > 0) {
        std::cout << std::format("Record clients      : {:>11}\n", replayEngineView.GetRecordClientCount());
        std::cout << std::format("Custom events       : {:>11}\n", replayEngineView.GetCustomEventCount());
        std::cout << std::format("Activities          : {:>11}\n", replayEngineView.GetActivityCount());
        std::cout << std::format("Islands             : {:>11}\n", replayEngineView.GetIslandCount());
    }

    // Print the module list, showing when and where each module was loaded
    //std::cout << std::format("Module loading activity:\n");
    //for (ModuleInstance const& moduleInstance : ModuleInstances(&replayEngineView)) {
    //    std::wcout << std::format(L"[{}]  0x{:016X} 0x{:08X} {}\n",
    //        PositionRange(Position(moduleInstance.LoadTime), Position(moduleInstance.UnloadTime)),
    //        moduleInstance.pModule->Address,
    //        moduleInstance.pModule->Size,
    //        ModuleName(*moduleInstance.pModule));
    //}

    // Print the exception list with position as the first column
    std::cout << std::format("Exceptions:\n");
    for (ExceptionEvent const& exceptionInfo : ExceptionEvents(&replayEngineView)) {
        PrintException(exceptionInfo);
    }
}


int ttdproc_(int argc, char* argv[]) {
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





#if 0
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

// TtdDap - A Time Travel Debug Adapter Protocol (DAP) server for TTD trace files.
//
// This application implements the Debug Adapter Protocol to provide debugging
// capabilities for TTD trace files through VS Code and other DAP-compatible editors.
//
// Usage: TtdDap.exe
//   Starts the DAP server, communicating through stdin/stdout using JSON-RPC

#include <assert.h>
#define DBG_ASSERT(cond) assert(cond)

// TTD headers
#include <Formatters.h>
#include <ReplayHelpers.h>
#include <RegisterNameMapping.h>
#include <TTD/IReplayEngine.h>
#include <TTD/IReplayEngineStl.h>
#include <TTD/IReplayEngineRegisters.h>
#include <TTD/ErrorReporting.h>
// Standard C++ headers
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <stdexcept>

// JSON library - we'll use a simple JSON implementation
#include "json.hpp"
#include "DapProtocol.h"

using namespace TTD;
using namespace Replay;
using json = nlohmann::json;

// Simple error reporting class that prints errors to the console
class BasicErrorReporting : public ErrorReporting::IErrorReporting
{
public:
    virtual void ReportError(_In_ wchar_t const* pszError) noexcept override
    {
        std::wcerr << L"Error: " << pszError << L'\n';
    }
};

class TtdDapServer
{
public:
    TtdDapServer();
    ~TtdDapServer();

    int Run();

private:
    void ProcessMessage(const json& message);
    void SendMessage(const json& message);

    // DAP request handlers
    void HandleInitialize(const json& request);
    void HandleLaunch(const json& request);
    void HandleAttach(const json& request);
    void HandleConfigurationDone(const json& request);
    void HandleDisconnect(const json& request);
    void HandleTerminate(const json& request);
    void HandleSetBreakpoints(const json& request);
    void HandleContinue(const json& request);
    void HandleNext(const json& request);
    void HandleStepIn(const json& request);
    void HandleStepOut(const json& request);
    void HandleStepBack(const json& request);
    void HandlePause(const json& request);
    void HandleStackTrace(const json& request);
    void HandleScopes(const json& request);
    void HandleVariables(const json& request);
    void HandleEvaluate(const json& request);
    void HandleThreads(const json& request);

    // Helper methods
    void SendResponse(const json& request, const json& body = nullptr);
    void SendErrorResponse(const json& request, const std::string& message);
    void SendEvent(const std::string& event, const json& body = nullptr);
    void LoadTraceFile(const std::string& traceFile);

    // TTD state
    UniqueReplayEngine m_replayEngine;
    UniqueCursor m_cursor;
    std::string m_currentTraceFile;
    bool m_isInitialized = false;
    bool m_isLaunched = false;
    bool m_stopOnEntry = true;
    int m_nextSequence = 1;
};

TtdDapServer::TtdDapServer()
{
}

TtdDapServer::~TtdDapServer()
{
}

int TtdDapServer::Run()
{
    std::string line;

    while (std::getline(std::cin, line))
    {
        // DAP uses Content-Length header followed by JSON message
        if (line.find("Content-Length:") == 0)
        {
            // Extract content length
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos)
            {
                std::string lengthStr = line.substr(colonPos + 1);
                lengthStr.erase(0, lengthStr.find_first_not_of(" \t"));
                lengthStr.erase(lengthStr.find_last_not_of(" \t\r\n") + 1);

                int contentLength = std::stoi(lengthStr);

                // Skip empty line
                std::getline(std::cin, line);

                // Read JSON content
                std::string jsonContent(contentLength, '\0');
                std::cin.read(jsonContent.data(), contentLength);

                try
                {
                    json message = json::parse(jsonContent);
                    ProcessMessage(message);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "JSON parse error: " << e.what() << std::endl;
                }
            }
        }
    }

    return 0;
}

void TtdDapServer::ProcessMessage(const json& message)
{
    std::string type = message.value("type", "");

    if (type == "request")
    {
        std::string command = message.value("command", "");

        if (command == "initialize")
            HandleInitialize(message);
        else if (command == "launch")
            HandleLaunch(message);
        else if (command == "attach")
            HandleAttach(message);
        else if (command == "configurationDone")
            HandleConfigurationDone(message);
        else if (command == "disconnect")
            HandleDisconnect(message);
        else if (command == "terminate")
            HandleTerminate(message);
        else if (command == "setBreakpoints")
            HandleSetBreakpoints(message);
        else if (command == "continue")
            HandleContinue(message);
        else if (command == "next")
            HandleNext(message);
        else if (command == "stepIn")
            HandleStepIn(message);
        else if (command == "stepOut")
            HandleStepOut(message);
        else if (command == "stepBack")
            HandleStepBack(message);
        else if (command == "pause")
            HandlePause(message);
        else if (command == "stackTrace")
            HandleStackTrace(message);
        else if (command == "scopes")
            HandleScopes(message);
        else if (command == "variables")
            HandleVariables(message);
        else if (command == "evaluate")
            HandleEvaluate(message);
        else if (command == "threads")
            HandleThreads(message);
        else
        {
            SendErrorResponse(message, "Unknown command: " + command);
        }
    }
}

void TtdDapServer::SendMessage(const json& message)
{
    std::string content = message.dump();
    std::cout << "Content-Length: " << content.length() << "\r\n\r\n" << content << std::flush;
}

void TtdDapServer::HandleInitialize(const json& request)
{
    json response = {
        {"seq", m_nextSequence++},
        {"type", "response"},
        {"request_seq", request["seq"]},
        {"success", true},
        {"command", "initialize"},
        {"body", {
            {"supportsConfigurationDoneRequest", true},
            {"supportsEvaluateForHovers", true},
            {"supportsStepBack", true},
            {"supportsGotoTargetsRequest", false},
            {"supportsCompletionsRequest", false}
        }}
    };

    SendMessage(response);

    m_isInitialized = true;

    // Send initialized event
    SendEvent("initialized");
}

void TtdDapServer::HandleLaunch(const json& request)
{
    auto args = request.value("arguments", json::object());
    std::string traceFile = args.value("program", "");
    m_stopOnEntry = args.value("stopOnEntry", true);

    if (traceFile.empty())
    {
        SendErrorResponse(request, "No trace file specified in 'program' argument");
        return;
    }

    try
    {
        LoadTraceFile(traceFile);
        m_isLaunched = true;

        SendResponse(request);

        // If stopOnEntry is true, send a stopped event
        if (m_stopOnEntry)
        {
            SendEvent("stopped", { {"reason", "entry"}, {"threadId", 1} });
        }
    }
    catch (const std::exception& e)
    {
        SendErrorResponse(request, std::format("Failed to load trace file: {}", e.what()));
    }
}

void TtdDapServer::LoadTraceFile(const std::string& traceFile)
{
    // Create replay engine
    auto [pOwnedReplayEngine, createResult] = TTD::Replay::MakeReplayEngine();
    if (createResult != 0 || pOwnedReplayEngine == nullptr)
    {
        throw std::runtime_error(std::format("Failed to create replay engine: 0x{:x}", createResult));
    }

    m_replayEngine = std::move(pOwnedReplayEngine);

    // Set up error reporting
    BasicErrorReporting errorReporting;
    m_replayEngine->RegisterDebugModeAndLogging(DebugModeType::None, &errorReporting);

    // Load trace file
    std::wstring wideTraceFile(traceFile.begin(), traceFile.end());
    HRESULT hr = m_replayEngine->OpenTraceFile(wideTraceFile.c_str());
    if (FAILED(hr))
    {
        throw std::runtime_error(std::format("Failed to open trace file: 0x{:x}", hr));
    }

    // Create cursor
    m_cursor = TTD::Replay::UniqueCursor{ m_replayEngine->NewCursor() };
    if (!m_cursor)
    {
        throw std::runtime_error("Failed to create cursor");
    }

    // Set cursor to start of trace
    m_cursor->SetPosition(Position::Min);

    m_currentTraceFile = traceFile;
}

void TtdDapServer::HandleAttach(const json& request)
{
    SendErrorResponse(request, "Attach not supported for TTD traces");
}

void TtdDapServer::HandleConfigurationDone(const json& request)
{
    SendResponse(request);
    // Configuration is complete, ready for debugging
}

void TtdDapServer::HandleDisconnect(const json& request)
{
    SendResponse(request);
    exit(0);
}

void TtdDapServer::HandleTerminate(const json& request)
{
    SendResponse(request);
    exit(0);
}

void TtdDapServer::HandleSetBreakpoints(const json& request)
{
    // For now, just acknowledge breakpoints without setting them
    // TODO: Implement actual breakpoint support
    SendResponse(request, { {"breakpoints", json::array()} });
}

void TtdDapServer::HandleContinue(const json& request)
{
    if (!m_cursor)
    {
        SendErrorResponse(request, "No trace loaded");
        return;
    }

    // For TTD, continue means step to the end
    m_cursor->SetPosition(Position::Max);

    SendResponse(request, { {"allThreadsContinued", true} });
    SendEvent("stopped", { {"reason", "end"}, {"threadId", 1} });
}

void TtdDapServer::HandleNext(const json& request)
{
    if (!m_cursor)
    {
        SendErrorResponse(request, "No trace loaded");
        return;
    }

    m_cursor->ReplayForward(StepCount{ 1 });

    SendResponse(request);
    SendEvent("stopped", { {"reason", "step"}, {"threadId", 1} });
}

void TtdDapServer::HandleStepIn(const json& request)
{
    HandleNext(request); // For simplicity, treat as next for now
}

void TtdDapServer::HandleStepOut(const json& request)
{
    HandleNext(request); // For simplicity, treat as next for now
}

void TtdDapServer::HandleStepBack(const json& request)
{
    if (!m_cursor)
    {
        SendErrorResponse(request, "No trace loaded");
        return;
    }

    m_cursor->ReplayBackward(StepCount{ 1 });

    SendResponse(request);
    SendEvent("stopped", { {"reason", "step"}, {"threadId", 1} });
}

void TtdDapServer::HandlePause(const json& request)
{
    SendResponse(request);
    SendEvent("stopped", { {"reason", "pause"}, {"threadId", 1} });
}

void TtdDapServer::HandleStackTrace(const json& request)
{
    if (!m_cursor)
    {
        SendErrorResponse(request, "No trace loaded");
        return;
    }

    // Get current position and instruction pointer
    auto position = m_cursor->GetPosition();
    auto pc = m_cursor->GetProgramCounter();

    json stackFrames = json::array();
    stackFrames.push_back({
        {"id", 1},
        {"name", "main"},
        {"source", {{"name", "trace"}, {"path", m_currentTraceFile}}},
        {"line", 1},
        {"column", 1}
        });

    SendResponse(request, {
        {"stackFrames", stackFrames},
        {"totalFrames", 1}
        });
}

void TtdDapServer::HandleScopes(const json& request)
{
    json scopes = json::array();
    scopes.push_back({
        {"name", "Registers"},
        {"variablesReference", 1},
        {"expensive", false}
        });

    SendResponse(request, { {"scopes", scopes} });
}

void TtdDapServer::HandleVariables(const json& request)
{
    if (!m_cursor)
    {
        SendErrorResponse(request, "No trace loaded");
        return;
    }

    json variables = json::array();

    // Add register values and trace information
    try
    {
        auto position = m_cursor->GetPosition();
        auto pc = m_cursor->GetProgramCounter();

        variables.push_back({
            {"name", "Program Counter"},
            {"value", std::format("0x{:x}", pc)},
            {"type", "address"},
            {"variablesReference", 0}
            });

        variables.push_back({
            {"name", "Position"},
            {"value", std::format("{}:{}", position.Sequence, position.Steps)},
            {"type", "position"},
            {"variablesReference", 0}
            });

        // Try to get some basic register context
        try
        {
            auto registers = m_cursor->GetCrossPlatformContext();

            variables.push_back({
                {"name", "Stack Pointer"},
                {"value", std::format("0x{:x}", registers.Rsp)},
                {"type", "address"},
                {"variablesReference", 0}
                });

            variables.push_back({
                {"name", "RAX"},
                {"value", std::format("0x{:x}", registers.Rax)},
                {"type", "register"},
                {"variablesReference", 0}
                });
        }
        catch (...)
        {
            variables.push_back({
                {"name", "Registers"},
                {"value", "Error reading register context"},
                {"variablesReference", 0}
                });
        }
    }
    catch (...)
    {
        variables.push_back({
            {"name", "Error"},
            {"value", "Failed to read trace information"},
            {"variablesReference", 0}
            });
    }

    SendResponse(request, { {"variables", variables} });
}

void TtdDapServer::HandleEvaluate(const json& request)
{
    SendErrorResponse(request, "Expression evaluation not yet implemented");
}

void TtdDapServer::HandleThreads(const json& request)
{
    json threads = json::array();
    threads.push_back({
        {"id", 1},
        {"name", "Main Thread"}
        });

    SendResponse(request, { {"threads", threads} });
}

void TtdDapServer::SendResponse(const json& request, const json& body)
{
    json response = {
        {"seq", m_nextSequence++},
        {"type", "response"},
        {"request_seq", request["seq"]},
        {"success", true},
        {"command", request["command"]}
    };

    if (body.is_object() || body.is_array())
    {
        response["body"] = body;
    }

    SendMessage(response);
}

void TtdDapServer::SendErrorResponse(const json& request, const std::string& message)
{
    json response = {
        {"seq", m_nextSequence++},
        {"type", "response"},
        {"request_seq", request["seq"]},
        {"success", false},
        {"command", request["command"]},
        {"message", message}
    };

    SendMessage(response);
}

void TtdDapServer::SendEvent(const std::string& event, const json& body)
{
    json eventMsg = {
        {"seq", m_nextSequence++},
        {"type", "event"},
        {"event", event}
    };

    if (body.is_object() || body.is_array())
    {
        eventMsg["body"] = body;
    }

    SendMessage(eventMsg);
}

int main(int argc, char* argv[])
{
    try
    {
        TtdDapServer server;
        return server.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}


#endif