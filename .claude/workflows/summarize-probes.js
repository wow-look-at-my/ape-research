export const meta = {
  name: 'summarize-deleted-probes',
  description: 'One haiku agent per deleted research file returns a one-line summary; the script returns the ordered lines',
  phases: [{ title: 'Summarize', detail: 'one agent per file returns its summary text', model: 'haiku' }],
}

const REPO = '/Users/mhaynie/repos/ape-research'

function prompt(path) {
  return `Read this file in full: git -C ${REPO} show 1785223:${path}

Reply with only its summary: plain ASCII, one line, at most 3 sentences. Say what the file does or tests, its key mechanism (syscall numbers, offsets, load commands, flags), and what it showed if the file says so.`
}

const summaries = await parallel(args.map(path => () =>
  agent(prompt(path), { phase: 'Summarize', label: path.replace('research/', ''), model: 'haiku', effort: 'low' })
))
const lines = args.map((path, i) => {
  const s = (summaries[i] ?? '').replace(/\s+/g, ' ').trim()
  return { path, line: `${path}: ${s || 'SUMMARY MISSING'}` }
})
const missing = lines.filter(l => l.line.endsWith('SUMMARY MISSING')).map(l => l.path)
if (missing.length) log(`no summary for: ${missing.join(', ')}`)
return { lines, missing }
