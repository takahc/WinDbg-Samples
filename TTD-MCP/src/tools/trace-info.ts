/**
 * TTD Trace Information Tool
 * Implements MCP tool for getting basic information about TTD trace files
 */

import { Tool } from '@modelcontextprotocol/sdk/types.js';
import { WinDbgExecutor } from '../utils/windbg-executor.js';
import { promises as fs } from 'fs';

export const traceInfoTool: Tool = {
  name: 'ttd_trace_info',
  description: 'Get comprehensive information about a TTD trace file including duration, size, threads, and indexing status.',
  inputSchema: {
    type: 'object',
    properties: {
      trace_file: {
        type: 'string',
        description: 'Path to TTD trace file (.ttd)'
      },
      include_modules: {
        type: 'boolean',
        description: 'Include information about loaded modules',
        default: false
      },
      include_threads: {
        type: 'boolean', 
        description: 'Include detailed thread information',
        default: true
      }
    },
    required: ['trace_file']
  }
};

export async function handleTraceInfo(args: any): Promise<any> {
  const { trace_file, include_modules = false, include_threads = true } = args;
  
  const executor = new WinDbgExecutor();
  
  // Validate trace file
  if (!(await executor.validateTraceFile(trace_file))) {
    throw new Error(`Invalid or inaccessible trace file: ${trace_file}`);
  }

  try {
    // Get file system information
    const fileStats = await fs.stat(trace_file);
    
    // Build commands to get trace information
    const commands = [
      '!ttdext.tt',          // Basic TTD info
      '!index',              // Index status
      'dx @$cursession.TTD.Utility.GetBasicInfo()', // Basic trace info
      '.time',               // Current time position
    ];

    if (include_threads) {
      commands.push('~');    // Thread list
    }

    if (include_modules) {
      commands.push('lm');   // Module list
    }

    const fullCommand = commands.join('; ');
    
    // Execute the commands
    const output = await executor.executeCommand(trace_file, fullCommand);

    // Parse the output
    const traceInfo = parseTraceInfo(output, trace_file, fileStats);
    
    return {
      trace_file,
      file_info: {
        size_bytes: fileStats.size,
        size_mb: Math.round(fileStats.size / (1024 * 1024) * 100) / 100,
        created: fileStats.birthtime,
        modified: fileStats.mtime
      },
      trace_info: traceInfo,
      summary: {
        is_valid_trace: traceInfo.isValid,
        is_indexed: traceInfo.isIndexed,
        duration_info: traceInfo.duration,
        thread_count: traceInfo.threads?.length || 0,
        architecture: traceInfo.architecture
      }
    };

  } catch (error) {
    throw new Error(`Failed to get trace information: ${error instanceof Error ? error.message : 'Unknown error'}`);
  }
}

function parseTraceInfo(output: string, traceFile: string, fileStats: any): any {
  const info: any = {
    isValid: false,
    isIndexed: false,
    duration: null,
    threads: [],
    modules: [],
    architecture: 'unknown',
    recordingInfo: null
  };

  const lines = output.split('\n');
  let currentSection = 'none';

  for (const line of lines) {
    const trimmed = line.trim();

    // Detect trace validity
    if (trimmed.includes('Successfully opened')) {
      info.isValid = true;
    }

    // Detect indexing status
    if (trimmed.includes('Index loaded successfully') || trimmed.includes('Indexed')) {
      info.isIndexed = true;
    } else if (trimmed.includes('Index not found') || trimmed.includes('not indexed')) {
      info.isIndexed = false;
    }

    // Parse time travel positions
    const positionMatch = trimmed.match(/Position:\s*([0-9A-Fa-f]+:[0-9A-Fa-f]+)/);
    if (positionMatch) {
      if (!info.duration) {
        info.duration = { start: null, end: null, current: null };
      }
      info.duration.current = positionMatch[1];
    }

    // Parse basic info from dx command
    if (trimmed.includes('MinPosition')) {
      const minPosMatch = trimmed.match(/MinPosition.*?([0-9A-Fa-f]+:[0-9A-Fa-f]+)/);
      if (minPosMatch) {
        if (!info.duration) info.duration = {};
        info.duration.start = minPosMatch[1];
      }
    }

    if (trimmed.includes('MaxPosition')) {
      const maxPosMatch = trimmed.match(/MaxPosition.*?([0-9A-Fa-f]+:[0-9A-Fa-f]+)/);
      if (maxPosMatch) {
        if (!info.duration) info.duration = {};
        info.duration.end = maxPosMatch[1];
      }
    }

    // Parse thread information
    if (trimmed.match(/^\s*\d+\s+Id:/)) {
      currentSection = 'threads';
      const threadMatch = trimmed.match(/^\s*(\d+)\s+Id:\s*([0-9a-f.]+)\s+Suspend:\s*(\d+)\s+Teb:\s*([0-9a-f`]+)\s+(.+)/);
      if (threadMatch) {
        info.threads.push({
          index: threadMatch[1],
          id: threadMatch[2],
          suspendCount: threadMatch[3],
          teb: threadMatch[4],
          state: threadMatch[5]
        });
      }
    }

    // Parse module information  
    if (trimmed.match(/^[0-9a-f`]+\s+[0-9a-f`]+\s+\w+/)) {
      currentSection = 'modules';
      const moduleMatch = trimmed.match(/^([0-9a-f`]+)\s+([0-9a-f`]+)\s+(\w+)\s+(.+)/);
      if (moduleMatch) {
        info.modules.push({
          startAddress: moduleMatch[1],
          endAddress: moduleMatch[2],
          moduleName: moduleMatch[3],
          imagePath: moduleMatch[4]
        });
      }
    }

    // Detect architecture
    if (trimmed.includes('x64') || trimmed.includes('AMD64')) {
      info.architecture = 'x64';
    } else if (trimmed.includes('x86') || trimmed.includes('i386')) {
      info.architecture = 'x86';
    } else if (trimmed.includes('ARM64')) {
      info.architecture = 'ARM64';
    }

    // Parse recording information
    if (trimmed.includes('Recording started')) {
      const recordMatch = trimmed.match(/Recording started:\s*(.+)/);
      if (recordMatch) {
        if (!info.recordingInfo) info.recordingInfo = {};
        info.recordingInfo.startTime = recordMatch[1];
      }
    }

    if (trimmed.includes('Recording stopped')) {
      const recordMatch = trimmed.match(/Recording stopped:\s*(.+)/);
      if (recordMatch) {
        if (!info.recordingInfo) info.recordingInfo = {};
        info.recordingInfo.endTime = recordMatch[1];
      }
    }
  }

  // Calculate trace duration if we have start and end positions
  if (info.duration?.start && info.duration?.end) {
    info.duration.calculated = calculateTraceDuration(info.duration.start, info.duration.end);
  }

  // Add analysis recommendations
  info.recommendations = generateTraceRecommendations(info, fileStats);

  return info;
}

function calculateTraceDuration(startPos: string, endPos: string): any {
  try {
    const startSeq = parseInt(startPos.split(':')[0], 16);
    const endSeq = parseInt(endPos.split(':')[0], 16);
    const sequenceDelta = endSeq - startSeq;

    return {
      sequence_range: sequenceDelta,
      estimated_instructions: sequenceDelta * 10, // Rough approximation
      complexity: sequenceDelta > 100000 ? 'high' : sequenceDelta > 10000 ? 'medium' : 'low'
    };
  } catch {
    return { error: 'Unable to calculate duration' };
  }
}

function generateTraceRecommendations(info: any, fileStats: any): string[] {
  const recommendations: string[] = [];

  // Indexing recommendations
  if (!info.isIndexed) {
    recommendations.push('Consider indexing this trace with "!index -force" for better performance');
  }

  // Size recommendations
  const sizeMB = fileStats.size / (1024 * 1024);
  if (sizeMB > 1000) {
    recommendations.push('Large trace file (>1GB) - consider filtering queries with time ranges for better performance');
  }

  // Thread count recommendations
  if (info.threads.length > 10) {
    recommendations.push('High thread count detected - use thread-specific filters in queries');
  } else if (info.threads.length === 1) {
    recommendations.push('Single-threaded execution - simplified debugging workflow possible');
  }

  // Duration recommendations
  if (info.duration?.calculated?.complexity === 'high') {
    recommendations.push('Complex/long trace - break analysis into smaller time segments');
  }

  // Architecture recommendations
  if (info.architecture === 'unknown') {
    recommendations.push('Architecture not detected - verify trace file integrity');
  }

  return recommendations;
}

/**
 * Examples of trace info queries:
 * 
 * 1. Basic trace information:
 *    trace_file: "C:\\traces\\myapp.ttd"
 * 
 * 2. Detailed trace analysis:
 *    trace_file: "C:\\traces\\myapp.ttd"
 *    include_modules: true
 *    include_threads: true
 * 
 * 3. Quick validation check:
 *    trace_file: "C:\\traces\\myapp.ttd"
 *    include_modules: false
 *    include_threads: false
 */