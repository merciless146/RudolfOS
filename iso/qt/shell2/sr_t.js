// The system-reports workflow script against a stand-in for GitHub (in memory): node sr_t.js
// An issue body as GitHub writes it from the form; the database branch, the records, the index, labels, the comment.
const run = require('../../../.github/scripts/system-reports.js');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const status = `Rudolf OS: 1.2.0 (2026-10-20) · release 1.2.0.261020 2026102000 stable
Created: 2026-10-20T15:00:00Z
Started from: UEFI, usb, installed
Desktop: plain page (shell failed)
Display: soft · 1920 1080 60 scaled
Memory: 1.2 GB used of 7.8 GB
CPU: 12% now, load 0.5 0.4 0.3
Kernel warnings: 3`;
const body = (st, rep) => `### What happened

Sound stops after the computer wakes from sleep. [click me](https://evil.example) ![x](https://evil.example/p.png) | broken

### Rudolf OS version

1.2.0

### Status at the time

\`\`\`text
${st}
\`\`\`

### Full system report

\`\`\`text
${rep}
\`\`\``;
// a GitHub stand-in: branches, trees and files in memory
function fakeGitHub() {
  const blobs = {}, commits = {}, refs = {}, trees = {}, labels = [], comments = []; let n = 0; const id = () => 'sha' + (++n);
  const err404 = () => Object.assign(new Error('Not Found'), {status: 404});
  const filesOf = c => trees[commits[c].tree];
  return {labels, comments, refs, files: b => refs[b] ? filesOf(refs[b]) : null, rest: {
    repos: {
      getBranch: async ({branch}) => { if (!refs[branch]) throw err404(); return {data: {}} },
      getContent: async ({path, ref}) => { const f = refs[ref] && filesOf(refs[ref])[path]; if (f == null) throw err404(); return {data: {content: Buffer.from(f).toString('base64')}} },
    },
    git: {
      createTree: async ({base_tree, tree}) => { const t = Object.assign({}, base_tree ? trees[base_tree] : {}); tree.forEach(e => t[e.path] = e.content); const s = id(); trees[s] = t; return {data: {sha: s}} },
      createCommit: async ({tree, parents}) => { const s = id(); commits[s] = {tree, parents}; return {data: {sha: s}} },
      createRef: async ({ref, sha}) => { refs[ref.replace('refs/heads/', '')] = sha; return {data: {}} },
      getRef: async ({ref}) => ({data: {object: {sha: refs[ref.replace('heads/', '')]}}}),
      getCommit: async ({commit_sha}) => ({data: {tree: {sha: commits[commit_sha].tree}}}),
      updateRef: async ({ref, sha}) => { refs[ref.replace('heads/', '')] = sha; return {data: {}} },
    },
    issues: {
      addLabels: async ({labels: l}) => { labels.push(...l); return {data: {}} },
      createComment: async ({body}) => { comments.push(body); return {data: {}} },
    },
  }};
}
(async () => {
  const gh = fakeGitHub(), core = {info: () => {}};
  const issue = (n, b, extra) => Object.assign({number: n, title: '[Report] Sound stops after sleep', body: b, html_url: 'https://github.com/o/r/issues/' + n, user: {login: 'someone'}, created_at: '2026-10-20T15:01:00Z', updated_at: '2026-10-20T15:01:00Z', state: 'open', labels: [{name: 'system-report'}]}, extra || {});
  await run({github: gh, context: {repo: {owner: 'o', repo: 'r'}, payload: {action: 'opened', issue: issue(7, body(status, 'Rudolf OS system report\n\n== Status\n' + status + '\n\n== Processes\nchrome [desktop] 200 MB'))}}, core});
  const f = gh.files('system-reports');
  ok('the database branch is made, with the record, the index and the table', f && f['reports/7.json'] && f['index.json'] && f['README.md']);
  const rec = JSON.parse(f['reports/7.json']);
  ok('the record has the status at the time', rec.status['Started from'] == 'UEFI, usb, installed' && rec.status.Desktop == 'plain page (shell failed)' && rec.version == '1.2.0' && rec.hasStatus, rec.status);
  ok('...the problem and the full report', rec.what.startsWith('Sound stops') && rec.report.includes('== Processes'));
  ok('labels: version and areas from the text and the status (system-report is already there)', ['version: 1.2.0', 'area: sound', 'area: shell'].every(l => gh.labels.includes(l)) && !gh.labels.includes('system-report'), gh.labels);
  ok('a comment with the key status', gh.comments.length == 1 && gh.comments[0].includes('reports/7.json') && gh.comments[0].includes('UEFI'), gh.comments[0]);
  ok('the table escapes what the report says (no links or images from it)', !/\]\(https:\/\/evil/.test(f['README.md']) && f['README.md'].includes('#7'));
  // a second report without the status; then the first is closed
  gh.labels.length = 0;
  await run({github: gh, context: {repo: {owner: 'o', repo: 'r'}, payload: {action: 'opened', issue: issue(8, body('', '').replace('Sound stops', 'The Wi-Fi drops'), {title: '[Report] Wi-Fi drops'})}}, core});
  ok('a report without the status is labelled for it', gh.labels.includes('needs: system report') && gh.labels.includes('area: network'), gh.labels);
  ok('...and its comment asks for it', gh.comments[1].includes('Copy system report'));
  await run({github: gh, context: {repo: {owner: 'o', repo: 'r'}, payload: {action: 'closed', issue: issue(7, body(status, ''), {state: 'closed'})}}, core});
  const idx = JSON.parse(gh.files('system-reports')['index.json']);
  ok('the index keeps one line per report, newest first, with its state', idx.length == 2 && idx[0].number == 8 && idx[1].number == 7 && idx[1].state == 'closed', idx.map(r => [r.number, r.state]));
  ok('a closed report gets no second comment', gh.comments.length == 2);
  console.log(gh.files('system-reports')['README.md']);
  console.log(`RESULT ${R.filter(Boolean).length}/${R.length}`);
  process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(2) });
