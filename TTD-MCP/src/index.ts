#!/usr/bin/env node

/**
 * TTD-MCP Server
 * 
 * A Model Context Protocol (MCP) server that provides Time Travel Debugging (TTD) 
 * query tools based on the TTDQueries functionality from WinDbg-Samples.
 * 
 * This server exposes TTD debugging capabilities as MCP tools, allowing AI assistants
 * and other MCP clients to query TTD traces, analyze function calls, memory access
 * patterns, and navigate through time travel debugging sessions.
 */

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import {
  CallToolRequestSchema,
  ErrorCode,
  ListToolsRequestSchema,
  McpError,
} from '@modelcontextprotocol/sdk/types.js';

// Import tool definitions and handlers
import { functionCallsTool, handleFunctionCallsQuery } from './tools/function-calls.js';
import { memoryAccessTool, handleMemoryAccessQuery } from './tools/memory-access.js';
import { positionNavigationTool, handlePositionNavigation } from './tools/position-navigation.js';
import { registerInspectionTool, handleRegisterInspection } from './tools/register-inspection.js';
import { errorAnalysisTool, handleErrorAnalysis } from './tools/error-analysis.js';
import { traceInfoTool, handleTraceInfo } from './tools/trace-info.js';

/**
 * TTD-MCP Server class
 * Implements the MCP server protocol and handles TTD-related tool calls
 */
class TTDMcpServer {
  private server: Server;

  constructor() {
    this.server = new Server(
      {
        name: 'ttd-mcp-server',
        version: '1.0.0',
      },
      {
        capabilities: {
          tools: {},
        },
      }
    );

    this.setupToolHandlers();
    this.setupErrorHandling();
  }

  /**
   * Setup tool handlers for all TTD tools
   */
  private setupToolHandlers(): void {
    // List available tools
    this.server.setRequestHandler(ListToolsRequestSchema, async () => {
      return {
        tools: [
          functionCallsTool,
          memoryAccessTool,
          positionNavigationTool,
          registerInspectionTool,
          errorAnalysisTool,
          traceInfoTool,
        ],
      };
    });

    // Handle tool calls
    this.server.setRequestHandler(CallToolRequestSchema, async (request) => {
      const { name, arguments: args } = request.params;

      try {
        switch (name) {
          case 'ttd_query_calls':
            return {
              content: [
                {
                  type: 'text',
                  text: JSON.stringify(await handleFunctionCallsQuery(args || {}), null, 2),
                },
              ],
            };

          case 'ttd_query_memory':
            return {
              content: [
                {
                  type: 'text',
                  text: JSON.stringify(await handleMemoryAccessQuery(args || {}), null, 2),
                },
              ],
            };

          case 'ttd_navigate_position':
            return {
              content: [
                {
                  type: 'text',
                  text: JSON.stringify(await handlePositionNavigation(args || {}), null, 2),
                },
              ],
            };

          case 'ttd_inspect_registers':
            return {
              content: [
                {
                  type: 'text',
                  text: JSON.stringify(await handleRegisterInspection(args || {}), null, 2),
                },
              ],
            };

          case 'ttd_analyze_errors':
            return {
              content: [
                {
                  type: 'text',
                  text: JSON.stringify(await handleErrorAnalysis(args || {}), null, 2),
                },
              ],
            };

          case 'ttd_trace_info':
            return {
              content: [
                {
                  type: 'text',
                  text: JSON.stringify(await handleTraceInfo(args || {}), null, 2),
                },
              ],
            };

          default:
            throw new McpError(
              ErrorCode.MethodNotFound,
              `Unknown tool: ${name}`
            );
        }
      } catch (error) {
        const errorMessage = error instanceof Error ? error.message : 'Unknown error occurred';
        
        if (error instanceof McpError) {
          throw error;
        }

        throw new McpError(
          ErrorCode.InternalError,
          `Tool execution failed: ${errorMessage}`
        );
      }
    });
  }

  /**
   * Setup error handling for the server
   */
  private setupErrorHandling(): void {
    this.server.onerror = (error) => {
      console.error('[TTD-MCP Server Error]', error);
    };

    process.on('SIGINT', async () => {
      await this.server.close();
      process.exit(0);
    });
  }

  /**
   * Start the MCP server
   */
  async start(): Promise<void> {
    const transport = new StdioServerTransport();
    await this.server.connect(transport);
    
    console.error('TTD-MCP Server started. Ready to handle TTD debugging requests.');
  }
}

/**
 * Main entry point
 */
async function main(): Promise<void> {
  const server = new TTDMcpServer();
  await server.start();
}

// Handle unhandled promise rejections
process.on('unhandledRejection', (reason, promise) => {
  console.error('Unhandled Rejection at:', promise, 'reason:', reason);
  process.exit(1);
});

// Start the server
if (import.meta.url === `file://${process.argv[1]}`) {
  main().catch((error) => {
    console.error('Failed to start TTD-MCP Server:', error);
    process.exit(1);
  });
}