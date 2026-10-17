export interface CaseRow {
  id: string
  name: string
  input: string
  expected: string
}

let seq = 0
function nextId() {
  seq += 1
  return `case-${Date.now()}-${seq}`
}

/** Parse engine format (`# name` + `input|expected`) into structured cases. */
export function parseTestcases(text: string): CaseRow[] {
  const rows: CaseRow[] = []
  let pendingName = ''
  let auto = 1

  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trimEnd()
    if (!line.trim()) continue
    if (line.trimStart().startsWith('#')) {
      pendingName = line.replace(/^\s*#\s*/, '').trim()
      continue
    }
    const pipe = line.indexOf('|')
    if (pipe === -1) continue
    rows.push({
      id: nextId(),
      name: pendingName || `Case ${auto}`,
      input: line.slice(0, pipe),
      expected: line.slice(pipe + 1),
    })
    pendingName = ''
    auto += 1
  }
  return rows
}

/** Serialize structured cases back to engine format. */
export function serializeTestcases(rows: CaseRow[]): string {
  return rows
    .map((r, i) => {
      const label = r.name?.trim() || `Case ${i + 1}`
      return `# ${label}\n${r.input}|${r.expected}`
    })
    .join('\n')
}

export function emptyCase(index = 1): CaseRow {
  return { id: nextId(), name: `Case ${index}`, input: '', expected: '' }
}

/** Turn literal \\n sequences into real newlines when models mis-escape code. */
export function normalizeSource(code: string): string {
  if (!code) return code
  let s = code
  if (
    (s.startsWith('"') && s.endsWith('"')) ||
    (s.startsWith("'") && s.endsWith("'"))
  ) {
    s = s.slice(1, -1)
  }
  const escNl = (s.match(/\\n/g) || []).length
  const realNl = (s.match(/\n/g) || []).length
  // One long line (or almost) with many "\n" tokens → treat as mis-escaped.
  if (escNl >= 2 && (escNl > realNl || realNl <= 1)) {
    let out = ''
    for (let i = 0; i < s.length; i++) {
      if (s[i] === '\\' && i + 1 < s.length) {
        const n = s[i + 1]
        if (n === 'n') {
          out += '\n'
          i++
          continue
        }
        if (n === 't') {
          out += '\t'
          i++
          continue
        }
        if (n === 'r') {
          out += '\r'
          i++
          continue
        }
        if (n === '"' || n === '\\' || n === "'") {
          out += n
          i++
          continue
        }
      }
      out += s[i]
    }
    s = out
  }
  if (!s.endsWith('\n')) s += '\n'
  return s
}

export function friendlyError(message: string): string {
  const lower = message.toLowerCase()
  if (
    lower.includes('api key') ||
    lower.includes('groq') ||
    lower.includes('gemini') ||
    lower.includes('llm') ||
    lower.includes('openai')
  ) {
    return 'Test generation is unavailable. Please ask your administrator to configure the server, then try again.'
  }
  if (lower.includes('failed to fetch') || lower.includes('api offline')) {
    return 'Cannot reach the CodeTrust server. Start it with make serve and refresh.'
  }
  return message
}
