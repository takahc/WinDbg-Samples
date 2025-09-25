/**
 * Type definitions for TTD (Time Travel Debugging) related data structures
 * Based on the TTDQueries functionality and TTD Replay API
 */

export interface TTDPosition {
  sequence: string;
  steps: string;
  formatted: string; // e.g., "306600:5BF"
}

export interface TTDFunctionCall {
  eventType: 'Call';
  threadId: string;
  uniqueThreadId: string;
  timeStart: TTDPosition;
  timeEnd: TTDPosition;
  function: string;
  functionAddress: string;
  returnAddress: string;
  returnValue?: string;
  parameters?: Record<string, any>;
}

export interface TTDMemoryAccess {
  eventType: 'Memory';
  threadId: string;
  timeStart: TTDPosition;
  timeEnd: TTDPosition;
  address: string;
  size: number;
  accessType: 'r' | 'w' | 'x' | 'rw';
  value?: string;
  previousValue?: string;
}

export interface TTDRegisterState {
  [registerName: string]: string;
}

export interface TTDTraceInfo {
  fileName: string;
  fileSize: number;
  duration: {
    startPosition: TTDPosition;
    endPosition: TTDPosition;
  };
  threadCount: number;
  isIndexed: boolean;
  architecture: 'x86' | 'x64' | 'ARM64';
}

export interface TTDErrorInfo {
  errorCode: number;
  errorCount: number;
  lastOccurrence: TTDPosition;
  description?: string;
}

export interface TTDQueryOptions {
  traceFile?: string;
  timeRange?: {
    start: TTDPosition;
    end: TTDPosition;
  };
  limit?: number;
  orderBy?: 'TimeStart' | 'TimeEnd' | 'ThreadId';
  ascending?: boolean;
}

export interface TTDQueryResult<T> {
  results: T[];
  totalCount: number;
  executionTime: number;
  query: string;
}