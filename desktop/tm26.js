/* ===== V26 Task Manager and system reports (Rudolf OS 1.2) =====
   Task Manager: Apps (windows, and Oracle's real tabs with the memory each page uses), Processes (every process,
   grouped by what it is, with CPU and memory; End process), Performance (CPU, memory, network), System (Rudolf OS,
   the desktop's host, the services, the kernel), Hardware, Startup. rudolfd provides /api/system and /api/processes;
   ending a process and restarting a service ask for an administrator's password, except for the person's own
   apps. On the browser version the system tabs say what they need.
   System reports: sysReport() describes the computer's state at that moment, in plain text, without personal
   details (no names, files, web addresses, network addresses or network names). "Report a problem" sends it to the
   Rudolf OS issue tracker on GitHub as a new issue that the person reviews and submits (.github/ISSUE_TEMPLATE/
   system-report.yml); a GitHub workflow files it in the system-reports database (.github/workflows). ===== */
const TM_KIND = {desktop: 'Desktop', web: 'Web pages (desktop and Oracle tabs)', gpu: 'Graphics', network: 'Network', audio: 'Sound', chromium: 'Browser helpers',
  webwindow: 'Browser window for sign-in sites', app: 'Apps', system: 'System services', kernel: 'Kernel'};
const TM_ORDER = ['desktop', 'web', 'gpu', 'network', 'audio', 'chromium', 'webwindow', 'app', 'system', 'kernel'];
const TM_GPU = {'0x8086': 'Intel', '0x1002': 'AMD', '0x10de': 'NVIDIA', '0x1234': 'QEMU standard VGA (virtual)', '0x1af4': 'virtio (virtual)', '0x1b36': 'QEMU (virtual)', '0x15ad': 'VMware (virtual)', '0x80ee': 'VirtualBox (virtual)', '0x1414': 'Hyper-V (virtual)'};
const tmE = s => String(s == null ? '' : s).replace(/[&<>"']/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'})[c]);
const tmLines = t => String(t || '').split('\n').map(x => x.trim()).filter(Boolean);
const tmDur = s => { s = Math.max(0, s | 0); const d = s / 86400 | 0, h = s % 86400 / 3600 | 0, m = s % 3600 / 60 | 0; return (d ? d + ' d ' : '') + (d || h ? h + ' h ' : '') + m + ' min' };
const tmDisp = d => String(d || '').split('|').map(x => x.trim()).filter(Boolean).join(' · ') || 'not reported';
function tmGpu(line) {   /* "card0 0x8086:0x46a6 i915" */
  const [card, id = '', drv = ''] = line.split(/\s+/), v = id.split(':')[0];
  return (TM_GPU[v] || 'vendor ' + v) + ' graphics (' + id + ')' + (drv ? ', driver ' + drv : ', no driver');
}
/* the real tabs of Oracle (tabs26.js) and the memory each page uses (measured by rudolf-launch) */
function tmTabs(pages) {
  const mem = new Map(); (pages || []).forEach(p => mem.set(p.url, (mem.get(p.url) || 0) + p.used));
  return [...(typeof RT != 'undefined' ? RT : new Map()).values()].filter(t => t.W).map(t => ({t, title: t.title(), url: t.url(), used: mem.has(t.url()) ? mem.get(t.url()) : null}));
}

function taskmgr26() {
  if (window.__tmW && wins.includes(__tmW)) { __tmW.focus(); return __tmW }
  const W = window.__tmW = appWin('taskmgr', 'Task Manager', '📊', 860, 580);
  let tab = 'apps', iv, sys = null, procs = null, sel = null, sortK = 'rss', sortUp = false, filt = '', netPrev = null;
  const hist = {cpu: [], mem: [], net: []}, L = {};
  W.body.style.cssText = 'display:flex;flex-direction:column;background:var(--wbg,#fff)';
  W.body.innerHTML = `<div class='tmtabs' role='tablist'>${[['apps', 'Apps'], ['procs', 'Processes'], ['perf', 'Performance'], ['system', 'System'], ['hw', 'Hardware'], ['start', 'Startup']].map(([k, n]) => `<button data-t='${k}' role='tab'>${n}</button>`).join('')}</div>
    <div class='tmb' style='flex:1;overflow:auto;padding:8px'></div>
    <div class='rmrow tmfoot' style='justify-content:space-between;padding:4px 8px;border-top:1px solid var(--bd)'><span class='msg tmst'></span><span><button class='tmcopy'>Copy system report</button> <button class='tmrep pri'>Report a problem…</button></span></div>`;
  const B = W.body.querySelector('.tmb'), st = W.body.querySelector('.tmst');
  W.body.querySelectorAll('.tmtabs button').forEach(b => b.onclick = () => { tab = b.dataset.t; sel = null; draw(true) });
  W.body.querySelector('.tmcopy').onclick = async () => { const r = await sysReport(''); try { await navigator.clipboard.writeText(r.text); notify('System report copied', 'Paste it wherever you need it.', {toast: 3000}) } catch (e) { sysReportShow(r.text) } };
  W.body.querySelector('.tmrep').onclick = () => sysReportDialog();

  async function sample() {
    try {
      if (RD) {
        const s = await rd('stats'); L.cpu = s.cpu; L.mem = s.memTotal - s.memAvailable; L.memT = s.memTotal; L.temp = s.tempC; L.bat = s.battery;
        if (tab == 'apps' || tab == 'procs') procs = await rd('processes');
        if (tab == 'perf') {
          const n = await rd('net'); const now = performance.now(), tot = n.interfaces.filter(i => i.name != 'lo' && !/^ifb/.test(i.name)).reduce((a, i) => a + i.rx + i.tx, 0);
          L.net = netPrev ? Math.max(0, (tot - netPrev.b) / ((now - netPrev.t) / 1000)) : 0; netPrev = {b: tot, t: now};
        }
      } else { L.cpu = Math.min(100, (window.__lagAvg || 0) / 5 * 10); if (performance.memory) { L.mem = performance.memory.usedJSHeapSize; L.memT = performance.memory.jsHeapSizeLimit } }
    } catch (e) {}
    hist.cpu.push(L.cpu || 0); hist.mem.push(L.memT ? L.mem / L.memT * 100 : 0); hist.net.push(L.net || 0);
    for (const k in hist) if (hist[k].length > 60) hist[k].shift();
  }
  function graph(arr, label, color, max, fmt) {
    const c = document.createElement('canvas'); c.width = 640; c.height = 100;
    c.style.cssText = 'width:100%;height:100px;border:1px solid var(--bd);background:#0b1220;border-radius:6px';
    const top = max || Math.max(1, ...arr) * 1.2, last = arr.at(-1) || 0;
    c.setAttribute('role', 'img'); c.setAttribute('aria-label', label + ' ' + (fmt ? fmt(last) : last.toFixed(0) + '%'));
    const g = c.getContext('2d'); g.strokeStyle = '#1e293b'; for (let y = 0; y <= 4; y++) { g.beginPath(); g.moveTo(0, y * 25); g.lineTo(640, y * 25); g.stroke() }
    g.strokeStyle = color; g.lineWidth = 2; g.beginPath(); arr.forEach((v, i) => { const x = i * (640 / 59), y = 98 - v / top * 96; i ? g.lineTo(x, y) : g.moveTo(x, y) }); g.stroke();
    return c;
  }
  const h4 = t => { const h = document.createElement('h4'); h.className = 'sh4'; h.textContent = t; return h };
  const kv = rows => { const t = document.createElement('table'); t.className = 'tmt'; rows.filter(r => r).forEach(([k, v]) => { const r = t.insertRow(); const a = r.insertCell(); a.textContent = k; a.style.cssText = 'width:34%;color:var(--mut)'; const b = r.insertCell(); if (v instanceof Node) b.appendChild(v); else b.textContent = v == null || v === '' ? '—' : v }); return t };
  const needRD = () => { const m = document.createElement('div'); m.className = 'msg'; m.textContent = 'This needs Rudolf OS started from its disc, USB drive or hard disk.'; B.appendChild(m) };
  const details = (sum, lines, empty) => { const d = document.createElement('details'); d.innerHTML = `<summary></summary><pre style='font:12px monospace;white-space:pre-wrap;max-height:260px;overflow:auto;margin:6px 0'></pre>`; d.firstChild.textContent = sum; d.querySelector('pre').textContent = lines.length ? lines.join('\n') : empty || '(none)'; return d };
  async function getSys(force) { if (RD && (!sys || force)) { try { sys = await rd('system') } catch (e) { sys = {error: e.message} } } return sys }

  /* ending a process: an administrator approves, except for the person's own apps */
  async function endProc(p, force) {
    const own = p.tag == 'app' && p.user == 'rudolf';
    const what = (p.tag == 'web' ? 'This process shows web pages; ending it closes them (if it shows the desktop, the desktop starts again). ' : '') + `End ${p.name} (process ${p.pid})${force ? ' at once' : ''}?`;
    if (own) { if (!(await new Promise(r => sdlg(W, 'End process', what, [['Cancel', () => r(false)], ['End process', () => r(true), 1]])))) return }
    else if (!(await elevate(what))) return;
    try { await rdPost(`proc/end?pid=${p.pid}${force ? '&force=1' : ''}`); notify('Process ended', p.name, {toast: 3000}); sel = null; setTimeout(() => sample().then(() => draw()), 500) }
    catch (e) { notify('Could not end ' + p.name, e.message, {kind: 'error'}) }
  }
  async function restartSvc(name, label) {
    if (!(await elevate('Restart ' + label + '?'))) return;
    try { await rdPost('service/restart?name=' + name); notify(label, 'Started again.', {toast: 3000}); setTimeout(() => getSys(true).then(() => draw()), 1500) }
    catch (e) { notify('Could not restart ' + label, e.message, {kind: 'error'}) }
  }

  function drawApps() {
    const pages = procs && procs.pages && procs.pages.pages || [], tl = tmTabs(pages);
    const t = document.createElement('table'); t.className = 'tmt';
    t.innerHTML = '<thead><tr><th>App</th><th>Window</th><th>Status</th><th>Workspace</th><th style="text-align:right">Memory</th><th></th></tr></thead><tbody></tbody>';
    const tb = t.querySelector('tbody');
    wins.forEach(o => {
      const w = domWeight(o.el), tr = tb.insertRow();
      tr.innerHTML = `<td></td><td></td><td>${o.min ? 'Minimized' : o.el.classList.contains('act') ? 'Active' : 'Running'}</td><td>${o.ws || 1}</td><td style='text-align:right' title='estimated from the window'>${w.mb.toFixed(1)} MB</td><td><button class='sw'>Switch to</button> <button class='et'>End task</button></td>`;
      tr.children[0].textContent = (o.icon || '') + ' ' + (o.app && APPS[o.app] ? APPS[o.app].name : '—'); tr.children[1].textContent = o.el.querySelector('.t').textContent;
      tr.querySelector('.sw').onclick = () => o.focus();
      tr.querySelector('.et').onclick = () => { if (o === W) return; try { o.onclose = null } catch (e) {} o.close(); if (wins.includes(o)) { o.el.remove(); o.tb.remove(); wins.splice(wins.indexOf(o), 1) } notify('Task ended', o.el.querySelector('.t').textContent, {quiet: true}); draw() };
      tl.filter(x => x.t.W === o).forEach(x => {
        const r = tb.insertRow(); r.innerHTML = `<td style='padding-left:22px'>🌐 Tab</td><td></td><td>Real tab</td><td></td><td style='text-align:right' title='JavaScript memory of the page'>${x.used == null ? '…' : fmtB(x.used)}</td><td><button class='sw'>Show</button> <button class='et'>End tab</button></td>`;
        r.children[1].textContent = x.title + (x.url ? ' — ' + (webHost(x.url) || x.url) : '');
        r.querySelector('.sw').onclick = () => x.t.show(); r.querySelector('.et').onclick = () => { x.t.end(); setTimeout(draw, 200) };
      });
    });
    const dp = pages.find(p => p.url.startsWith(location.origin + '/'));
    B.appendChild(t);
    const m = document.createElement('div'); m.className = 'msg'; m.style.marginTop = '8px';
    m.textContent = (dp ? 'The desktop page uses ' + fmtB(dp.used) + ' of JavaScript memory. ' : '') + (RD ? 'Window memory is estimated from each window\'s contents; a real tab shows the JavaScript memory of its page. If the whole desktop stops responding for a minute, Rudolf OS restarts it and reopens your apps.' : 'Memory is estimated from each window\'s contents. "End task" closes a window even if it asks to save.');
    B.appendChild(m);
  }
  function drawProcs() {
    if (!RD) return needRD();
    if (!procs) { B.textContent = 'Loading…'; return }
    const bar = document.createElement('div'); bar.className = 'rmrow'; bar.style.cssText = 'justify-content:flex-start;margin-bottom:6px';
    bar.innerHTML = `<input class='tmf' placeholder='Find a process' aria-label='Find a process' style='flex:0 1 220px'><button class='tme' disabled>End process</button><button class='tmk' disabled title='Ends it at once, without letting it save or tidy up'>End now</button><span class='msg tmsel'></span>`;
    const f = bar.querySelector('.tmf'); f.value = filt; f.oninput = () => { filt = f.value; drawTable() };
    B.appendChild(bar);
    const wrap = document.createElement('div'); B.appendChild(wrap);
    const pick = p => {
      sel = p ? p.pid : null;
      bar.querySelector('.tme').disabled = bar.querySelector('.tmk').disabled = !p || p.prot;
      bar.querySelector('.tmsel').textContent = p ? (p.prot ? p.name + ': Rudolf OS needs it, so it cannot be ended' : p.name + ' (' + p.pid + ')' + (p.cmd ? ' — ' + p.cmd.slice(0, 90) : '')) : '';
      wrap.querySelectorAll('tr[data-pid]').forEach(r => r.classList.toggle('on', +r.dataset.pid === sel));
    };
    const cur = () => procs.items.find(p => p.pid === sel);
    bar.querySelector('.tme').onclick = () => cur() && endProc(cur(), false);
    bar.querySelector('.tmk').onclick = () => cur() && endProc(cur(), true);
    function drawTable() {
      const q = filt.trim().toLowerCase(), items = procs.items.filter(p => !q || (p.name + ' ' + p.cmd + ' ' + p.pid).toLowerCase().includes(q));
      const cols = [['name', 'Name'], ['pid', 'PID'], ['user', 'User'], ['cpu', 'CPU'], ['rss', 'Memory']];
      const t = document.createElement('table'); t.className = 'tmt tmp';
      t.innerHTML = `<thead><tr>${cols.map(([k, n]) => `<th data-k='${k}' style='cursor:pointer${k != 'name' && k != 'user' ? ';text-align:right' : ''}'>${n}${sortK == k ? (sortUp ? ' ▲' : ' ▼') : ''}</th>`).join('')}</tr></thead>`;
      t.querySelectorAll('th').forEach(h => h.onclick = () => { if (sortK == h.dataset.k) sortUp = !sortUp; else { sortK = h.dataset.k; sortUp = sortK == 'name' || sortK == 'pid' } drawTable() });
      const cmp = (a, b) => { const x = a[sortK] == null ? -1 : a[sortK], y = b[sortK] == null ? -1 : b[sortK]; return (typeof x == 'string' ? x.localeCompare(y) : x - y) * (sortUp ? 1 : -1) };
      for (const k of TM_ORDER) {
        const g = items.filter(p => p.tag == k).sort(cmp); if (!g.length) continue;
        const tb = t.createTBody(), hr = tb.insertRow(), hc = hr.insertCell(); hc.colSpan = 5;
        const mem = g.reduce((a, p) => a + p.rss, 0), cpu = g.reduce((a, p) => a + (p.cpu || 0), 0);
        hc.innerHTML = `<b></b> <span class='msg'></span>`; hc.firstChild.textContent = TM_KIND[k] || k; hc.lastChild.textContent = `${g.length} · ${cpu.toFixed(1)}% CPU · ${fmtB(mem)}`; hc.style.background = 'var(--bg2)';
        g.forEach(p => {
          const r = tb.insertRow(); r.dataset.pid = p.pid; r.style.cursor = 'pointer'; r.tabIndex = 0;
          r.innerHTML = `<td></td><td style='text-align:right'>${p.pid}</td><td></td><td style='text-align:right'>${p.cpu == null ? '…' : p.cpu.toFixed(1) + '%'}</td><td style='text-align:right'>${p.rss ? fmtB(p.rss) : '—'}</td>`;
          const ty = p.name == 'chrome' && /--type=(\S+)/.exec(p.cmd);   /* Chromium's processes: say which kind */
          r.children[0].textContent = (p.prot ? '🔒 ' : '') + p.name + (ty ? ' · ' + ty[1].replace('-process', '') : p.tag == 'desktop' && p.name == 'chrome' ? ' · browser' : ''); r.children[2].textContent = p.user;
          r.onclick = () => pick(p); r.onkeydown = e => { if (e.key == 'Enter') pick(p); else if (e.key == 'Delete' && !p.prot) endProc(p, false) };
        });
      }
      wrap.innerHTML = ''; wrap.appendChild(t); pick(procs.items.find(p => p.pid === sel));
    }
    drawTable();
  }
  function drawPerf() {
    const cards = document.createElement('div'); cards.className = 'tmcards';
    const card = (t, v) => { const d = document.createElement('div'); d.className = 'tmcard'; d.innerHTML = '<small></small><b></b>'; d.querySelector('small').textContent = t; d.querySelector('b').textContent = v; cards.appendChild(d) };
    card(RD ? 'CPU' : 'Desktop load', L.cpu == null ? '—' : L.cpu.toFixed(0) + '%');
    card('Memory', L.mem == null ? '—' : fmtB(L.mem) + (L.memT ? ' of ' + fmtB(L.memT) : ''));
    if (RD) card('Network', fmtB(L.net || 0) + '/s');
    if (L.temp != null) card('Temperature', L.temp.toFixed(0) + ' °C');
    (L.bat || []).forEach(b => card('Battery', b.capacity + '%' + (b.status ? ' · ' + b.status : '')));
    card('Windows', wins.length); card('CPU threads', navigator.hardwareConcurrency || '—');
    B.appendChild(cards);
    B.appendChild(h4(RD ? 'CPU (last minute)' : 'Desktop load (how busy the page is)')); B.appendChild(graph(hist.cpu, 'CPU', '#38bdf8', 100));
    B.appendChild(h4('Memory in use')); B.appendChild(graph(hist.mem, 'Memory', '#a78bfa', 100));
    if (RD) { B.appendChild(h4('Network (received and sent, per second)')); B.appendChild(graph(hist.net, 'Network', '#34d399', 0, v => fmtB(v) + '/s')) }
  }
  async function drawSystem() {
    const s = await getSys(); if (tab != 'system') return; B.innerHTML = '';
    const up = typeof sysuStatus == 'function' ? await sysuStatus().catch(() => null) : null;
    const rb = document.createElement('div'); rb.className = 'rmrow'; rb.style.justifyContent = 'flex-end'; rb.innerHTML = "<button class='tmrf'>Refresh</button>"; rb.querySelector('.tmrf').onclick = () => getSys(true).then(() => draw(true)); B.appendChild(rb);
    B.appendChild(h4('Rudolf OS'));
    B.appendChild(kv([['Version', RVER + ' (' + RVERDATE + ')'], up && ['Installed release', [up.version, up.serial, up.channel].filter(Boolean).join(' · ')],
      ['Started from', !RD ? 'a web browser' : s && s.kernel ? `${s.kernel.boot.toUpperCase()}, ${{cd: 'disc', usb: 'USB drive', disk: 'hard disk'}[s.kernel.media] || s.kernel.media}${s.kernel.live ? ' (live: changes are not kept)' : ''}${s.kernel.recovery ? ', recovery mode' : ''}` : '—'],
      s && s.kernel && ['Running for', tmDur(s.kernel.uptime)], s && s.kernel && ['Load (1, 5, 15 min)', s.kernel.load.join(', ')]]));
    B.appendChild(h4('The desktop'));
    const sv = s && s.services || {};
    B.appendChild(kv([['Runs in', !RD ? 'a web browser' : SHELL ? 'the shell (Oracle has real browser tabs)' : sv.shellFailed ? 'the plain page: the shell did not start (fallback)' : 'the plain page (real tabs are off or not ready)'],
      SHELL && ['Shell', 'version ' + SHELL_VER + ' · ' + (SHELL_CAPS.join(', ') || '—')], SHELL && ['Real tabs open', String(typeof RT != 'undefined' ? RT.size : 0)],
      ['Display', s ? tmDisp(s.display) : '—'], ['Desktop size', innerWidth + '×' + innerHeight + ' · scale ' + devicePixelRatio]]));
    if (SHELL_LOG.length) B.appendChild(details('The shell\'s notes (' + SHELL_LOG.length + ')', SHELL_LOG));
    if (RD && s && s.services) {
      B.appendChild(h4('Services'));
      const t = document.createElement('table'); t.className = 'tmt';
      [['rudolfd (system service)', sv.rudolfd], ['Display (rudolf-display)', sv.display, 'display'], ['Sound server', sv.sound, 'sound'], ['Wi-Fi', sv.wifi, 'wifi'], ['X display server', sv.x],
        ['Shell (real tabs)', sv.shell], ['Browser window for sign-in sites', sv.webWindow], ['Linux apps', sv.linuxApps]].forEach(([n, on, rs]) => {
        const r = t.insertRow(); r.insertCell().textContent = n; r.insertCell().textContent = on ? 'Running' : 'Not running';
        const c = r.insertCell(); if (rs) { const b = document.createElement('button'); b.textContent = 'Restart'; b.onclick = () => restartSvc(rs, n); c.appendChild(b) }
      });
      const r = t.insertRow(); r.insertCell().textContent = 'Desktop (Chromium)'; r.insertCell().textContent = 'Running';
      const b = document.createElement('button'); b.textContent = 'Restart the desktop'; b.onclick = async () => { if (!(await elevate('Restart the desktop? Open apps close; your files are saved.'))) return; try { if (typeof fsFlush == 'function') fsFlush() } catch (e) {} setTimeout(() => rdPost('shell/switch').catch(() => {}), 500) }; r.insertCell().appendChild(b);
      B.appendChild(t);
    }
    B.appendChild(h4('Protection and storage'));
    B.appendChild(kv([['File storage', IDB.db ? 'Running (IndexedDB)' : 'Browser storage'], ['Home folder encryption', KEY ? 'On (AES-256-GCM)' : USER && USER.r == 'guest' ? 'Guest session' : 'Off'],
      ['Firewall', window.SECC && SECC.fw && SECC.fw.on ? 'On (' + SECC.fw.mode + ')' : 'Off'], ['Antivirus', window.SECC && SECC.av && SECC.av.on ? 'On' : 'Off'],
      ['Screen reader', S.sr ? 'On' : 'Off'], ['Notifications', S.dnd ? 'Do not disturb' : 'On'], ['Workspaces', NWS + ' (current ' + WS + ')']]));
    if (RD && s && s.kernel) {
      const k = s.kernel, m = s.memory || {};
      B.appendChild(h4('Kernel'));
      B.appendChild(kv([['Linux', k.release + ' (' + k.machine + ')'], ['Built', k.version], ['Start-up options', k.cmdline], ['Processes and threads', k.procs],
        ['Memory', `${fmtB(m.total - m.available)} in use of ${fmtB(m.total)} · cached ${fmtB(m.cached + m.buffers)}`],
        ['Swap', m.swapTotal > 0 ? `${fmtB(m.swapTotal - m.swapFree)} of ${fmtB(m.swapTotal)}` + (m.zramStored ? ` · compressed in memory: ${fmtB(m.zramStored)} stored in ${fmtB(m.zramUsed)}` : '') : 'none']]));
      B.appendChild(details('Drivers loaded (' + tmLines(k.modules).length + ')', tmLines(k.modules).length ? [tmLines(k.modules).join('  ')] : [], 'none: built into the kernel'));
      B.appendChild(details('Kernel warnings and errors (' + tmLines(k.messages).length + ')', tmLines(k.messages), 'none'));
    }
  }
  async function drawHw() {
    if (!RD) return needRD();
    const s = await getSys(); if (tab != 'hw') return; B.innerHTML = '';
    if (!s || s.error) { B.textContent = s ? s.error : '—'; return }
    const c = s.cpu || {};
    B.appendChild(h4('Computer')); B.appendChild(kv([['Model', s.machine.trim() || 'not reported'], ['Firmware', s.firmware.trim() || 'not reported'], ['Started with', (s.kernel.boot || '').toUpperCase()]]));
    B.appendChild(h4('Processor')); B.appendChild(kv([['Model', c.model], ['Threads', c.threads], ['Speed now', c.mhz ? c.mhz + ' MHz' : null], ['Temperature', c.tempC == null ? 'not reported' : c.tempC + ' °C'], ['Memory', fmtB(s.memory.total)]]));
    B.appendChild(h4('Graphics and display')); B.appendChild(kv([...tmLines(s.gpu).map(l => [l.split(' ')[0], tmGpu(l)]), ['Display', tmDisp(s.display)]]));
    if ((s.battery || []).length) { B.appendChild(h4('Battery')); B.appendChild(kv(s.battery.map(b => ['Battery', b.capacity + '%' + (b.status ? ', ' + b.status : '')]))) }
    B.appendChild(h4('Storage')); B.appendChild(kv(tmLines(s.storage).map(l => { const [d, sz, us, at] = l.split(' '); return [d + ' (' + at + ')', `${fmtB(+us)} used of ${fmtB(+sz)}`] })));
    B.appendChild(h4('Network adapters')); B.appendChild(kv(tmLines(s.net).map(l => { const [n, state, sp, rx, tx, kind] = l.split(' '); return [n + (kind == 'wifi' ? ' (Wi-Fi)' : ' (wired)'), `${state}${sp && sp != '-' && +sp > 0 ? ', ' + sp + ' Mb/s' : ''} · received ${fmtB(+rx)}, sent ${fmtB(+tx)}`] })));
    B.appendChild(h4('Sound')); B.appendChild(kv(tmLines(s.audio).filter(l => /^\d/.test(l)).map(l => ['Card ' + l.split(' ')[0], l.replace(/^\d+\s*/, '')])));
    B.appendChild(details('Input devices (' + tmLines(s.input).length + ')', tmLines(s.input)));
  }
  function drawStart() {
    const d = document.createElement('div'); d.innerHTML = `<label class='ck'><input type='checkbox' class='rs'> Reopen my apps when I sign in</label><h4 class='sh4'>Apps reopened at sign-in</h4><div class='sl'></div>`;
    const rs = d.querySelector('.rs'); rs.checked = S.restore !== false; rs.onchange = () => { S.restore = rs.checked; if (SC) SC.restore = S.restore; persistS() };
    const list = ld(sessK(), []), sl = d.querySelector('.sl'); if (!list.length) sl.textContent = 'None yet.';
    list.forEach(k => { const r = document.createElement('div'); r.className = 'acr'; r.innerHTML = '<span></span><button>Remove</button>'; r.firstChild.textContent = (APPS[k] ? APPS[k].icon + ' ' + APPS[k].name : k); r.lastChild.onclick = () => { svj(sessK(), ld(sessK(), []).filter(x => x != k)); draw() }; sl.appendChild(r) });
    B.appendChild(d);
  }
  function draw(full) {
    W.body.querySelectorAll('.tmtabs button').forEach(b => { b.classList.toggle('on', b.dataset.t == tab); b.setAttribute('aria-selected', b.dataset.t == tab) });
    /* the process list keeps its scroll position and the search box its focus while it refreshes */
    if (tab == 'procs' && !full && B.querySelector('.tmp')) { const y = B.scrollTop, f = document.activeElement && document.activeElement.classList.contains('tmf'); B.innerHTML = ''; drawProcs(); B.scrollTop = y; if (f) { const i = B.querySelector('.tmf'); i.focus(); i.setSelectionRange(i.value.length, i.value.length) } }
    else if (tab == 'system' || tab == 'hw') { if (full) { B.innerHTML = 'Loading…'; (tab == 'system' ? drawSystem : drawHw)() } }
    else { B.innerHTML = ''; ({apps: drawApps, procs: drawProcs, perf: drawPerf, start: drawStart})[tab]() }
    st.textContent = `${wins.length} window(s)${procs ? ' · ' + procs.items.length + ' processes' : ''} · ${RD ? 'Rudolf OS on this computer' : 'Rudolf OS in a browser'}`;
  }
  sample().then(() => draw(true));
  iv = setInterval(async () => { if (W.min) return; await sample(); if (tab == 'apps' || tab == 'procs' || tab == 'perf') draw() }, 2000);
  W.onclose = () => { clearInterval(iv); window.__tmW = null };
  return W;
}
/* Task Manager is this one from now on */
window.taskmgr = taskmgr26;
if (typeof APPS != 'undefined' && APPS.taskmgr) APPS.taskmgr.open = taskmgr26;

/* ===================== system reports ===================== */
const SR_REPO = 'merciless146/RudolfOS';
/* personal details out: network addresses, hardware addresses, e-mail addresses, the account names */
function srScrub(t, words) {
  t = String(t).replace(/\b\d{1,3}(\.\d{1,3}){3}\b/g, 'x.x.x.x').replace(/\b([0-9a-f]{2}:){5}[0-9a-f]{2}\b/gi, 'xx:xx:xx:xx:xx:xx').replace(/[\w.+-]+@[\w-]+(\.[\w-]+)+/g, '(e-mail)');
  try { (typeof ACCTS != 'undefined' ? ACCTS : []).forEach(a => { if (a.n && a.n.length > 2) t = t.split(a.n).join('(user)') }) } catch (e) {}
  (words || []).forEach(w => { if (w && w.length > 1) t = t.replace(new RegExp('(^|[^\\w])' + w.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '(?![\\w])', 'g'), '$1(network name)') });   /* whole words */
  return t;
}
/* the computer's state now. Returns {text, summary, data}: the full report, and the key lines for a short form */
async function sysReport(problem) {
  const get = async p => { if (!RD) return null; try { return await rd(p) } catch (e) { return null } };
  const [sys, st, info, pr, up, wf] = await Promise.all([get('system'), get('stats'), get('info'), get('processes'), typeof sysuStatus == 'function' ? sysuStatus().catch(() => null) : null, get('wifi/status')]);
  /* Wi-Fi network names are left out wherever they appear (the session log, for one) */
  const nets = []; (function walk(o) { if (o && typeof o == 'object') for (const k in o) { if (/^ssid$/i.test(k) && typeof o[k] == 'string') nets.push(o[k]); else walk(o[k]) } })(wf);
  if (wf && typeof wf.raw == 'string') { for (const m of wf.raw.matchAll(/^\s*ssid[=:]\s*(.+)$/gim)) nets.push(m[1].trim()); for (const m of wf.raw.matchAll(/^saved=[^\t]*\t([^\t\n]+)/gm)) nets.push(m[1].trim()) }   /* wifi.sh status: the current and the saved networks */
  try { (NETC.list || NETC.known || []).forEach(n => n && typeof n.ssid == 'string' && nets.push(n.ssid)) } catch (e) {}
  const k = sys && sys.kernel || {}, m = sys && sys.memory || {}, c = sys && sys.cpu || {}, sv = sys && sys.services || {};
  const yes = b => b ? 'running' : 'not running';
  const key = [
    ['Rudolf OS', RVER + ' (' + RVERDATE + ')' + (up ? ' · release ' + [up.version, up.serial, up.channel].filter(Boolean).join(' ') : '')],
    ['Created', new Date().toISOString().replace(/\.\d+Z$/, 'Z')],
    ['Started from', !RD ? 'web browser' : `${(k.boot || info && info.boot || '?').toUpperCase()}, ${k.media || info && info.media || '?'}${(k.live ?? (info && info.live)) ? ', live' : ', installed'}${k.recovery ? ', recovery' : ''}`],
    ['Desktop', !RD ? 'browser version' : SHELL ? 'shell ' + SHELL_VER + ' (real tabs)' : sv.shellFailed ? 'plain page (shell failed)' : 'plain page'],
    ['Display', sys ? tmDisp(sys.display) : '—'],
    ['Desktop size', innerWidth + 'x' + innerHeight + ' @' + devicePixelRatio],
    ['Uptime', k.uptime != null ? tmDur(k.uptime) : '—'],
    ['CPU', st ? st.cpu.toFixed(0) + '% now, load ' + (k.load || [st.load]).join(' ') : '—'],
    ['Memory', st ? `${fmtB(st.memTotal - st.memAvailable)} used of ${fmtB(st.memTotal)}` + (m.swapTotal ? ', swap ' + fmtB(m.swapTotal - m.swapFree) + ' used' : '') : performance.memory ? fmtB(performance.memory.usedJSHeapSize) + ' JS heap' : '—'],
    ['Temperature', st && st.tempC != null ? st.tempC + ' C' : 'not reported'],
    ['Battery', st && st.battery && st.battery.length ? st.battery.map(b => b.capacity + '% ' + (b.status || '')).join(', ') : 'none'],
    ['Storage', info && info.persistTotal > 0 ? fmtB(info.persistFree) + ' free of ' + fmtB(info.persistTotal) : 'live (not installed)'],
    ['Network', typeof netUp == 'function' ? (netUp() ? 'connected' : 'not connected') : (navigator.onLine ? 'online' : 'offline')],
    ['Services', sys ? `display ${yes(sv.display)}, sound ${yes(sv.sound)}, wifi ${yes(sv.wifi)}, X ${yes(sv.x)}, linux apps ${yes(sv.linuxApps)}` : '—'],
    ['Windows', wins.length + (typeof RT != 'undefined' && RT.size ? ', real tabs ' + RT.size : '')],
    ['Kernel warnings', sys ? String(tmLines(k.messages).length) : '—'],
  ];
  const out = ['Rudolf OS system report', ''];
  if (problem) out.push('Problem: ' + problem.replace(/\s+/g, ' ').trim(), '');
  out.push('== Status', ...key.map(([a, b]) => a + ': ' + b), '');
  if (sys) {
    out.push('== Computer', 'Machine: ' + (sys.machine.trim() || '?'), 'Firmware: ' + (sys.firmware.trim() || '?'), 'Processor: ' + c.model + ', ' + c.threads + ' threads' + (c.mhz ? ', ' + c.mhz + ' MHz' : ''),
      ...tmLines(sys.gpu).map(l => 'Graphics: ' + tmGpu(l)), ...tmLines(sys.audio).filter(l => /^\d/.test(l)).map(l => 'Sound: ' + l.replace(/^\d+\s*/, '')),
      ...tmLines(sys.storage).map(l => { const [d, sz, us, at] = l.split(' '); return `Storage: ${d} ${at} ${fmtB(+us)} used of ${fmtB(+sz)}` }),
      ...tmLines(sys.net).map(l => { const [n, s1, sp, , , kind] = l.split(' '); return `Network: ${n} ${kind} ${s1}${sp && sp != '-' && +sp > 0 ? ' ' + sp + ' Mb/s' : ''}` }),
      'Input: ' + tmLines(sys.input).join('; '), '');
    out.push('== Kernel', 'Linux: ' + k.release + ' ' + k.machine, 'Options: ' + k.cmdline, 'Drivers: ' + tmLines(k.modules).length,
      `Memory: total ${fmtB(m.total)}, available ${fmtB(m.available)}, cached ${fmtB(m.cached)}` + (m.zramStored ? `, compressed swap ${fmtB(m.zramStored)} in ${fmtB(m.zramUsed)}` : ''), '');
  }
  if (pr && pr.items) {
    const top = pr.items.slice().sort((a, b) => b.rss - a.rss).slice(0, 10);
    out.push('== Processes (' + pr.items.length + '), most memory first', ...top.map(p => `${p.name} [${p.tag}] ${fmtB(p.rss)}${p.cpu != null ? ' ' + p.cpu.toFixed(1) + '%' : ''}`));
    const busy = pr.items.filter(p => p.cpu > 5).sort((a, b) => b.cpu - a.cpu).slice(0, 5);
    if (busy.length) out.push('Busiest: ' + busy.map(p => `${p.name} ${p.cpu.toFixed(0)}%`).join(', '));
    if (pr.pages && pr.pages.pages) out.push('Page memory: ' + pr.pages.pages.map(p => (p.url.startsWith(location.origin) ? 'desktop' : 'tab') + ' ' + fmtB(p.used)).join(', '));
    out.push('');
  }
  if (sys && tmLines(k.messages).length) out.push('== Kernel warnings and errors (latest)', ...tmLines(k.messages).slice(-20), '');
  if (sys && tmLines(sys.session).length) out.push('== Session log (latest)', ...tmLines(sys.session).slice(-15), '');
  if (SHELL_LOG.length) out.push('== Shell notes', ...SHELL_LOG.slice(-15), '');
  const cl = RD ? await get('clientlog') : null;
  if (cl && tmLines(cl.log).length) out.push('== Desktop errors (latest)', ...tmLines(cl.log).slice(-15), '');
  const text = srScrub(out.join('\n'), nets);
  return {text, summary: srScrub(key.map(([a, b]) => a + ': ' + b).join('\n'), nets), problem: problem || ''};
}
function sysReportShow(text) {
  const o = document.createElement('div'); o.className = 'rmodal';
  o.innerHTML = `<div class='rmbox' role='dialog' aria-label='System report' style='max-width:760px;width:90vw'><h3>System report</h3><textarea readonly style='width:100%;height:50vh;font:12px monospace;box-sizing:border-box'></textarea><div class='rmrow'><button class='sx'>Close</button></div></div>`;
  o.querySelector('textarea').value = text; o.querySelector('.sx').onclick = () => o.remove(); document.body.appendChild(o); o.querySelector('textarea').select();
}
/* the address of a new issue on GitHub with the report filled in (the person reviews and submits it there). GitHub
   refuses very long addresses, so the full report goes on the clipboard when it does not fit; the form says where
   to paste it. */
function sysReportUrl(r, title, full) {
  const u = new URL('https://github.com/' + SR_REPO + '/issues/new');
  u.searchParams.set('template', 'system-report.yml');
  u.searchParams.set('title', '[Report] ' + (title || r.problem.slice(0, 70) || 'Problem with Rudolf OS'));
  u.searchParams.set('labels', 'system-report');
  u.searchParams.set('what', r.problem || '');
  u.searchParams.set('status', r.summary);
  u.searchParams.set('version', RVER);
  if (full) u.searchParams.set('report', r.text);
  return u.href;
}
const SR_MAX = 7000;
function sysReportDialog() {
  const o = document.createElement('div'); o.className = 'rmodal';
  o.innerHTML = `<div class='rmbox' role='dialog' aria-labelledby='srh' style='max-width:620px;width:92vw'>
    <h3 id='srh'>Report a problem</h3>
    <p class='msg'>Your report goes to the Rudolf OS issue tracker on GitHub, where it can be seen by anyone. You sign in to GitHub and send it there; nothing is sent before you do.</p>
    <label>Short title</label><input class='srt' maxlength='80' style='width:100%;box-sizing:border-box' placeholder='e.g. Sound stops after waking from sleep'>
    <label style='display:block;margin-top:8px'>What happened, and what were you doing?</label><textarea class='srw' style='width:100%;height:90px;box-sizing:border-box'></textarea>
    <label class='ck' style='margin-top:8px'><input type='checkbox' class='sri' checked> Include the system report (recommended: the state of the computer right now)</label>
    <div class='msg'>It contains the Rudolf OS version, how the computer started, the display, the hardware, memory, CPU, temperature, battery and storage figures, the services, the busiest processes, and recent warnings. It does not contain your name, files, passwords, web addresses, network addresses or network names. <a href='#' class='srv'>See the report</a></div>
    <div class='msg srm' style='color:#b45309'></div>
    <div class='rmrow'><button class='srs'>Save to a file</button><button class='src'>Cancel</button><button class='srg pri'>Continue on GitHub</button></div></div>`;
  document.body.appendChild(o); o.querySelector('.srt').focus();
  const q = s => o.querySelector(s); let rep = null;
  const build = async () => { rep = await sysReport(q('.srw').value.trim()); if (!q('.sri').checked) rep = {text: '', summary: 'Version: ' + RVER + ' (system report not included)', problem: rep.problem}; return rep };
  q('.src').onclick = () => o.remove();
  q('.srv').onclick = async e => { e.preventDefault(); sysReportShow((await build()).text || '(not included)') };
  q('.srs').onclick = async () => {
    const r = await build(), name = 'REPORT-' + new Date().toISOString().slice(0, 16).replace(/[-:T]/g, '') + '.TXT';
    try { const C = fs['C:']; if (!C.DOCUMENTS || typeof C.DOCUMENTS != 'object') C.DOCUMENTS = {}; C.DOCUMENTS[name] = (q('.srt').value ? 'Title: ' + q('.srt').value + '\n' : '') + r.text; fsSave(); q('.srm').textContent = 'Saved as C:\\DOCUMENTS\\' + name + '. You can send it later.' } catch (x) { q('.srm').textContent = x.message }
  };
  q('.srg').onclick = async () => {
    const r = await build(), title = q('.srt').value.trim();
    if (!title && !r.problem) { q('.srm').textContent = 'Please give the problem a title or a short description.'; return }
    let url = sysReportUrl(r, title, true), note = '';
    if (url.length > SR_MAX) {
      url = sysReportUrl(r, title, false);
      try { await navigator.clipboard.writeText(r.text); note = 'The full report is on the clipboard: paste it into the "Full system report" box on GitHub.' }
      catch (x) { note = 'The full report did not fit; use "Save to a file" and paste its contents into the "Full system report" box on GitHub.' }
    }
    if (typeof netUp == 'function' && window.__net && !netUp()) { q('.srm').textContent = 'There is no internet connection. Save the report to a file and send it when you are connected.'; return }
    o.remove();
    if (note) notify('Report a problem', note, {toast: 15000});
    browser(url);
  };
}
regApp('report', {name: 'Report a problem', icon: '🐞', kw: 'bug issue feedback support system report problem help', open: () => sysReportDialog()});
/* Help: the web in Oracle, and Task Manager with reporting problems */
if (typeof HELP != 'undefined') {
  const at = Math.max(0, HELP.findIndex(h => h[0] == 'trouble'));
  HELP.splice(at, 0,
    ['web', 'The web in Oracle', () => `<ul><li>Oracle opens websites as <b>real browser tabs</b>: signing in works for every site, pop-up sign-ins work, and each account on this computer has its own cookies. Settings › Oracle sign-in › Real browser tabs turns this off (sites that need a sign-in then open in a separate browser window).</li><li><kbd>Ctrl+T</kbd> new tab, <kbd>Ctrl+W</kbd> close tab, <kbd>Ctrl+L</kbd> address bar, <kbd>Alt+←</kbd>/<kbd>Alt+→</kbd> back and forward, <kbd>Ctrl++</kbd>/<kbd>Ctrl+−</kbd> zoom, <kbd>Ctrl+Tab</kbd> next tab. These work inside a website too.</li><li>Downloads go to <code>C:\\DOWNLOADS</code> after the firewall, file-type and antivirus checks (Menu › View downloads).</li><li>Videos and pages can go fullscreen; <kbd>Esc</kbd> brings them back.</li><li>The firewall (Security) applies to everything a website loads.</li></ul>`],
    ['taskman', 'Task Manager and reporting problems', () => `<ul><li><button data-app='taskmgr'>📊 Task Manager</button> (<kbd>Ctrl+Shift+Esc</kbd>): <b>Apps</b> (windows and Oracle's tabs, with the memory each page uses), <b>Processes</b> (every process by kind; End process asks an administrator, and the processes Rudolf OS needs cannot be ended), <b>Performance</b>, <b>System</b> (how Rudolf OS started, the services with Restart, the kernel and its warnings) and <b>Hardware</b>.</li><li><b>Report a problem</b> (<button data-app='report'>🐞 Report a problem</button>, or in Task Manager) opens a ready-filled report on the Rudolf OS issue tracker on GitHub with the state of the computer at that moment. Reports are public; they leave out your name, files, web and network addresses and Wi-Fi names. You read it before sending it, or save it to <code>C:\\DOCUMENTS</code> to send later. You need a GitHub account.</li></ul>`]);
}
