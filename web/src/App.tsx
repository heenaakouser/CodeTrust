import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import Editor from '@monaco-editor/react'
import {
  Alert,
  AppBar,
  Box,
  Button,
  Chip,
  CircularProgress,
  CssBaseline,
  IconButton,
  MenuItem,
  Paper,
  Stack,
  TextField,
  ThemeProvider,
  Toolbar,
  Typography,
  createTheme,
} from '@mui/material'
import PlayArrowIcon from '@mui/icons-material/PlayArrow'
import AutoAwesomeIcon from '@mui/icons-material/AutoAwesome'
import AddIcon from '@mui/icons-material/Add'
import DeleteIcon from '@mui/icons-material/Delete'
import AutoFixHighIcon from '@mui/icons-material/AutoFixHigh'
import RefreshIcon from '@mui/icons-material/Refresh'
import CheckCircleOutlinedIcon from '@mui/icons-material/CheckCircleOutlined'
import ErrorOutlinedIcon from '@mui/icons-material/ErrorOutlined'
import {
  api,
  type Health,
  type LanguageId,
  type LanguageInfo,
  type RunReport,
  type OverallStatus,
  type TestStatus,
} from './api'
import {
  type CaseRow,
  emptyCase,
  friendlyError,
  normalizeSource,
  parseTestcases,
  serializeTestcases,
} from './testcases'

/** Clean product UI — side-by-side workspace, no vendor branding. */
const theme = createTheme({
  palette: {
    mode: 'light',
    primary: { main: '#0b6bcb' },
    success: { main: '#1f7a4d' },
    error: { main: '#c62828' },
    warning: { main: '#b26a00' },
    background: { default: '#f6f8fb', paper: '#ffffff' },
    text: { primary: '#0f172a', secondary: '#64748b' },
    divider: '#e2e8f0',
  },
  typography: {
    fontFamily:
      'Inter, ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, sans-serif',
    h5: { fontWeight: 700, letterSpacing: -0.3 },
    subtitle2: { fontWeight: 650 },
  },
  shape: { borderRadius: 12 },
  components: {
    MuiButton: {
      styleOverrides: {
        root: { textTransform: 'none', fontWeight: 650, borderRadius: 10 },
      },
    },
    MuiPaper: {
      styleOverrides: {
        root: { backgroundImage: 'none' },
      },
    },
  },
})

const STARTERS: Record<LanguageId, string> = {
  cpp: `#include <iostream>
using namespace std;

int main() {
    return 0;
}
`,
  python: ``,
  java: `public class Main {
  public static void main(String[] args) {
  }
}
`,
  javascript: ``,
  go: `package main

func main() {
}
`,
}

function statusLabel(status: OverallStatus | TestStatus) {
  switch (status) {
    case 'ALL_PASSED':
    case 'PASS':
      return 'Passed'
    case 'FAIL_OUTPUT':
    case 'FAILED':
      return 'Wrong Answer'
    case 'RUNTIME_ERROR':
      return 'Runtime Error'
    case 'TIMEOUT':
      return 'Timed Out'
    case 'COMPILE_ERROR':
      return 'Compile Error'
    case 'ERROR':
      return 'Error'
    default:
      return status
  }
}

function statusColor(status: OverallStatus | TestStatus) {
  if (status === 'ALL_PASSED' || status === 'PASS') return 'success' as const
  if (status === 'TIMEOUT' || status === 'SKIPPED') return 'warning' as const
  return 'error' as const
}

function buildFailureContext(report: RunReport): string {
  if (!report.compileOk) {
    return 'Compilation failed. Fix the source so it builds cleanly, then matches the problem I/O.'
  }

  const failed = report.tests.filter((t) => t.status !== 'PASS')
  const passed = report.tests.filter((t) => t.status === 'PASS')
  const parts: string[] = [
    'Judging is exact stdout match (trim trailing whitespace).',
    'EXPECTED outputs below are ground truth — change the CODE, not the expected answers.',
    'Infer the stdin format from the examples (e.g. first integer may be n, last may be target).',
  ]

  if (failed.length) {
    parts.push(`\nFAILING CASES (${failed.length}) — code must produce expected exactly:`)
    for (const t of failed.slice(0, 6)) {
      parts.push(
        [
          `### ${t.name} [${t.status}]`,
          'stdin:',
          t.input || '(empty)',
          'expected stdout:',
          t.expected || '(empty)',
          'actual stdout:',
          t.actual || '(empty)',
          t.message ? `note: ${t.message}` : '',
        ]
          .filter(Boolean)
          .join('\n'),
      )
    }
  }

  if (passed.length) {
    parts.push(`\nPASSING CASES (do not break these):`)
    for (const t of passed.slice(0, 2)) {
      parts.push(
        [
          `### ${t.name}`,
          'stdin:',
          t.input || '(empty)',
          'expected stdout:',
          t.expected || '(empty)',
        ].join('\n'),
      )
    }
  }

  return parts.join('\n\n')
}

export default function App() {
  const [languages, setLanguages] = useState<LanguageInfo[]>([])
  const [language, setLanguage] = useState<LanguageId>('cpp')
  const [source, setSource] = useState(STARTERS.cpp)
  const [specification, setSpecification] = useState('')
  const [cases, setCases] = useState<CaseRow[]>([emptyCase(1)])
  const [activeCase, setActiveCase] = useState(0)
  const [health, setHealth] = useState<Health | null>(null)
  const [report, setReport] = useState<RunReport | null>(null)
  const [generating, setGenerating] = useState(false)
  const [running, setRunning] = useState(false)
  const [fixing, setFixing] = useState(false)
  const [regenerating, setRegenerating] = useState(false)
  const [fixNote, setFixNote] = useState<string | null>(null)
  const [error, setError] = useState<string | null>(null)
  // True after system wrote the editor (Regenerate / Fix). Manual edits clear it.
  // When true, Generate tests uses the source as an oracle so tests match the code.
  const [codeTrusted, setCodeTrusted] = useState(false)
  const skipTrustClear = useRef(false)

  const activeLang = useMemo(
    () => languages.find((l) => l.id === language),
    [languages, language],
  )
  const current = cases[activeCase] ?? null
  const needsFix =
    report &&
    (report.status === 'COMPILE_ERROR' ||
      report.status === 'FAILED' ||
      report.status === 'ERROR')

  const refresh = useCallback(async () => {
    try {
      const [h, langs] = await Promise.all([api.health(), api.languages()])
      setHealth(h)
      setLanguages(langs)
    } catch (e) {
      setHealth(null)
      setError(friendlyError(e instanceof Error ? e.message : 'Server offline'))
    }
  }, [])

  useEffect(() => {
    void refresh()
  }, [refresh])

  function applyGenerated(text: string) {
    const parsed = parseTestcases(text)
    setCases(parsed.length ? parsed : [emptyCase(1)])
    setActiveCase(0)
  }

  function resetCases() {
    setCases([emptyCase(1)])
    setActiveCase(0)
    setReport(null)
  }

  function onLanguageChange(next: LanguageId) {
    setLanguage(next)
    setSource(STARTERS[next])
    setCodeTrusted(false)
    setFixNote(null)
    resetCases()
  }

  async function regenerateCode() {
    const trimmed = specification.trim()
    if (trimmed.length < 8) {
      setError('Write a short problem description first, then regenerate code.')
      return
    }
    setRegenerating(true)
    setError(null)
    try {
      const res = await api.sync({
        direction: 'spec-to-code',
        language,
        source,
        specification: trimmed,
      })
      skipTrustClear.current = true
      setSource(normalizeSource(res.source))
      setCodeTrusted(true)
      setFixNote(null)
      // Stale cases from a previous problem must not linger.
      resetCases()
    } catch (e) {
      setError(friendlyError(e instanceof Error ? e.message : 'Could not regenerate code'))
    } finally {
      setRegenerating(false)
    }
  }

  async function generateTests() {
    setGenerating(true)
    setError(null)
    try {
      const res = await api.generateTests({
        source,
        language,
        specification,
        count: 8,
        oracleFromSource: codeTrusted,
      })
      applyGenerated(res.testcases)
      setReport(null)
    } catch (e) {
      setError(friendlyError(e instanceof Error ? e.message : 'Generate failed'))
    } finally {
      setGenerating(false)
    }
  }

  async function runTests() {
    setRunning(true)
    setError(null)
    setFixNote(null)
    try {
      let rows = cases
      if (!rows.length || rows.every((c) => !c.input && !c.expected)) {
        const gen = await api.generateTests({
          source,
          language,
          specification,
          count: 8,
          oracleFromSource: codeTrusted,
        })
        applyGenerated(gen.testcases)
        rows = parseTestcases(gen.testcases)
      }
      const reportRes = await api.createRun({
        source,
        language,
        testcases: serializeTestcases(rows),
        specification,
        mode: 'full',
        timeoutSec: 3,
        memoryMb: 256,
        label: 'editor',
      })
      setReport(reportRes)
      const failIdx = reportRes.tests.findIndex((t) => t.status !== 'PASS')
      if (failIdx >= 0) setActiveCase(failIdx)
    } catch (e) {
      setError(friendlyError(e instanceof Error ? e.message : 'Run failed'))
    } finally {
      setRunning(false)
    }
  }

  async function fixWithAssistant() {
    if (!report) return
    setFixing(true)
    setError(null)
    try {
      const fixed = await api.fixCode({
        source,
        language,
        specification,
        compileLog: report.compileLog,
        failureContext: buildFailureContext(report),
      })
      skipTrustClear.current = true
      setSource(normalizeSource(fixed.fixedSource))
      setCodeTrusted(true)
      setFixNote(fixed.explanation || 'Suggested fix applied to the editor.')
      setReport(null)
    } catch (e) {
      setError(friendlyError(e instanceof Error ? e.message : 'Fix failed'))
    } finally {
      setFixing(false)
    }
  }

  function updateCurrent(patch: Partial<CaseRow>) {
    setCases((prev) =>
      prev.map((c, i) => (i === activeCase ? { ...c, ...patch } : c)),
    )
  }

  function addCase() {
    setCases((prev) => {
      const next = [...prev, emptyCase(prev.length + 1)]
      setActiveCase(next.length - 1)
      return next
    })
  }

  function removeCase() {
    setCases((prev) => {
      if (prev.length <= 1) return [emptyCase(1)]
      const next = prev.filter((_, i) => i !== activeCase)
      setActiveCase(Math.max(0, activeCase - 1))
      return next.map((c, i) => ({ ...c, name: `Case ${i + 1}` }))
    })
  }

  const ready = Boolean(health?.ok && health.docker)

  return (
    <ThemeProvider theme={theme}>
      <CssBaseline />
      <Box sx={{ minHeight: '100vh', bgcolor: 'background.default' }}>
        <AppBar
          position="sticky"
          elevation={0}
          sx={{
            bgcolor: 'rgba(255,255,255,0.92)',
            color: 'text.primary',
            borderBottom: '1px solid',
            borderColor: 'divider',
            backdropFilter: 'blur(10px)',
          }}
        >
          <Toolbar sx={{ gap: 1.5, minHeight: 64 }}>
            <Box>
              <Typography variant="h6" sx={{ fontWeight: 800, lineHeight: 1.1 }}>
                CodeTrust
              </Typography>
              <Typography variant="caption" color="text.secondary">
                Automated verification platform
              </Typography>
            </Box>
            <Chip
              size="small"
              label={ready ? 'Ready' : 'Offline offline'}
              color={ready ? 'success' : 'default'}
              variant="outlined"
              sx={{ ml: 1 }}
            />
            <Box sx={{ flex: 1 }} />
            <TextField
              select
              size="small"
              label="Language"
              value={language}
              onChange={(e) => void onLanguageChange(e.target.value as LanguageId)}
              sx={{ minWidth: 150, bgcolor: '#fff' }}
            >
              {(languages.length
                ? languages
                : [
                    { id: 'cpp', name: 'C++' },
                    { id: 'python', name: 'Python' },
                    { id: 'java', name: 'Java' },
                    { id: 'javascript', name: 'JavaScript' },
                    { id: 'go', name: 'Go' },
                  ]
              ).map((l) => (
                <MenuItem key={l.id} value={l.id}>
                  {l.name}
                </MenuItem>
              ))}
            </TextField>
            <Button
              variant="outlined"
              startIcon={generating ? <CircularProgress size={16} /> : <AutoAwesomeIcon />}
              disabled={generating || running || fixing || regenerating}
              onClick={() => void generateTests()}
            >
              Generate tests
            </Button>
            <Button
              variant="contained"
              startIcon={running ? <CircularProgress size={16} color="inherit" /> : <PlayArrowIcon />}
              disabled={running || generating || fixing || regenerating}
              onClick={() => void runTests()}
            >
              Run
            </Button>
          </Toolbar>
        </AppBar>

        <Box sx={{ maxWidth: 1400, mx: 'auto', p: { xs: 1.5, md: 2.5 } }}>
          <Stack spacing={2}>
            <Paper variant="outlined" sx={{ p: 2 }}>
              <Stack direction="row" sx={{ alignItems: 'center', mb: 1, gap: 1 }}>
                <Typography variant="subtitle2" sx={{ flex: 1 }}>
                  Problem
                </Typography>
                <Button
                  size="small"
                  variant="outlined"
                  startIcon={
                    regenerating ? <CircularProgress size={14} /> : <RefreshIcon />
                  }
                  disabled={regenerating || generating || running || fixing}
                  onClick={() => void regenerateCode()}
                >
                  Regenerate code
                </Button>
              </Stack>
              <TextField
                fullWidth
                multiline
                minRows={2}
                placeholder="Describe what the program should do…"
                value={specification}
                onChange={(e) => setSpecification(e.target.value)}
                helperText="Describe the problem, then click Regenerate code to fill the editor."
              />
            </Paper>

            {error && (
              <Alert severity="error" onClose={() => setError(null)}>
                {error}
              </Alert>
            )}
            {fixNote && (
              <Alert
                severity="success"
                icon={<AutoFixHighIcon />}
                onClose={() => setFixNote(null)}
                action={
                  <Button color="inherit" size="small" onClick={() => void runTests()}>
                    Run again
                  </Button>
                }
              >
                {fixNote}
              </Alert>
            )}

            <Box
              sx={{
                display: 'grid',
                gridTemplateColumns: { xs: '1fr', lg: '1.35fr 1fr' },
                gap: 2,
                alignItems: 'stretch',
              }}
            >
              {/* Code panel */}
              <Paper
                variant="outlined"
                sx={{
                  overflow: 'hidden',
                  display: 'flex',
                  flexDirection: 'column',
                  height: { xs: 480, lg: 620 },
                }}
              >
                <Box
                  sx={{
                    px: 2,
                    py: 1.25,
                    borderBottom: '1px solid',
                    borderColor: 'divider',
                    display: 'flex',
                    alignItems: 'center',
                    gap: 1,
                    flexShrink: 0,
                  }}
                >
                  <Typography variant="subtitle2" sx={{ flex: 1 }}>
                    Code
                  </Typography>
                  <Typography variant="caption" color="text.secondary">
                    {activeLang?.sourceFile || 'source'}
                  </Typography>
                </Box>
                <Box sx={{ flex: 1, minHeight: 0, position: 'relative', bgcolor: '#0b1220' }}>
                  <Box sx={{ position: 'absolute', inset: 0 }}>
                    <Editor
                      height="100%"
                      language={activeLang?.monaco || 'cpp'}
                      theme="vs-dark"
                      value={source}
                      onChange={(v) => {
                        if (skipTrustClear.current) {
                          skipTrustClear.current = false
                          setSource(v ?? '')
                          return
                        }
                        setCodeTrusted(false)
                        setSource(v ?? '')
                      }}
                      options={{
                        minimap: { enabled: false },
                        fontSize: 14,
                        fontFamily: 'JetBrains Mono, Menlo, monospace',
                        scrollBeyondLastLine: false,
                        automaticLayout: true,
                        padding: { top: 14 },
                        lineNumbers: 'on',
                        wordWrap: 'on',
                      }}
                    />
                  </Box>
                </Box>
              </Paper>

              {/* Right column: tests + results */}
              <Stack spacing={2} sx={{ minHeight: { xs: 480, lg: 620 } }}>
                <Paper variant="outlined" sx={{ p: 2, flex: 1 }}>
                  <Stack direction="row" sx={{ alignItems: 'center', mb: 1.5 }}>
                    <Typography variant="subtitle2" sx={{ flex: 1 }}>
                      Test cases
                    </Typography>
                    <IconButton size="small" onClick={addCase}>
                      <AddIcon fontSize="small" />
                    </IconButton>
                    <IconButton size="small" onClick={removeCase}>
                      <DeleteIcon fontSize="small" />
                    </IconButton>
                  </Stack>

                  <Stack direction="row" spacing={1} sx={{ flexWrap: 'wrap', gap: 1, mb: 2 }}>
                    {cases.map((c, i) => {
                      const tr = report?.tests[i]
                      return (
                        <Chip
                          key={c.id}
                          label={`Case ${i + 1}`}
                          onClick={() => setActiveCase(i)}
                          color={
                            tr
                              ? statusColor(tr.status)
                              : activeCase === i
                                ? 'primary'
                                : 'default'
                          }
                          variant={activeCase === i ? 'filled' : 'outlined'}
                          icon={
                            tr?.status === 'PASS' ? (
                              <CheckCircleOutlinedIcon />
                            ) : tr ? (
                              <ErrorOutlinedIcon />
                            ) : undefined
                          }
                        />
                      )
                    })}
                  </Stack>

                  {current && (
                    <Stack spacing={1.5}>
                      <Box>
                        <Typography variant="caption" color="text.secondary" sx={{ fontWeight: 700 }}>
                          INPUT
                        </Typography>
                        <TextField
                          fullWidth
                          multiline
                          minRows={3}
                          value={current.input}
                          onChange={(e) => updateCurrent({ input: e.target.value })}
                          sx={{ mt: 0.5 }}
                          slotProps={{
                            input: {
                              sx: { fontFamily: 'JetBrains Mono, monospace', fontSize: 13 },
                            },
                          }}
                        />
                      </Box>
                      <Box>
                        <Typography variant="caption" color="text.secondary" sx={{ fontWeight: 700 }}>
                          EXPECTED
                        </Typography>
                        <TextField
                          fullWidth
                          multiline
                          minRows={2}
                          value={current.expected}
                          onChange={(e) => updateCurrent({ expected: e.target.value })}
                          sx={{ mt: 0.5 }}
                          slotProps={{
                            input: {
                              sx: { fontFamily: 'JetBrains Mono, monospace', fontSize: 13 },
                            },
                          }}
                        />
                      </Box>
                      {report?.tests[activeCase] && (
                        <Box>
                          <Typography variant="caption" color="text.secondary" sx={{ fontWeight: 700 }}>
                            YOUR OUTPUT
                          </Typography>
                          <Box component="pre" sx={monoBox}>
                            {report.tests[activeCase].actual || '(empty)'}
                          </Box>
                        </Box>
                      )}
                    </Stack>
                  )}
                </Paper>

                <Paper variant="outlined" sx={{ p: 2 }}>
                  <Typography variant="subtitle2" sx={{ mb: 1.25 }}>
                    Results
                  </Typography>
                  {!report && (
                    <Typography variant="body2" color="text.secondary">
                      Generate tests, then Run to verify your program.
                    </Typography>
                  )}
                  {report && (
                    <Stack spacing={1.5}>
                      <Stack direction="row" spacing={1} sx={{ alignItems: 'center', flexWrap: 'wrap' }}>
                        <Chip
                          label={statusLabel(report.status)}
                          color={statusColor(report.status)}
                          sx={{ fontWeight: 700 }}
                        />
                        <Typography variant="body2" color="text.secondary">
                          {report.passed}/{report.total || 0} passed ·{' '}
                          {Math.round(report.totalDurationMs)} ms
                        </Typography>
                      </Stack>

                      {report.suggestion && (
                        <Alert severity={report.status === 'ALL_PASSED' ? 'success' : 'warning'}>
                          {report.suggestion}
                        </Alert>
                      )}

                      {!report.compileOk && report.compileLog && (
                        <Box>
                          <Typography variant="caption" color="text.secondary" sx={{ fontWeight: 700 }}>
                            COMPILER OUTPUT
                          </Typography>
                          <Box component="pre" sx={{ ...monoBox, bgcolor: '#0f172a', color: '#e2e8f0' }}>
                            {report.compileLog}
                          </Box>
                        </Box>
                      )}

                      {needsFix && (
                        <Button
                          fullWidth
                          variant="contained"
                          color="warning"
                          startIcon={
                            fixing ? <CircularProgress size={16} color="inherit" /> : <AutoFixHighIcon />
                          }
                          disabled={fixing || running}
                          onClick={() => void fixWithAssistant()}
                        >
                          {fixing ? 'Fixing…' : 'Fix code'}
                        </Button>
                      )}
                    </Stack>
                  )}
                </Paper>
              </Stack>
            </Box>

            <Typography variant="caption" color="text.secondary" sx={{ textAlign: 'center' }}>
              CodeTrust · secure sandboxed verification
            </Typography>
          </Stack>
        </Box>
      </Box>
    </ThemeProvider>
  )
}

const monoBox = {
  mt: 0.5,
  mb: 0,
  p: 1.25,
  borderRadius: 2,
  bgcolor: '#f1f5f9',
  border: '1px solid #e2e8f0',
  fontFamily: 'JetBrains Mono, Menlo, monospace',
  fontSize: 12.5,
  whiteSpace: 'pre-wrap' as const,
  wordBreak: 'break-word' as const,
  maxHeight: 180,
  overflow: 'auto',
}
