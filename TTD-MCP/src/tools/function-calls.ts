/**
 * TTD Function Call Query Tool
 * Implements MCP tool for querying function calls in TTD traces
 */

import { Tool } from '@modelcontextprotocol/sdk/types.js';
import { WinDbgExecutor } from '../utils/windbg-executor.js';

export const functionCallsTool: Tool = {
  name: 'ttd_query_calls',
  description: 'Query function calls in TTD traces with optional filtering. Useful for finding specific API calls, debugging call patterns, or analyzing execution flow.',
  inputSchema: {
    type: 'object',
    properties: {
      function_name: {
        type: 'string',
        description: 'Name of function to search for (e.g., "user32!MessageBoxW", "kernel32!CreateFileW", "ntdll!NtCreateFile")'
      },
      trace_file: {
        type: 'string',
        description: 'Path to TTD trace file (.ttd). If not provided, uses the currently loaded trace in WinDbg.'
      },
      filter_expression: {
        type: 'string',
        description: 'Additional LINQ-style filtering expression (e.g., "x => x.ReturnValue != 0" to filter out successful calls)'
      },
      order_by: {
        type: 'string',
        description: 'How to order results',
        enum: ['TimeStart', 'TimeEnd', 'ThreadId', 'ReturnValue']
      },
      limit: {
        type: 'number',
        description: 'Maximum number of results to return (default: 100)',
        minimum: 1,
        maximum: 10000
      }
    },
    required: ['function_name']
  }
};

export async function handleFunctionCallsQuery(args: any): Promise<any> {
  const { function_name, trace_file, filter_expression, order_by, limit = 100 } = args;
  
  const executor = new WinDbgExecutor();
  
  // Validate trace file if provided
  if (trace_file && !(await executor.validateTraceFile(trace_file))) {
    throw new Error(`Invalid or inaccessible trace file: ${trace_file}`);
  }

  try {
    // Build the query command
    const query = executor.buildFunctionCallQuery(function_name, {
      filterExpression: filter_expression,
      orderBy: order_by,
      limit
    });

    // Execute the command
    const output = trace_file 
      ? await executor.executeCommand(trace_file, query)
      : await executor.executeCommand('', query); // Use currently loaded trace

    // Parse the results
    const functionCalls = executor.parseFunctionCalls(output);

    return {
      function_calls: functionCalls,
      total_found: functionCalls.length,
      query_executed: query,
      trace_file: trace_file || 'current',
      summary: {
        function_name,
        total_calls: functionCalls.length,
        unique_threads: [...new Set(functionCalls.map(c => c.threadId))].length,
        time_range: functionCalls.length > 0 ? {
          first_call: functionCalls[0]?.timeStart?.formatted,
          last_call: functionCalls[functionCalls.length - 1]?.timeStart?.formatted
        } : null
      }
    };
  } catch (error) {
    throw new Error(`Failed to query function calls: ${error instanceof Error ? error.message : 'Unknown error'}`);
  }
}

/**
 * Examples of common function call queries:
 * 
 * 1. Find all MessageBox calls:
 *    function_name: "user32!MessageBoxW"
 * 
 * 2. Find failed file operations:
 *    function_name: "kernel32!CreateFileW" 
 *    filter_expression: "x => x.ReturnValue == 0xFFFFFFFF"
 * 
 * 3. Find recent API calls:
 *    function_name: "kernel32!GetLastError"
 *    order_by: "TimeStart"
 *    limit: 10
 * 
 * 4. Find calls in specific thread:
 *    function_name: "ntdll!NtCreateFile"
 *    filter_expression: "x => x.ThreadId == '0x6c4'"
 */