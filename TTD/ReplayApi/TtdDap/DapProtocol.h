// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>

namespace Dap
{
    // DAP Protocol structures
    
    struct Source
    {
        std::optional<std::string> name;
        std::optional<std::string> path;
        std::optional<int> sourceReference;
    };
    
    struct SourceBreakpoint
    {
        int line;
        std::optional<int> column;
        std::optional<std::string> condition;
        std::optional<std::string> hitCondition;
        std::optional<std::string> logMessage;
    };
    
    struct Breakpoint
    {
        std::optional<int> id;
        bool verified;
        std::optional<std::string> message;
        std::optional<Source> source;
        std::optional<int> line;
        std::optional<int> column;
    };
    
    struct StackFrame
    {
        int id;
        std::string name;
        std::optional<Source> source;
        int line;
        int column;
        std::optional<int> endLine;
        std::optional<int> endColumn;
        std::optional<bool> canRestart;
        std::optional<int> instructionPointerReference;
    };
    
    struct Thread
    {
        int id;
        std::string name;
    };
    
    struct Scope
    {
        std::string name;
        int variablesReference;
        std::optional<int> namedVariables;
        std::optional<int> indexedVariables;
        bool expensive;
        std::optional<Source> source;
        std::optional<int> line;
        std::optional<int> column;
        std::optional<int> endLine;
        std::optional<int> endColumn;
    };
    
    struct Variable
    {
        std::string name;
        std::string value;
        std::optional<std::string> type;
        std::optional<int> variablesReference;
        std::optional<int> namedVariables;
        std::optional<int> indexedVariables;
        std::optional<int> memoryReference;
    };
    
    // DAP Request/Response types
    
    struct InitializeRequestArguments
    {
        std::string clientID;
        std::optional<std::string> clientName;
        std::string adapterID;
        std::optional<std::string> locale;
        std::optional<bool> linesStartAt1;
        std::optional<bool> columnsStartAt1;
        std::optional<std::string> pathFormat;
        std::optional<bool> supportsVariableType;
        std::optional<bool> supportsVariablePaging;
        std::optional<bool> supportsRunInTerminalRequest;
        std::optional<bool> supportsMemoryReferences;
        std::optional<bool> supportsProgressReporting;
        std::optional<bool> supportsInvalidatedEvent;
    };
    
    struct LaunchRequestArguments
    {
        std::optional<bool> noDebug;
        std::optional<std::string> program;
        std::optional<std::vector<std::string>> args;
        std::optional<std::string> cwd;
        std::optional<std::map<std::string, std::string>> env;
        std::optional<bool> stopOnEntry;
        std::optional<bool> console;
    };
    
    struct SetBreakpointsArguments
    {
        Source source;
        std::optional<std::vector<SourceBreakpoint>> breakpoints;
        std::optional<bool> sourceModified;
    };
    
    struct ContinueArguments
    {
        int threadId;
        std::optional<bool> singleThread;
    };
    
    struct NextArguments
    {
        int threadId;
        std::optional<bool> singleThread;
        std::optional<std::string> granularity;
    };
    
    struct StepInArguments
    {
        int threadId;
        std::optional<bool> singleThread;
        std::optional<int> targetId;
        std::optional<std::string> granularity;
    };
    
    struct StepOutArguments
    {
        int threadId;
        std::optional<bool> singleThread;
        std::optional<std::string> granularity;
    };
    
    struct StackTraceArguments
    {
        int threadId;
        std::optional<int> startFrame;
        std::optional<int> levels;
        std::optional<std::string> format;
    };
    
    struct ScopesArguments
    {
        int frameId;
    };
    
    struct VariablesArguments
    {
        int variablesReference;
        std::optional<std::string> filter;
        std::optional<int> start;
        std::optional<int> count;
        std::optional<std::string> format;
    };
    
    struct EvaluateArguments
    {
        std::string expression;
        std::optional<int> frameId;
        std::optional<std::string> context;
        std::optional<std::string> format;
    };
    
    struct ThreadsArguments
    {
        // No arguments for threads request
    };
}