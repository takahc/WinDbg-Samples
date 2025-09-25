/**
 * TTD Error Analysis Tool
 * Implements MCP tool for analyzing error patterns in TTD traces, particularly GetLastError patterns
 */

import { Tool } from '@modelcontextprotocol/sdk/types.js';
import { WinDbgExecutor } from '../utils/windbg-executor.js';

export const errorAnalysisTool: Tool = {
  name: 'ttd_analyze_errors',
  description: 'Analyze error patterns in TTD trace, particularly GetLastError patterns. Useful for understanding error distribution and finding the root cause of failures.',
  inputSchema: {
    type: 'object',
    properties: {
      trace_file: {
        type: 'string',
        description: 'Path to TTD trace file (.ttd). If not provided, uses the currently loaded trace in WinDbg.'
      },
      group_by_error: {
        type: 'boolean',
        description: 'Group results by error code for frequency analysis',
        default: true
      },
      exclude_success: {
        type: 'boolean',
        description: 'Exclude successful calls (error code 0)',
        default: true
      },
      error_functions: {
        type: 'array',
        items: { type: 'string' },
        description: 'List of error-related functions to analyze (default: ["kernelbase!GetLastError", "kernel32!GetLastError"])',
        default: ['kernelbase!GetLastError', 'kernel32!GetLastError']
      },
      limit: {
        type: 'number',
        description: 'Maximum number of error instances to return (default: 500)',
        minimum: 1,
        maximum: 5000,
        default: 500
      }
    }
  }
};

export async function handleErrorAnalysis(args: any): Promise<any> {
  const { 
    trace_file, 
    group_by_error = true, 
    exclude_success = true,
    error_functions = ['kernelbase!GetLastError', 'kernel32!GetLastError'],
    limit = 500 
  } = args;
  
  const executor = new WinDbgExecutor();
  
  // Validate trace file if provided
  if (trace_file && !(await executor.validateTraceFile(trace_file))) {
    throw new Error(`Invalid or inaccessible trace file: ${trace_file}`);
  }

  try {
    const errorAnalysisResults: any = {
      error_calls: [],
      error_statistics: {},
      common_errors: [],
      error_timeline: []
    };

    // Analyze each error function
    for (const errorFunction of error_functions) {
      const query = buildErrorAnalysisQuery(errorFunction, exclude_success, limit);
      
      const output = trace_file 
        ? await executor.executeCommand(trace_file, query)
        : await executor.executeCommand('', query);

      const errorCalls = executor.parseFunctionCalls(output);
      errorAnalysisResults.error_calls.push(...errorCalls);
    }

    // Sort all error calls by time
    errorAnalysisResults.error_calls.sort((a: any, b: any) => {
      const aSeq = parseInt(a.timeStart?.sequence || '0', 16);
      const bSeq = parseInt(b.timeStart?.sequence || '0', 16);
      return aSeq - bSeq;
    });

    // Perform error analysis
    if (group_by_error) {
      errorAnalysisResults.error_statistics = analyzeErrorFrequency(errorAnalysisResults.error_calls);
      errorAnalysisResults.common_errors = getCommonErrors(errorAnalysisResults.error_statistics);
    }

    // Build error timeline
    errorAnalysisResults.error_timeline = buildErrorTimeline(errorAnalysisResults.error_calls);

    // Get error descriptions for common Windows error codes
    errorAnalysisResults.error_descriptions = getErrorDescriptions(errorAnalysisResults.common_errors);

    // Generate insights
    errorAnalysisResults.insights = generateErrorInsights(errorAnalysisResults);

    return {
      total_error_calls: errorAnalysisResults.error_calls.length,
      trace_file: trace_file || 'current',
      analysis_results: errorAnalysisResults,
      summary: {
        total_errors: errorAnalysisResults.error_calls.length,
        unique_error_codes: Object.keys(errorAnalysisResults.error_statistics).length,
        most_common_error: errorAnalysisResults.common_errors[0] || null,
        error_rate_per_second: calculateErrorRate(errorAnalysisResults.error_calls),
        analysis_timespan: getAnalysisTimespan(errorAnalysisResults.error_calls)
      }
    };

  } catch (error) {
    throw new Error(`Failed to analyze errors: ${error instanceof Error ? error.message : 'Unknown error'}`);
  }
}

function buildErrorAnalysisQuery(errorFunction: string, excludeSuccess: boolean, limit: number): string {
  let query = `@$cursession.TTD.Calls("${errorFunction}")`;
  
  if (excludeSuccess) {
    query += '.Where(x => x.ReturnValue != 0)';
  }
  
  query += '.OrderBy(c => c.TimeStart)';
  
  if (limit) {
    query += `.Take(${limit})`;
  }

  return `dx -g ${query}`;
}

function analyzeErrorFrequency(errorCalls: any[]): Record<string, any> {
  const errorStats: Record<string, any> = {};
  
  for (const call of errorCalls) {
    const errorCode = call.returnValue || 'unknown';
    
    if (!errorStats[errorCode]) {
      errorStats[errorCode] = {
        count: 0,
        first_occurrence: call.timeStart,
        last_occurrence: call.timeStart,
        threads: new Set()
      };
    }
    
    errorStats[errorCode].count++;
    errorStats[errorCode].last_occurrence = call.timeStart;
    errorStats[errorCode].threads.add(call.threadId);
  }

  // Convert Sets to arrays for JSON serialization
  Object.values(errorStats).forEach((stat: any) => {
    stat.unique_threads = Array.from(stat.threads);
    delete stat.threads;
  });

  return errorStats;
}

function getCommonErrors(errorStats: Record<string, any>): any[] {
  return Object.entries(errorStats)
    .map(([code, stats]) => ({ error_code: code, ...stats }))
    .sort((a, b) => b.count - a.count)
    .slice(0, 10); // Top 10 errors
}

function buildErrorTimeline(errorCalls: any[]): any[] {
  if (errorCalls.length === 0) return [];

  // Group errors by time windows (e.g., every 1000 sequence steps)
  const timelineGroups: Record<string, any[]> = {};
  
  for (const call of errorCalls) {
    const sequence = parseInt(call.timeStart?.sequence || '0', 16);
    const timeWindow = Math.floor(sequence / 1000) * 1000;
    const timeKey = timeWindow.toString(16).toUpperCase();
    
    if (!timelineGroups[timeKey]) {
      timelineGroups[timeKey] = [];
    }
    
    timelineGroups[timeKey].push(call);
  }

  return Object.entries(timelineGroups).map(([timeWindow, calls]) => ({
    time_window: `${timeWindow}:*`,
    error_count: calls.length,
    error_codes: [...new Set(calls.map(c => c.returnValue))],
    first_error: calls[0].timeStart,
    last_error: calls[calls.length - 1].timeStart
  }));
}

function getErrorDescriptions(commonErrors: any[]): Record<string, string> {
  const descriptions: Record<string, string> = {
    '0': 'ERROR_SUCCESS - The operation completed successfully',
    '2': 'ERROR_FILE_NOT_FOUND - The system cannot find the file specified',
    '3': 'ERROR_PATH_NOT_FOUND - The system cannot find the path specified',
    '5': 'ERROR_ACCESS_DENIED - Access is denied',
    '6': 'ERROR_INVALID_HANDLE - The handle is invalid',
    '87': 'ERROR_INVALID_PARAMETER - The parameter is incorrect',
    '122': 'ERROR_INSUFFICIENT_BUFFER - The data area passed is too small',
    '183': 'ERROR_ALREADY_EXISTS - Cannot create a file when that file already exists',
    '998': 'ERROR_NOACCESS - Invalid access to memory location',
    '1113': 'ERROR_NO_UNICODE_TRANSLATION - No mapping for the Unicode character exists'
  };

  const result: Record<string, string> = {};
  for (const error of commonErrors) {
    const code = error.error_code;
    result[code] = descriptions[code] || `Error code ${code} (description not available)`;
  }

  return result;
}

function generateErrorInsights(analysisResults: any): string[] {
  const insights: string[] = [];
  const { error_calls, error_statistics, common_errors } = analysisResults;

  if (error_calls.length === 0) {
    insights.push('No error calls found in the trace');
    return insights;
  }

  // Most common error insight
  if (common_errors.length > 0) {
    const topError = common_errors[0];
    insights.push(`Most frequent error: ${topError.error_code} (${topError.count} occurrences)`);
  }

  // Threading insights
  const threadDistribution = Object.values(error_statistics).reduce((acc: Record<string, number>, stat: any) => {
    for (const thread of stat.unique_threads) {
      acc[thread] = (acc[thread] || 0) + stat.count;
    }
    return acc;
  }, {} as Record<string, number>);

  const threadCount = Object.keys(threadDistribution).length;
  if (threadCount === 1) {
    insights.push('All errors occur in a single thread - likely synchronous execution');
  } else if (threadCount > 5) {
    insights.push(`Errors distributed across ${threadCount} threads - potential threading issues`);
  }

  // Error clustering insights
  if (common_errors.length > 0 && common_errors[0].count > error_calls.length * 0.5) {
    insights.push('High concentration of single error type - suggests systematic issue');
  }

  return insights;
}

function calculateErrorRate(errorCalls: any[]): number {
  if (errorCalls.length < 2) return 0;

  const firstSeq = parseInt(errorCalls[0].timeStart?.sequence || '0', 16);
  const lastSeq = parseInt(errorCalls[errorCalls.length - 1].timeStart?.sequence || '0', 16);
  
  // Rough approximation: sequence numbers represent time units
  const timeSpan = lastSeq - firstSeq;
  return timeSpan > 0 ? errorCalls.length / timeSpan : 0;
}

function getAnalysisTimespan(errorCalls: any[]): any {
  if (errorCalls.length === 0) return null;

  return {
    start_position: errorCalls[0].timeStart?.formatted,
    end_position: errorCalls[errorCalls.length - 1].timeStart?.formatted,
    total_calls_analyzed: errorCalls.length
  };
}

/**
 * Examples of error analysis queries:
 * 
 * 1. Basic error analysis:
 *    (no parameters - uses defaults)
 * 
 * 2. Include successful calls:
 *    exclude_success: false
 * 
 * 3. Analyze specific error functions:
 *    error_functions: ["ntdll!RtlGetLastWin32Error", "kernelbase!GetLastError"]
 * 
 * 4. High-volume error analysis:
 *    limit: 2000
 *    group_by_error: true
 */