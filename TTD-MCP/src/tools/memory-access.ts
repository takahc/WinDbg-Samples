/**
 * TTD Memory Access Query Tool
 * Implements MCP tool for querying memory access patterns in TTD traces
 */

import { Tool } from '@modelcontextprotocol/sdk/types.js';
import { WinDbgExecutor } from '../utils/windbg-executor.js';

export const memoryAccessTool: Tool = {
  name: 'ttd_query_memory',
  description: 'Query memory access patterns in TTD traces. Useful for finding reads/writes to specific memory locations, debugging memory corruption, or analyzing data flow.',
  inputSchema: {
    type: 'object',
    properties: {
      address_range: {
        type: 'string',
        description: 'Memory address range to monitor. Can be single address (e.g., "0x7ffb578f1e30") or range (e.g., "0x7ffb578f1e30-0x7ffb578f1e40")'
      },
      access_type: {
        type: 'string',
        description: 'Type of memory access to monitor',
        enum: ['r', 'w', 'x', 'rw', 'rx', 'wx', 'rwx']
      },
      trace_file: {
        type: 'string',
        description: 'Path to TTD trace file (.ttd). If not provided, uses the currently loaded trace in WinDbg.'
      },
      time_range: {
        type: 'string',
        description: 'Time range to search within (e.g., "306600:5BF-306700:1A0"). If not provided, searches entire trace.'
      },
      limit: {
        type: 'number',
        description: 'Maximum number of results to return (default: 100)',
        minimum: 1,
        maximum: 5000
      }
    },
    required: ['address_range', 'access_type']
  }
};

export async function handleMemoryAccessQuery(args: any): Promise<any> {
  const { address_range, access_type, trace_file, time_range, limit = 100 } = args;
  
  const executor = new WinDbgExecutor();
  
  // Validate trace file if provided
  if (trace_file && !(await executor.validateTraceFile(trace_file))) {
    throw new Error(`Invalid or inaccessible trace file: ${trace_file}`);
  }

  // Validate access type
  const validAccessTypes = ['r', 'w', 'x', 'rw', 'rx', 'wx', 'rwx'];
  if (!validAccessTypes.includes(access_type)) {
    throw new Error(`Invalid access type: ${access_type}. Must be one of: ${validAccessTypes.join(', ')}`);
  }

  try {
    // Build the query command
    const query = executor.buildMemoryQuery(address_range, access_type, {
      timeRange: time_range
    });

    // Add limit if specified
    const limitedQuery = limit ? `${query}.Take(${limit})` : query;

    // Execute the command
    const output = trace_file 
      ? await executor.executeCommand(trace_file, limitedQuery)
      : await executor.executeCommand('', limitedQuery);

    // Parse the results
    const memoryAccesses = executor.parseMemoryAccesses(output);

    // Analyze access patterns
    const accessStats = analyzeMemoryAccessPatterns(memoryAccesses);

    return {
      memory_accesses: memoryAccesses,
      total_found: memoryAccesses.length,
      query_executed: limitedQuery,
      trace_file: trace_file || 'current',
      access_statistics: accessStats,
      summary: {
        address_range,
        access_type,
        total_accesses: memoryAccesses.length,
        unique_threads: [...new Set(memoryAccesses.map(a => a.threadId))].length,
        time_span: memoryAccesses.length > 0 ? {
          first_access: memoryAccesses[0]?.timeStart?.formatted,
          last_access: memoryAccesses[memoryAccesses.length - 1]?.timeStart?.formatted
        } : null
      }
    };
  } catch (error) {
    throw new Error(`Failed to query memory access: ${error instanceof Error ? error.message : 'Unknown error'}`);
  }
}

function analyzeMemoryAccessPatterns(accesses: any[]): any {
  if (accesses.length === 0) {
    return { pattern: 'no_accesses' };
  }

  // Group by thread
  const threadGroups = accesses.reduce((groups, access) => {
    const threadId = access.threadId;
    if (!groups[threadId]) {
      groups[threadId] = [];
    }
    groups[threadId].push(access);
    return groups;
  }, {} as Record<string, any[]>);

  // Analyze access frequency
  const totalAccesses = accesses.length;
  const uniqueThreads = Object.keys(threadGroups).length;
  
  // Detect potential patterns
  let pattern = 'mixed';
  if (accesses.every(a => a.accessType === 'r')) {
    pattern = 'read_only';
  } else if (accesses.every(a => a.accessType === 'w')) {
    pattern = 'write_only';
  } else if (uniqueThreads === 1) {
    pattern = 'single_thread';
  } else if (totalAccesses > 100) {
    pattern = 'high_frequency';
  }

  return {
    pattern,
    total_accesses: totalAccesses,
    unique_threads: uniqueThreads,
    thread_distribution: Object.entries(threadGroups).map(([threadId, threadAccesses]) => ({
      thread_id: threadId,
      access_count: (threadAccesses as any[]).length,
      percentage: (((threadAccesses as any[]).length / totalAccesses) * 100).toFixed(1)
    })),
    access_type_distribution: accesses.reduce((dist, access) => {
      const type = access.accessType || 'unknown';
      dist[type] = (dist[type] || 0) + 1;
      return dist;
    }, {} as Record<string, number>)
  };
}

/**
 * Examples of common memory access queries:
 * 
 * 1. Find all writes to a specific address:
 *    address_range: "0x7ffb578f1e30"
 *    access_type: "w"
 * 
 * 2. Monitor a buffer for any access:
 *    address_range: "0x1000000-0x1001000"
 *    access_type: "rw"
 * 
 * 3. Find code execution in a range:
 *    address_range: "0x140000000-0x140100000"
 *    access_type: "x"
 * 
 * 4. Debug memory corruption around specific time:
 *    address_range: "0x12345678"
 *    access_type: "w" 
 *    time_range: "306600:5BF-306700:1A0"
 */