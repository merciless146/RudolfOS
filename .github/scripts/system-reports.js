// Rudolf OS system reports: files an issue made with the "System report" form in the system-reports database.
// Called by .github/workflows/system-reports.yml through actions/github-script; see docs/system-reports.md.
// The issue body is data written by anyone: it is parsed, trimmed and escaped, never run.
'use strict';
const BRANCH = 'system-reports';
const MAX_REPORT = 60000;

/* the form's sections: "### Label\n\n<value>" (a value shown as text sits in a ``` block; empty: "_No response_") */
function parseForm(body) {
  const out = {};
  for (const part of String(body || '').replace(/\r\n/g, '\n').split(/^### /m).slice(1)) {
    const nl = part.indexOf('\n'); if (nl < 0) continue;
    const label = part.slice(0, nl).trim();
    let v = part.slice(nl + 1).trim();
    const m = /^```[a-z]*\n([\s\S]*?)\n?```$/.exec(v); if (m) v = m[1];
    if (v === '_No response_') v = '';
    out[label] = v;
  }
  return {what: out['What happened'] || '', version: out['Rudolf OS version'] || '', status: out['Status at the time'] || '', report: (out['Full system report'] || '').slice(0, MAX_REPORT)};
}
/* "Key: value" lines (the status block, and the status section of the full report) */
function parseStatus(text) {
  const o = {};
  for (const l of String(text || '').split('\n')) { const m = /^([A-Z][A-Za-z ]{1,30}):\s?(.*)$/.exec(l.trim()); if (m && !(m[1] in o)) o[m[1]] = m[2].slice(0, 300) }
  return o;
}
/* labels for sorting; a fixed set, so an issue cannot make up others */
const AREAS = [
  ['area: oracle', /\b(oracle|tab|tabs|website|web ?page|browser|sign(ed|ing)? ?in|log(ged|ging)? ?in|download)/i],
  ['area: sound', /\b(sound|audio|speaker|microphone|volume)\b/i],
  ['area: display', /\b(display|screen|resolution|monitor|flicker|black screen|frame ?rate|fps)\b/i],
  ['area: network', /\b(wi-?fi|network|internet|ethernet|offline|connect)/i],
  ['area: games', /\b(game|quake|emulator|ruffle|flash|controller|gamepad)\b/i],
  ['area: linux apps', /\b(linux app|flatpak|sober|debian)\b/i],
  ['area: install', /\b(install|update|usb|hard ?disk|boot|start(s|ing)? ?up)\b/i],
];
function labelsFor(rec) {
  const L = new Set(['system-report']);
  const v = /^\d+\.\d+(\.\d+)?$/.exec(String(rec.version || '').trim()); if (v) L.add('version: ' + v[0]);
  const text = rec.title + '\n' + rec.what;
  for (const [l, re] of AREAS) if (re.test(text)) L.add(l);
  if (/shell failed/i.test(rec.status.Desktop || '')) L.add('area: shell');
  if (!rec.hasStatus) L.add('needs: system report');
  return [...L];
}
function record(issue) {
  const f = parseForm(issue.body), st = parseStatus(f.status), full = parseStatus(f.report);
  const s = Object.assign({}, full, st);
  const rec = {
    number: issue.number, title: String(issue.title || '').replace(/^\[Report\]\s*/i, '').slice(0, 200), url: issue.html_url,
    author: issue.user && issue.user.login || '', created: issue.created_at, updated: issue.updated_at, state: issue.state,
    version: f.version || (/^(\d+\.\d+(\.\d+)?)/.exec(s['Rudolf OS'] || '') || [])[1] || '',
    what: f.what.slice(0, 4000), status: s, hasStatus: !!(f.status && Object.keys(st).length >= 3), report: f.report,
  };
  rec.labels = labelsFor(rec);
  return rec;
}
/* the index: one line per report */
const brief = r => ({number: r.number, title: r.title, state: r.state, created: r.created, updated: r.updated, version: r.version,
  started: r.status['Started from'] || '', desktop: r.status.Desktop || '', display: r.status.Display || '', memory: r.status.Memory || '',
  warnings: r.status['Kernel warnings'] || '', labels: r.labels});
/* Markdown cells: everything that could make a link, an image or a table cell is escaped */
const md = s => String(s == null ? '' : s).replace(/[\r\n]+/g, ' ').replace(/[\\`*_{}\[\]()#+\-.!|<>~]/g, c => '\\' + c).slice(0, 120);
function readme(index) {
  const open = index.filter(r => r.state == 'open');
  const count = k => { const m = new Map(); open.forEach(r => { const v = r[k] || '?'; m.set(v, (m.get(v) || 0) + 1) }); return [...m].sort((a, b) => b[1] - a[1]) };
  const area = new Map(); open.forEach(r => r.labels.filter(l => l.startsWith('area: ')).forEach(l => area.set(l, (area.get(l) || 0) + 1)));
  return ['# Rudolf OS system reports', '',
    'Reports sent from Rudolf OS with **Report a problem**, filed by the *System reports* workflow. Each report is an issue labelled `system-report`; this branch keeps a record of each (`reports/<issue>.json`) and an index (`index.json`). Do not edit by hand: the workflow rewrites these files.', '',
    `**${open.length}** open, **${index.length - open.length}** closed.`, '',
    '| Open reports by version | |', '|---|---|', ...count('version').map(([k, n]) => `| ${md(k)} | ${n} |`), '',
    '| Open reports by area | |', '|---|---|', ...[...area].sort((a, b) => b[1] - a[1]).map(([k, n]) => `| ${md(k)} | ${n} |`), '',
    '| Issue | State | Date | Version | Started from | Desktop | Display | Title |', '|---|---|---|---|---|---|---|---|',
    ...index.map(r => `| #${r.number} | ${r.state} | ${md((r.created || '').slice(0, 10))} | ${md(r.version)} | ${md(r.started)} | ${md(r.desktop)} | ${md(r.display)} | ${md(r.title)} |`), ''].join('\n');
}

async function ensureBranch(github, owner, repo) {
  try { await github.rest.repos.getBranch({owner, repo, branch: BRANCH}); return }
  catch (e) { if (e.status !== 404) throw e }
  const tree = await github.rest.git.createTree({owner, repo, tree: [{path: 'README.md', mode: '100644', type: 'blob', content: readme([])}, {path: 'index.json', mode: '100644', type: 'blob', content: '[]\n'}]});
  const c = await github.rest.git.createCommit({owner, repo, message: 'System reports database', tree: tree.data.sha, parents: []});
  await github.rest.git.createRef({owner, repo, ref: 'refs/heads/' + BRANCH, sha: c.data.sha});
}
async function readIndex(github, owner, repo) {
  try { const r = await github.rest.repos.getContent({owner, repo, path: 'index.json', ref: BRANCH}); const v = JSON.parse(Buffer.from(r.data.content, 'base64').toString('utf8')); return Array.isArray(v) ? v : [] }
  catch (e) { if (e.status === 404) return []; throw e }
}
/* one commit with every changed file */
async function commit(github, owner, repo, files, message) {
  const ref = await github.rest.git.getRef({owner, repo, ref: 'heads/' + BRANCH});
  const head = ref.data.object.sha, base = await github.rest.git.getCommit({owner, repo, commit_sha: head});
  const tree = await github.rest.git.createTree({owner, repo, base_tree: base.data.tree.sha, tree: Object.entries(files).map(([path, content]) => ({path, mode: '100644', type: 'blob', content}))});
  const c = await github.rest.git.createCommit({owner, repo, message, tree: tree.data.sha, parents: [head]});
  await github.rest.git.updateRef({owner, repo, ref: 'heads/' + BRANCH, sha: c.data.sha});
}

async function run({github, context, core}) {
  const {owner, repo} = context.repo, issue = context.payload.issue, action = context.payload.action;
  if (!issue || issue.pull_request) return;
  const rec = record(issue);
  await ensureBranch(github, owner, repo);
  const index = (await readIndex(github, owner, repo)).filter(r => r.number !== rec.number);
  index.push(brief(rec)); index.sort((a, b) => b.number - a.number);
  await commit(github, owner, repo, {
    [`reports/${rec.number}.json`]: JSON.stringify(rec, null, 1) + '\n',
    'index.json': JSON.stringify(index, null, 1) + '\n',
    'README.md': readme(index),
  }, `Report #${rec.number}: ${action}`);
  const have = new Set((issue.labels || []).map(l => typeof l == 'string' ? l : l.name));
  const add = rec.labels.filter(l => !have.has(l));
  if (add.length) await github.rest.issues.addLabels({owner, repo, issue_number: rec.number, labels: add});
  if (action == 'opened') {
    const s = rec.status, line = k => s[k] ? `| ${k} | ${md(s[k])} |` : null;
    const body = [`Thank you for the report. It is filed as [reports/${rec.number}.json](../blob/${BRANCH}/reports/${rec.number}.json) in the system reports database.`, '',
      rec.hasStatus ? '| At the time | |\n|---|---|\n' + ['Rudolf OS', 'Started from', 'Desktop', 'Display', 'Memory', 'CPU', 'Kernel warnings'].map(line).filter(Boolean).join('\n')
        : 'The status of the computer is missing. If you can, add it: in Rudolf OS, open Task Manager › Copy system report and paste it into this issue.'].join('\n');
    await github.rest.issues.createComment({owner, repo, issue_number: rec.number, body});
  }
  core.info(`filed report #${rec.number} (${action}), labels: ${rec.labels.join(', ')}`);
}
module.exports = run;
Object.assign(module.exports, {parseForm, parseStatus, labelsFor, record, readme, brief});
