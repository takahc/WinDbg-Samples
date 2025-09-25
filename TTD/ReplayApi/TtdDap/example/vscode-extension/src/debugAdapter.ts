// VS Code Debug Adapter wrapper for TtdDap.exe
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

import { spawn, ChildProcess } from 'child_process';
import { DebugSession, LoggingDebugSession, InitializedEvent, TerminatedEvent, StoppedEvent, OutputEvent, Thread, StackFrame, Scope, Variable, Breakpoint } from 'vscode-debugadapter';
import { DebugProtocol } from 'vscode-debugprotocol';
import * as path from 'path';

interface LaunchRequestArguments extends DebugProtocol.LaunchRequestArguments {
    program: string;
    stopOnEntry?: boolean;
}

export class TtdDebugSession extends LoggingDebugSession {
    private ttdProcess: ChildProcess | null = null;
    private isConfigurationDone = false;

    public constructor() {
        super("ttd-debug.txt");
        this.setDebuggerLinesStartAt1(false);
        this.setDebuggerColumnsStartAt1(false);
    }

    protected initializeRequest(response: DebugProtocol.InitializeResponse, args: DebugProtocol.InitializeRequestArguments): void {
        response.body = response.body || {};
        
        // Capabilities supported by this adapter
        response.body.supportsConfigurationDoneRequest = true;
        response.body.supportsEvaluateForHovers = true;
        response.body.supportsStepBack = true;
        response.body.supportsGotoTargetsRequest = false;
        response.body.supportsCompletionsRequest = false;

        this.sendResponse(response);
        this.sendEvent(new InitializedEvent());
    }

    protected async launchRequest(response: DebugProtocol.LaunchResponse, args: LaunchRequestArguments): Promise<void> {
        try {
            // Find TtdDap.exe (assumes it's in the same directory or in PATH)
            const ttdDapPath = this.findTtdDapExecutable();
            
            // Launch TtdDap.exe process
            this.ttdProcess = spawn(ttdDapPath, [], {
                stdio: ['pipe', 'pipe', 'pipe']
            });

            if (!this.ttdProcess.stdin || !this.ttdProcess.stdout) {
                throw new Error('Failed to create TTD process pipes');
            }

            // Set up communication with TtdDap
            this.setupTtdCommunication();

            // Send initialize request to TtdDap
            const initRequest = {
                seq: 1,
                type: 'request',
                command: 'initialize',
                arguments: {
                    clientID: 'vscode',
                    adapterID: 'ttd'
                }
            };

            this.sendToTtd(initRequest);

            // Send launch request to TtdDap
            const launchRequest = {
                seq: 2,
                type: 'request',
                command: 'launch',
                arguments: {
                    program: args.program,
                    stopOnEntry: args.stopOnEntry
                }
            };

            this.sendToTtd(launchRequest);

            this.sendResponse(response);

        } catch (error) {
            this.sendErrorResponse(response, 2001, `Failed to launch TTD: ${error}`);
        }
    }

    protected configurationDoneRequest(response: DebugProtocol.ConfigurationDoneResponse, args: DebugProtocol.ConfigurationDoneArguments): void {
        super.configurationDoneRequest(response, args);
        this.isConfigurationDone = true;
    }

    protected disconnectRequest(response: DebugProtocol.DisconnectResponse, args: DebugProtocol.DisconnectArguments): void {
        if (this.ttdProcess) {
            this.ttdProcess.kill();
            this.ttdProcess = null;
        }
        this.sendResponse(response);
    }

    // Forward debugging requests to TtdDap
    protected continueRequest(response: DebugProtocol.ContinueResponse, args: DebugProtocol.ContinueArguments): void {
        this.forwardRequest('continue', response, args);
    }

    protected nextRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
        this.forwardRequest('next', response, args);
    }

    protected stepInRequest(response: DebugProtocol.StepInResponse, args: DebugProtocol.StepInArguments): void {
        this.forwardRequest('stepIn', response, args);
    }

    protected stepOutRequest(response: DebugProtocol.StepOutResponse, args: DebugProtocol.StepOutArguments): void {
        this.forwardRequest('stepOut', response, args);
    }

    protected stackTraceRequest(response: DebugProtocol.StackTraceResponse, args: DebugProtocol.StackTraceArguments): void {
        this.forwardRequest('stackTrace', response, args);
    }

    protected scopesRequest(response: DebugProtocol.ScopesResponse, args: DebugProtocol.ScopesArguments): void {
        this.forwardRequest('scopes', response, args);
    }

    protected variablesRequest(response: DebugProtocol.VariablesResponse, args: DebugProtocol.VariablesArguments): void {
        this.forwardRequest('variables', response, args);
    }

    protected threadsRequest(response: DebugProtocol.ThreadsResponse): void {
        this.forwardRequest('threads', response, {});
    }

    private findTtdDapExecutable(): string {
        // Try to find TtdDap.exe in the extension directory or PATH
        const extensionDir = path.dirname(path.dirname(__dirname));
        const localPath = path.join(extensionDir, 'TtdDap.exe');
        
        // For development, assume it's built and available
        return localPath;
    }

    private setupTtdCommunication(): void {
        if (!this.ttdProcess || !this.ttdProcess.stdout) {
            return;
        }

        let buffer = '';
        
        this.ttdProcess.stdout.on('data', (data: Buffer) => {
            buffer += data.toString();
            
            // Process complete DAP messages
            while (true) {
                const lengthMatch = buffer.match(/Content-Length: (\d+)\r?\n\r?\n/);
                if (!lengthMatch) {
                    break;
                }
                
                const headerLength = lengthMatch[0].length;
                const contentLength = parseInt(lengthMatch[1], 10);
                const totalLength = headerLength + contentLength;
                
                if (buffer.length < totalLength) {
                    break;
                }
                
                const messageContent = buffer.substr(headerLength, contentLength);
                buffer = buffer.substr(totalLength);
                
                try {
                    const message = JSON.parse(messageContent);
                    this.handleTtdMessage(message);
                } catch (error) {
                    this.sendEvent(new OutputEvent(`Error parsing message: ${error}\n`));
                }
            }
        });

        this.ttdProcess.on('exit', (code) => {
            this.sendEvent(new TerminatedEvent());
        });
    }

    private sendToTtd(message: any): void {
        if (!this.ttdProcess || !this.ttdProcess.stdin) {
            return;
        }

        const content = JSON.stringify(message);
        const header = `Content-Length: ${content.length}\r\n\r\n`;
        
        this.ttdProcess.stdin.write(header + content);
    }

    private handleTtdMessage(message: any): void {
        if (message.type === 'event') {
            // Forward events from TtdDap to VS Code
            switch (message.event) {
                case 'initialized':
                    // Already sent during initialize
                    break;
                case 'stopped':
                    this.sendEvent(new StoppedEvent(message.body.reason, message.body.threadId));
                    break;
                case 'terminated':
                    this.sendEvent(new TerminatedEvent());
                    break;
            }
        }
        // Response handling would be more complex in a full implementation
        // For now, we assume synchronous request/response
    }

    private forwardRequest(command: string, response: DebugProtocol.Response, args: any): void {
        const request = {
            seq: Date.now(), // Simple sequence number
            type: 'request',
            command: command,
            arguments: args
        };

        this.sendToTtd(request);
        
        // In a full implementation, we would wait for the response and forward it
        // For now, just send a basic success response
        this.sendResponse(response);
    }
}

// Entry point for the debug adapter
export function main() {
    DebugSession.run(TtdDebugSession);
}