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
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <stdexcept>
#include <chrono>
#include <iomanip>

#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

// JSON library - we'll use a simple JSON implementation
#include "json.hpp"
#include "DapProtocol.h"

using namespace TTD;
using namespace Replay;
using json = nlohmann::json;

// Simple error reporting class that prints errors to the console
class BasicErrorReporting : public ErrorReporting
{
public:
    BasicErrorReporting() = default;

    void __fastcall VPrintError(_Printf_format_string_ char const *const pFmt, _In_ va_list argList) override
    {
        char buffer[2048];

        vsprintf_s(buffer, pFmt, argList);

        std::cout << std::format("Error: {}\n", buffer);
    }
};

// File logger class for debugging and tracing DAP communication
class FileLogger
{
public:
    FileLogger(const std::string &logFilePath) : m_logFilePath(logFilePath)
    {
        m_logFile.open(m_logFilePath, std::ios::out | std::ios::app);
        if (m_logFile.is_open())
        {
            LogInfo("=== TTD DAP Server Started ===");
        }
    }

    ~FileLogger()
    {
        if (m_logFile.is_open())
        {
            LogInfo("=== TTD DAP Server Stopped ===");
            m_logFile.close();
        }
    }

    void LogInfo(const std::string &message)
    {
        LogMessage("INFO", message);
    }

    void LogError(const std::string &message)
    {
        LogMessage("ERROR", message);
    }

    void LogDebug(const std::string &message)
    {
        LogMessage("DEBUG", message);
    }

    void LogMessage(const std::string &level, const std::string &message)
    {
        if (!m_logFile.is_open())
            return;

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm;
        localtime_s(&tm, &time_t);

        m_logFile << std::format("[{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}] [{}] {}\n",
                                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                 tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count(),
                                 level, message);

        m_logFile.flush();
    }

    void LogJsonMessage(const std::string &direction, const json &message)
    {
        if (!m_logFile.is_open())
            return;

        std::string messageStr = message.dump(2); // Pretty print with 2-space indent
        LogMessage("JSON", std::format("{}: {}", direction, messageStr));
    }

private:
    std::string m_logFilePath;
    std::ofstream m_logFile;
};
class TtdDapServer
{
public:
    TtdDapServer();
    ~TtdDapServer();

    int Run();

private:
    void ProcessMessage(const json &message);
    void SendMessage(const json &message);

    // DAP request handlers
    void HandleInitialize(const json &request);
    void HandleLaunch(const json &request);
    void HandleAttach(const json &request);
    void HandleConfigurationDone(const json &request);
    void HandleDisconnect(const json &request);
    void HandleTerminate(const json &request);
    void HandleSetBreakpoints(const json &request);
    void HandleContinue(const json &request);
    void HandleNext(const json &request);
    void HandleStepIn(const json &request);
    void HandleStepOut(const json &request);
    void HandleStepBack(const json &request);
    void HandlePause(const json &request);
    void HandleStackTrace(const json &request);
    void HandleScopes(const json &request);
    void HandleVariables(const json &request);
    void HandleEvaluate(const json &request);
    void HandleThreads(const json &request);

    // Helper methods
    void SendResponse(const json &request, const json &body = nullptr);
    void SendErrorResponse(const json &request, const std::string &message);
    void SendEvent(const std::string &event, const json &body = nullptr);
    void LoadTraceFile(const std::string &traceFile);

    // TTD state
    UniqueReplayEngine m_replayEngine;
    UniqueCursor m_cursor;
    std::string m_currentTraceFile;
    bool m_isInitialized = false;
    bool m_isLaunched = false;
    bool m_stopOnEntry = true;
    int m_nextSequence = 1;

    // Logger
    std::unique_ptr<FileLogger> m_logger;
};

TtdDapServer::TtdDapServer()
{
    // Initialize logger with timestamp in filename
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time_t);

    std::string logFileName = std::format("ttd_dap_{:04}{:02}{:02}_{:02}{:02}{:02}.log",
                                          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                          tm.tm_hour, tm.tm_min, tm.tm_sec);

    m_logger = std::make_unique<FileLogger>(logFileName);
    m_logger->LogInfo("TtdDapServer constructed");
}

TtdDapServer::~TtdDapServer()
{
    if (m_logger)
    {
        m_logger->LogInfo("TtdDapServer destroyed");
    }
}

int TtdDapServer::Run()
{
    if (m_logger)
    {
        m_logger->LogInfo("DAP server started, waiting for messages...");
    }

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
                    if (m_logger)
                    {
                        m_logger->LogJsonMessage("RECEIVED", message);
                    }
                    ProcessMessage(message);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "JSON parse error: " << e.what() << std::endl;
                    if (m_logger)
                    {
                        m_logger->LogError(std::format("JSON parse error: {}", e.what()));
                    }
                }
            }
        }
    }

    return 0;
}

void TtdDapServer::ProcessMessage(const json &message)
{
    std::string type = message.value("type", "");

    if (m_logger)
    {
        std::string command = message.value("command", "unknown");
        m_logger->LogDebug(std::format("Processing message type: {}, command: {}", type, command));
    }

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

void TtdDapServer::SendMessage(const json &message)
{
    if (m_logger)
    {
        m_logger->LogJsonMessage("SENT", message);
    }

    std::string content = message.dump();
    std::string raw = "Content-Length: " + std::to_string(content.length()) + "\r\n\r\n" + content;
    // std::cout << "Content-Length: " << content.length() << "\r\n\r\n" << content << std::flush;

    // printf("%s", raw.c_str());
    // fflush(stdout);

    std::cout << raw << std::flush;

    // if(m_logger)
    //{
    //     m_logger->LogDebug(raw);
    // }
}

void TtdDapServer::HandleInitialize(const json &request)
{
    if (m_logger)
    {
        m_logger->LogInfo("Handling Initialize request");
    }

    json response = {
        {"seq", m_nextSequence++},
        {"type", "response"},
        {"request_seq", request["seq"]},
        {"success", true},
        {"command", "initialize"},
        {"body", {
                     {"supportsConfigurationDoneRequest", true}, {"supportsEvaluateForHovers", true}, {"supportsStepBack", true},
                     //{"supportsGotoTargetsRequest", false},
                     //{"supportsCompletionsRequest", false}
                 }}};

    SendMessage(response);

    m_isInitialized = true;

    // Send initialized event
    SendEvent("initialized");
}

void TtdDapServer::HandleLaunch(const json &request)
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
            SendEvent("stopped", {{"reason", "entry"}, {"threadId", 1}});
        }
    }
    catch (const std::exception &e)
    {
        SendErrorResponse(request, std::format("Failed to load trace file: {}", e.what()));
    }
}

void TtdDapServer::LoadTraceFile(const std::string &traceFile)
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
    HRESULT hr = m_replayEngine->Initialize(wideTraceFile.c_str());
    if (FAILED(hr))
    {
        throw std::runtime_error(std::format("Failed to open trace file: 0x{:x}", hr));
    }

    // Create cursor
    m_cursor = TTD::Replay::UniqueCursor{m_replayEngine->NewCursor()};
    if (!m_cursor)
    {
        throw std::runtime_error("Failed to create cursor");
    }

    // Set cursor to start of trace
    m_cursor->SetPosition(Position::Min);

    m_currentTraceFile = traceFile;
}

void TtdDapServer::HandleAttach(const json &request)
{
    SendErrorResponse(request, "Attach not supported for TTD traces");
}

void TtdDapServer::HandleConfigurationDone(const json &request)
{
    SendResponse(request);
    // Configuration is complete, ready for debugging
}

void TtdDapServer::HandleDisconnect(const json &request)
{
    SendResponse(request);
    exit(0);
}

void TtdDapServer::HandleTerminate(const json &request)
{
    SendResponse(request);
    exit(0);
}

void TtdDapServer::HandleSetBreakpoints(const json &request)
{
    // For now, just acknowledge breakpoints without setting them
    // TODO: Implement actual breakpoint support
    SendResponse(request, {{"breakpoints", json::array()}});
}

void TtdDapServer::HandleContinue(const json &request)
{
    if (!m_cursor)
    {
        SendErrorResponse(request, "No trace loaded");
        return;
    }

    // For TTD, continue means step to the end
    m_cursor->SetPosition(Position::Max);

    SendResponse(request, {{"allThreadsContinued", true}});
    SendEvent("stopped", {{"reason", "end"}, {"threadId", 1}});
}

void TtdDapServer::HandleNext(const json &request)
{
    if (!m_cursor)
    {
        SendErrorResponse(request, "No trace loaded");
        return;
    }

    m_cursor->ReplayForward(StepCount{1});

    SendResponse(request);
    SendEvent("stopped", {{"reason", "step"}, {"threadId", 1}});
}

void TtdDapServer::HandleStepIn(const json &request)
{
    HandleNext(request); // For simplicity, treat as next for now
}

void TtdDapServer::HandleStepOut(const json &request)
{
    HandleNext(request); // For simplicity, treat as next for now
}

void TtdDapServer::HandleStepBack(const json &request)
{
    if (!m_cursor)
    {
        SendErrorResponse(request, "No trace loaded");
        return;
    }

    m_cursor->ReplayBackward(StepCount{1});

    SendResponse(request);
    SendEvent("stopped", {{"reason", "step"}, {"threadId", 1}});
}

void TtdDapServer::HandlePause(const json &request)
{
    SendResponse(request);
    SendEvent("stopped", {{"reason", "pause"}, {"threadId", 1}});
}

void TtdDapServer::HandleStackTrace(const json &request)
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
    stackFrames.push_back({{"id", 1},
                           {"name", "main"},
                           {"source", {{"name", "trace"}, {"path", m_currentTraceFile}}},
                           {"line", 1},
                           {"column", 1}});

    SendResponse(request, {{"stackFrames", stackFrames},
                           {"totalFrames", 1}});
}

void TtdDapServer::HandleScopes(const json &request)
{
    json scopes = json::array();
    scopes.push_back({{"name", "Registers"},
                      {"variablesReference", 1},
                      {"expensive", false}});

    SendResponse(request, {{"scopes", scopes}});
}

void TtdDapServer::HandleVariables(const json &request)
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

        variables.push_back({{"name", "Program Counter"},
                             {"value", std::format("0x{:x}", pc)},
                             {"type", "address"},
                             {"variablesReference", 0}});

        variables.push_back({{"name", "Position"},
                             {"value", std::format("{}:{}", position.Sequence, position.Steps)},
                             {"type", "position"},
                             {"variablesReference", 0}});

        // Try to get some basic register context
        try
        {
            auto registers = m_cursor->GetCrossPlatformContext();

            variables.push_back({{"name", "Stack Pointer"},
                                 {"value", std::format("0x{:x}", m_cursor->GetStackPointer())},
                                 {"type", "address"},
                                 {"variablesReference", 0}});

            variables.push_back({{"name", "RAX"},
                                 {"value", std::format("0x{:x}", 999)}, // TODO
                                 {"type", "register"},
                                 {"variablesReference", 0}});
        }
        catch (...)
        {
            variables.push_back({{"name", "Registers"},
                                 {"value", "Error reading register context"},
                                 {"variablesReference", 0}});
        }
    }
    catch (...)
    {
        variables.push_back({{"name", "Error"},
                             {"value", "Failed to read trace information"},
                             {"variablesReference", 0}});
    }

    SendResponse(request, {{"variables", variables}});
}

void TtdDapServer::HandleEvaluate(const json &request)
{
    SendErrorResponse(request, "Expression evaluation not yet implemented");
}

void TtdDapServer::HandleThreads(const json &request)
{
    json threads = json::array();
    threads.push_back({{"id", 1},
                       {"name", "Main Thread"}});

    SendResponse(request, {{"threads", threads}});
}

void TtdDapServer::SendResponse(const json &request, const json &body)
{
    json response = {
        {"seq", m_nextSequence++},
        {"type", "response"},
        {"request_seq", request["seq"]},
        {"success", true},
        {"command", request["command"]}};

    if (body.is_object() || body.is_array())
    {
        response["body"] = body;
    }

    SendMessage(response);
}

void TtdDapServer::SendErrorResponse(const json &request, const std::string &message)
{
    json response = {
        {"seq", m_nextSequence++},
        {"type", "response"},
        {"request_seq", request["seq"]},
        {"success", false},
        {"command", request["command"]},
        {"message", message}};

    SendMessage(response);
}

void TtdDapServer::SendEvent(const std::string &event, const json &body)
{
    json eventMsg = {
        {"seq", m_nextSequence++},
        {"type", "event"},
        {"event", event}};

    if (body.is_object() || body.is_array())
    {
        eventMsg["body"] = body;
    }

    SendMessage(eventMsg);
}

int main(int argc, char *argv[])
{
    _setmode(_fileno(stdout), _O_BINARY);

    try
    {
        TtdDapServer server;
        return server.Run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
