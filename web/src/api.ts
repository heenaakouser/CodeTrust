export type TestStatus =
  | 'PASS'
  | 'FAIL_OUTPUT'
  | 'RUNTIME_ERROR'
  | 'TIMEOUT'
  | 'SKIPPED'

export type OverallStatus =
  | 'ALL_PASSED'
  | 'FAILED'
  | 'COMPILE_ERROR'
  | 'ERROR'

export type LanguageId = 'cpp' | 'python' | 'java' | 'javascript' | 'go'

export interface LanguageInfo {
  id: LanguageId
  name: string
  monaco: string
  sourceFile: string
  dockerImage: string
  compiled: boolean
}

export interface TestResult {
  index: number
  name: string
  status: TestStatus
  input: string
  expected: string
  actual: string
  message: string
  hint: string
  exitCode: number
  durationMs: number
}

export interface RunReport {
  id: string
  createdAt: string
  status: OverallStatus
  mode: string
  language?: string
  label: string
  compileOk: boolean
  compileLog: string
  timeoutSec: number
  memoryMb: number
  total: number
  passed: number
  failed: number
  totalDurationMs: number
  summary: string
  suggestion: string
  tests: TestResult[]
}

export interface Health {
  ok: boolean
  service: string
  version: string
  docker: boolean
  llm?: {
    configured: boolean
  }
}

export interface GenerateTestsResult {
  testcases: string
  count: number
}

export interface FixCodeResult {
  fixedSource: string
  explanation: string
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(path, {
    headers: { 'Content-Type': 'application/json', ...(init?.headers || {}) },
    ...init,
  })
  if (!res.ok) {
    let message = `Request failed (${res.status})`
    try {
      const j = await res.json()
      if (j?.error) message = j.error
    } catch {
      const text = await res.text()
      if (text) message = text
    }
    throw new Error(message)
  }
  return res.json() as Promise<T>
}

export const api = {
  health: () => request<Health>('/api/health'),
  languages: () => request<LanguageInfo[]>('/api/languages'),
  generateTests: (body: {
    source: string
    language: LanguageId
    specification?: string
    count?: number
    /** When true, expected outputs are taken from running the current source. */
    oracleFromSource?: boolean
  }) =>
    request<GenerateTestsResult>('/api/generate-tests', {
      method: 'POST',
      body: JSON.stringify(body),
    }),
  fixCode: (body: {
    source: string
    language: LanguageId
    specification?: string
    compileLog?: string
    failureContext?: string
  }) =>
    request<FixCodeResult>('/api/fix-code', {
      method: 'POST',
      body: JSON.stringify(body),
    }),
  sync: (body: {
    direction: 'spec-to-code' | 'code-to-spec'
    language: LanguageId
    source: string
    specification: string
  }) =>
    request<{ source: string; specification: string }>('/api/sync', {
      method: 'POST',
      body: JSON.stringify(body),
    }),
  createRun: (body: {
    source: string
    language: LanguageId
    testcases: string
    specification?: string
    mode?: 'full' | 'compile_only'
    timeoutSec?: number
    memoryMb?: number
    label?: string
  }) =>
    request<RunReport>('/api/runs', {
      method: 'POST',
      body: JSON.stringify(body),
    }),
}
