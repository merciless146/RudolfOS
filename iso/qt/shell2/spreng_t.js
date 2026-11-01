// SpreadsheetPro's formula engine (desktop/spreng45.js) in node: parsing, functions, errors, cycles, formats, speed.
const fs = require('fs'), path = require('path'), vm = require('vm');
const src = fs.readFileSync(path.join(__dirname, '../../../desktop/spreng45.js'), 'utf8');
const ctx = {}; vm.createContext(ctx); vm.runInContext(src + ';this.SPE=SPE', ctx); const SPE = ctx.SPE;
let pass = 0, fail = 0;
const ok = (n, c, d) => { if (c) pass++; else { fail++; console.log('FAIL ' + n + (d !== undefined ? ' - ' + JSON.stringify(d) : '')) } };
function book(cells, more) {
  const mk = (name, id, cs) => { const s = { name, id, cells: new Map() }; for (const [a, v] of Object.entries(cs || {})) { const p = SPE.parseA1(a); s.cells.set(p.r * SPE.MAXC + p.c, typeof v == 'string' && v[0] == '=' ? { f: v } : { v }) } return s };
  const b = { sheets: [mk('Sheet1', 1, cells)], names: {} };
  if (more) for (const [n, cs] of Object.entries(more)) b.sheets.push(mk(n, b.sheets.length + 1, cs));
  return b;
}
const val = (b, a, sh = 0) => { SPE.calc(b); const p = SPE.parseA1(a); const c = b.sheets[sh].cells.get(p.r * SPE.MAXC + p.c); return c && (c.f != null ? c.val : c.v) };
const ev = (f, cells, more) => SPE.evalIn(book(cells, more), 0, f);
const eq = (n, got, want) => ok(n, typeof want == 'number' && typeof got == 'number' ? Math.abs(got - want) < 1e-9 * Math.max(1, Math.abs(want)) : JSON.stringify(got) === JSON.stringify(want), { got, want });
const err = (n, got, e) => ok(n, got && got.e === e, { got, want: e });
// names
eq('colName 0', SPE.colName(0), 'A'); eq('colName 25', SPE.colName(25), 'Z'); eq('colName 26', SPE.colName(26), 'AA'); eq('colName 701', SPE.colName(701), 'ZZ');
eq('colNum AA', SPE.colNum('AA'), 26); eq('parseA1 $B$3', SPE.parseA1('$B$3'), { r: 2, c: 1 }); eq('parseA1 out', SPE.parseA1('AAA1'), null);
// arithmetic and precedence
eq('1+2*3', ev('1+2*3'), 7); eq('(1+2)*3', ev('(1+2)*3'), 9); eq('-2^2 (Excel: 4)', ev('-2^2'), 4); eq('2^3^2 left to right, as Excel', ev('2^3^2'), 64);
eq('10%', ev('10%'), 0.1); eq('&', ev('"a"&1&TRUE'), 'a1TRUE'); eq('= text no case', ev('"abc"="ABC"'), true); eq('1<"a"', ev('1<"a"'), true);
err('1/0', ev('1/0'), '#DIV/0!'); err('"x"+1', ev('"x"+1'), '#VALUE!'); err('unknown fn', ev('FOO(1)'), '#NAME?'); err('bad syntax', ev('1+'), '#VALUE!');
eq('"3"+4 text number', ev('"3"+4'), 7); eq('string escape', ev('"a""b"'), 'a"b');
// references
const c1 = { A1: 1, A2: 2, A3: 3, A4: 'x', A5: true, B1: '=A1*10', B2: '=B1+A2', C1: '=SUM(A1:A5)' };
eq('ref', val(book(c1), 'B1'), 10); eq('chain', val(book(c1), 'B2'), 12); eq('SUM skips text and bool in ranges', val(book(c1), 'C1'), 6);
eq('SUM args convert', ev('SUM(1,"2",TRUE)'), 4);
eq('other sheet', ev("Data!A1+'My Sheet'!B2", {}, { Data: { A1: 5 }, 'My Sheet': { B2: 7 } }), 12);
eq('whole column', ev('SUM(A:A)', { A1: 1, A99999: 2 }), 3); eq('whole row', ev('SUM(2:2)', { B2: 4, ZZ2: 6 }), 10);
err('missing sheet', ev('Nope!A1'), '#REF!');
// cycles
{ const b = book({ A1: '=B1', B1: '=A1', C1: '=A1+1', D1: '=5' }); SPE.calc(b); err('cycle A1', val(b, 'A1'), '#CIRCULAR!'); err('cycle spreads', val(b, 'C1'), '#CIRCULAR!'); eq('others fine', val(b, 'D1'), 5) }
{ const b = book({ A1: '=A1+1' }); err('self ref', val(b, 'A1'), '#CIRCULAR!') }
// maths
eq('ROUND', ev('ROUND(2.345,2)'), 2.35); eq('ROUND neg', ev('ROUND(-2.5,0)'), -3); eq('ROUND tens', ev('ROUND(1234,-2)'), 1200);
eq('ROUNDUP', ev('ROUNDUP(1.21,1)'), 1.3); eq('ROUNDDOWN', ev('ROUNDDOWN(-1.29,1)'), -1.2); eq('INT neg', ev('INT(-1.5)'), -2);
eq('MOD neg', ev('MOD(-3,2)'), 1); err('MOD 0', ev('MOD(1,0)'), '#DIV/0!'); err('SQRT neg', ev('SQRT(-1)'), '#NUM!');
eq('POWER', ev('POWER(2,10)'), 1024); eq('ABS', ev('ABS(-3)'), 3); eq('PRODUCT', ev('PRODUCT(2,3,4)'), 24); eq('FACT', ev('FACT(5)'), 120);
eq('GCD', ev('GCD(12,18)'), 6); eq('LCM', ev('LCM(4,6)'), 12); eq('CEILING', ev('CEILING(2.1,0.5)'), 2.5); eq('FLOOR', ev('FLOOR(2.9,1)'), 2);
eq('SUMPRODUCT', ev('SUMPRODUCT(A1:A3,B1:B3)', { A1: 1, A2: 2, A3: 3, B1: 4, B2: 5, B3: 6 }), 32);
{ const r = ev('RAND()'); ok('RAND 0..1', r >= 0 && r < 1, r); const k = ev('RANDBETWEEN(3,5)'); ok('RANDBETWEEN', k >= 3 && k <= 5 && Number.isInteger(k), k) }
// statistics
const st = { A1: 2, A2: 4, A3: 4, A4: 4, A5: 5, A6: 5, A7: 7, A8: 9, A9: 'x' };
eq('AVERAGE', ev('AVERAGE(A1:A9)', st), 5); eq('MEDIAN', ev('MEDIAN(A1:A8)', st), 4.5); eq('MODE', ev('MODE(A1:A8)', st), 4);
eq('STDEVP', ev('STDEVP(A1:A8)', st), 2); eq('VAR', ev('VAR(A1:A8)', st), 32 / 7); eq('MIN/MAX', ev('MAX(A1:A9)-MIN(A1:A9)', st), 7);
eq('COUNT', ev('COUNT(A1:A9)', st), 8); eq('COUNTA', ev('COUNTA(A1:A10)', st), 9); eq('COUNTBLANK', ev('COUNTBLANK(A1:A10)', st), 1);
eq('LARGE', ev('LARGE(A1:A8,2)', st), 7); eq('SMALL', ev('SMALL(A1:A8,1)', st), 2); eq('RANK', ev('RANK(7,A1:A8)', st), 2);
eq('PERCENTILE', ev('PERCENTILE(A1:A8,0.5)', st), 4.5); eq('QUARTILE', ev('QUARTILE(A1:A8,4)', st), 9);
err('AVERAGE empty', ev('AVERAGE(B1:B3)'), '#DIV/0!');
const sales = { A1: 'east', A2: 'west', A3: 'east', A4: 'north', B1: 10, B2: 20, B3: 30, B4: 40, C1: 'a', C2: 'b', C3: 'b', C4: 'a' };
eq('COUNTIF text', ev('COUNTIF(A1:A4,"east")', sales), 2); eq('COUNTIF >', ev('COUNTIF(B1:B4,">15")', sales), 3); eq('COUNTIF wild', ev('COUNTIF(A1:A4,"*st")', sales), 3);
eq('COUNTIF <>', ev('COUNTIF(A1:A4,"<>east")', sales), 2); eq('SUMIF', ev('SUMIF(A1:A4,"east",B1:B4)', sales), 40); eq('SUMIF self', ev('SUMIF(B1:B4,">=30")', sales), 70);
eq('SUMIFS', ev('SUMIFS(B1:B4,A1:A4,"east",C1:C4,"b")', sales), 30); eq('COUNTIFS', ev('COUNTIFS(A1:A4,"east",B1:B4,"<20")', sales), 1);
eq('AVERAGEIF', ev('AVERAGEIF(A1:A4,"east",B1:B4)', sales), 20); eq('MAXIFS', ev('MAXIFS(B1:B4,C1:C4,"a")', sales), 40); eq('MINIFS', ev('MINIFS(B1:B4,C1:C4,"b")', sales), 20);
eq('CORREL', ev('CORREL(A1:A3,B1:B3)', { A1: 1, A2: 2, A3: 3, B1: 2, B2: 4, B3: 6 }), 1);
// logic: lazy
eq('IF', ev('IF(1>2,"y","n")'), 'n'); eq('IF lazy (no #DIV/0!)', ev('IF(TRUE,1,1/0)'), 1); eq('IF no else', ev('IF(FALSE,1)'), false);
eq('IFERROR', ev('IFERROR(1/0,"oops")'), 'oops'); eq('IFERROR ok', ev('IFERROR(5,"oops")'), 5); eq('IFERROR on ref', ev('IFERROR(A1,0)', { A1: '=1/0' }), 0);
eq('IFNA', ev('IFNA(NA(),1)'), 1); err('IFNA other', ev('IFNA(1/0,1)'), '#DIV/0!');
eq('IFS', ev('IFS(1>2,"a",2>1,"b")'), 'b'); err('IFS none', ev('IFS(FALSE,1)'), '#N/A');
eq('AND/OR/NOT/XOR', [ev('AND(TRUE,1)'), ev('OR(FALSE,0)'), ev('NOT(0)'), ev('XOR(TRUE,TRUE,TRUE)')], [true, false, true, true]);
eq('SWITCH', ev('SWITCH(2,1,"one",2,"two","other")'), 'two'); eq('SWITCH default', ev('SWITCH(9,1,"one","other")'), 'other');
eq('CHOOSE', ev('CHOOSE(2,"a","b","c")'), 'b'); eq('IS*', [ev('ISBLANK(A1)'), ev('ISNUMBER(1)'), ev('ISTEXT("a")'), ev('ISERROR(1/0)'), ev('ISNA(NA())'), ev('ISEVEN(4)')], [true, true, true, true, true, true]);
// lookup
const tb = { A1: 'apple', B1: 1.5, C1: 'red', A2: 'banana', B2: 0.5, C2: 'yellow', A3: 'cherry', B3: 3, C3: 'red', E1: 0, F1: 'F', E2: 60, F2: 'D', E3: 70, F3: 'C', E4: 80, F4: 'B', E5: 90, F5: 'A' };
eq('VLOOKUP exact', ev('VLOOKUP("banana",A1:C3,3,FALSE)', tb), 'yellow'); err('VLOOKUP missing', ev('VLOOKUP("kiwi",A1:C3,2,FALSE)', tb), '#N/A');
eq('VLOOKUP approx grades', ev('VLOOKUP(85,E1:F5,2)', tb), 'B'); eq('VLOOKUP approx top', ev('VLOOKUP(100,E1:F5,2,TRUE)', tb), 'A');
eq('VLOOKUP wildcard', ev('VLOOKUP("ch*",A1:C3,2,FALSE)', tb), 3); eq('VLOOKUP no case', ev('VLOOKUP("APPLE",A1:B3,2,0)', tb), 1.5);
eq('HLOOKUP', ev('HLOOKUP("b",A1:C2,2,FALSE)', { A1: 'a', B1: 'b', C1: 'c', A2: 1, B2: 2, C2: 3 }), 2);
eq('MATCH exact', ev('MATCH("cherry",A1:A3,0)', tb), 3); eq('MATCH approx', ev('MATCH(75,E1:E5)', tb), 3);
eq('INDEX', ev('INDEX(A1:C3,2,3)', tb), 'yellow'); eq('INDEX/MATCH', ev('INDEX(B1:B3,MATCH("cherry",A1:A3,0))', tb), 3); eq('INDEX 1-row', ev('INDEX(A1:C1,2)', tb), 1.5);
eq('SUM(INDEX col)', ev('SUM(INDEX(A1:C3,0,2))', tb), 5);
eq('XLOOKUP', ev('XLOOKUP("cherry",A1:A3,C1:C3)', tb), 'red'); eq('XLOOKUP not found', ev('XLOOKUP("kiwi",A1:A3,C1:C3,"none")', tb), 'none');
eq('XLOOKUP last', ev('XLOOKUP("red",C1:C3,A1:A3,,0,-1)', tb), 'cherry'); eq('XLOOKUP next smaller', ev('XLOOKUP(65,E1:E5,F1:F5,,-1)', tb), 'D');
eq('OFFSET', ev('SUM(OFFSET(A1,1,1,2,1))', tb), 3.5); eq('INDIRECT', ev('INDIRECT("B"&3)', tb), 3); eq('ROW/COLUMN', [ev('ROW(C7)'), ev('COLUMN(C7)'), ev('ROWS(A1:B4)'), ev('COLUMNS(A1:B4)')], [7, 3, 4, 2]);
eq('ADDRESS', ev('ADDRESS(2,28)'), '$AB$2'); eq('array const', ev('SUM({1,2;3,4})'), 10);
// text
eq('LEFT/RIGHT/MID', [ev('LEFT("hello",2)'), ev('RIGHT("hello",3)'), ev('MID("hello",2,3)'), ev('LEFT("hi")')], ['he', 'llo', 'ell', 'h']);
eq('LEN/UPPER/LOWER/PROPER', [ev('LEN("abc")'), ev('UPPER("ab")'), ev('LOWER("AB")'), ev('PROPER("hello wORLD")')], [3, 'AB', 'ab', 'Hello World']);
eq('TRIM', ev('TRIM("  a   b ")'), 'a b'); eq('FIND', ev('FIND("l","hello")'), 3); err('FIND none', ev('FIND("z","hello")'), '#VALUE!'); eq('SEARCH', ev('SEARCH("L*O","hello")'), 3);
eq('SUBSTITUTE', ev('SUBSTITUTE("a-b-c","-","+")'), 'a+b+c'); eq('SUBSTITUTE nth', ev('SUBSTITUTE("a-b-c","-","+",2)'), 'a-b+c'); eq('REPLACE', ev('REPLACE("abcdef",2,3,"X")'), 'aXef');
eq('CONCAT range', ev('CONCAT(A1:A2,"!")', { A1: 'x', A2: 'y' }), 'xy!'); eq('TEXTJOIN', ev('TEXTJOIN(", ",TRUE,A1:A3)', { A1: 'x', A3: 'z' }), 'x, z');
eq('REPT', ev('REPT("ab",3)'), 'ababab'); eq('VALUE', ev('VALUE("1,234.5")'), 1234.5); eq('EXACT', ev('EXACT("a","A")'), false);
eq('TEXT number', ev('TEXT(1234.567,"#,##0.00")'), '1,234.57'); eq('TEXT pct', ev('TEXT(0.256,"0.0%")'), '25.6%'); eq('TEXT date', ev('TEXT(DATE(2026,11,1),"yyyy-mm-dd")'), '2026-11-01');
eq('TEXT long date', ev('TEXT(DATE(2026,11,1),"dddd, mmmm d, yyyy")'), 'Sunday, November 1, 2026'); eq('TEXT time', ev('TEXT(TIME(14,5,0),"h:mm AM/PM")'), '2:05 PM');
eq('number & text', ev('1/4&""'), '0.25'); eq('FIXED', ev('FIXED(1234.5,1)'), '1,234.5'); eq('DOLLAR', ev('DOLLAR(1234.5)'), '$1,234.50');
// dates
eq('DATE serial (Excel 46327)', ev('DATE(2026,11,1)'), 46327); eq('YEAR/MONTH/DAY', [ev('YEAR(46327)'), ev('MONTH(46327)'), ev('DAY(46327)')], [2026, 11, 1]);
eq('DATE month overflow', ev('DATE(2026,13,1)'), ev('DATE(2027,1,1)')); eq('WEEKDAY', ev('WEEKDAY(DATE(2026,11,1))'), 1); eq('WEEKDAY 2', ev('WEEKDAY(DATE(2026,11,1),2)'), 7);
eq('EDATE end of month', ev('EDATE(DATE(2026,1,31),1)'), ev('DATE(2026,2,28)')); eq('EOMONTH', ev('EOMONTH(DATE(2024,2,10),0)'), ev('DATE(2024,2,29)'));
eq('DATEDIF Y', ev('DATEDIF(DATE(2000,5,10),DATE(2026,5,9),"Y")'), 25); eq('DATEDIF M', ev('DATEDIF(DATE(2026,1,15),DATE(2026,3,14),"M")'), 1); eq('DATEDIF D', ev('DATEDIF(DATE(2026,1,1),DATE(2026,3,1),"D")'), 59);
eq('NETWORKDAYS', ev('NETWORKDAYS(DATE(2026,11,2),DATE(2026,11,13))'), 10); eq('WORKDAY', ev('WORKDAY(DATE(2026,11,6),1)'), ev('DATE(2026,11,9)'));
eq('date text in maths', ev('"2026-11-02"-DATE(2026,11,1)'), 1); eq('TIME/HOUR/MINUTE', [ev('HOUR(TIME(13,45,10))'), ev('MINUTE(TIME(13,45,10))'), ev('SECOND(TIME(13,45,10))')], [13, 45, 10]);
ok('TODAY', Math.abs(ev('TODAY()') - (Math.floor((Date.now() - new Date().getTimezoneOffset() * 6e4 - Date.UTC(1899, 11, 30)) / 864e5))) <= 1);
// finance (values from Excel)
eq('PMT', ev('ROUND(PMT(0.05/12,360,200000),2)'), -1073.64); eq('FV', ev('ROUND(FV(0.06/12,10,-200,-500,1),2)'), 2581.40);
eq('PV', ev('ROUND(PV(0.08/12,240,500),2)'), -59777.15); eq('NPER', ev('ROUND(NPER(0.01,-100,1000),4)'), 10.5886);
eq('NPV', ev('ROUND(NPV(0.1,-10000,3000,4200,6800),2)'), 1188.44); eq('IRR', ev('ROUND(IRR({-70000,12000,15000,18000,21000,26000}),4)'), 0.0866);
eq('RATE', ev('ROUND(RATE(48,-200,8000),6)'), 0.007701);
// formats
const F = SPE.fmtValue;
eq('fmt General', [F(0.1 + 0.2), F(1e20), F(1 / 3)], ['0.3', '1E+20', '0.333333333333'.slice(0, 13)]);
eq('fmt #,##0.00', F(1234567.891, '#,##0.00'), '1,234,567.89'); eq('fmt neg section', F(-5, '0.00;(0.00)'), '(5.00)'); eq('fmt currency', F(-1234.5, '$#,##0.00'), '-$1,234.50');
eq('fmt 0%', F(0.123, '0%'), '12%'); eq('fmt text @', F('x', '"Name: "@'), 'Name: x'); eq('fmt sci', F(12345, '0.00E+00'), '1.23E+04'); eq('fmt 0.0#', [F(1.5, '0.0#'), F(1.234, '0.0#')], ['1.5', '1.23']);
eq('fmt date m/d/yyyy', F(46327, 'm/d/yyyy'), '11/1/2026'); eq('fmt mmm', F(46327, 'd mmm yy'), '1 Nov 26'); eq('fmt h:mm', F(0.5 + 1 / 1440, 'h:mm'), '12:01');
eq('fmt error', F({ e: '#N/A' }), '#N/A'); eq('fmt bool', F(true), 'TRUE');
// copying formulas
eq('copy relative', SPE.copyFormula('=A1+$B$1+B$2+$C3', 2, 1), '=B3+$B$1+C$2+$C5'); eq('copy sheet ref', SPE.copyFormula("=Data!A1*'My S'!B2", 1, 0), "=Data!A2*'My S'!B3");
eq('copy off the sheet', SPE.copyFormula('=A1', -1, 0), '=#REF!'); eq('copy leaves functions and strings', SPE.copyFormula('=LOG10(A1)&"A1"', 1, 0), '=LOG10(A2)&"A1"'.replace('"A2"', '"A1"'));
// names
{ const b = book({ A1: 3, A2: 4, B1: '=SUM(Nums)*Rate' }); b.names = { NUMS: { ref: 'A1:A2', sheet: 'Sheet1' }, RATE: { ref: 'A1', sheet: 'Sheet1' } }; eq('named ranges', val(b, 'B1'), 21) }
// no eval/Function in the engine
ok('no eval or Function', !/\beval\s*\(|new Function|\bFunction\s*\(/.test(src));
eq('descriptions for every function', Object.keys(SPE.F).filter(k => !SPE.D[k]), []);
ok('at least 150 functions', Object.keys(SPE.F).length >= 150, Object.keys(SPE.F).length);
// speed: 100,000 rows, each with a formula
{
  const s = { name: 'Big', id: 1, cells: new Map() }, b = { sheets: [s], names: {} };
  for (let r = 0; r < 100000; r++) { s.cells.set(r * SPE.MAXC, { v: r % 97 }); s.cells.set(r * SPE.MAXC + 1, { f: r ? '=B' + r + '+A' + (r + 1) + '*2' : '=A1*2' }); s.cells.set(r * SPE.MAXC + 2, { f: '=IF(A' + (r + 1) + '>50,"hi","lo")' }) }
  s.cells.set(3, { f: '=SUM(A1:A100000)' }); s.cells.set(4, { f: '=VLOOKUP(96,A1:B100000,2,FALSE)' });
  const m0 = process.memoryUsage().heapUsed; let t = Date.now(); SPE.calc(b); const t1 = Date.now() - t;
  t = Date.now(); SPE.calc(b); const t2 = Date.now() - t; const mem = (process.memoryUsage().heapUsed - m0) / 1048576;
  console.log(`100k rows: first ${t1} ms (parse + calc), recalc ${t2} ms, ${mem.toFixed(0)} MB more`);
  ok('100k rows recalculated within 1 s', t2 < 1000, t2); const last = s.cells.get(99999 * SPE.MAXC + 1).val; ok('...deep chain right (no stack overflow)', typeof last == 'number' && last > 0, last);
  eq('...SUM', s.cells.get(3).val, Array.from({ length: 100000 }, (_, i) => i % 97).reduce((a, b) => a + b, 0));
}
// recalculating only what a change reaches
{
  const b = book({ A1: 1, A2: 2, B1: '=A1*2', B2: '=SUM(A1:A2)', C1: '=B1+1', D1: '=RAND()', E1: '=A2*5' }); SPE.recalc(b, null);
  const cell = a => { const p = SPE.parseA1(a); return b.sheets[0].cells.get(p.r * SPE.MAXC + p.c) };
  const d0 = cell('D1').val, e0g = cell('E1')._g; cell('A1').v = 10; SPE.recalc(b, [[b.sheets[0], 0, 0]]);
  eq('recalc: dependents', [cell('B1').val, cell('B2').val, cell('C1').val], [20, 12, 21]); ok('recalc: volatile again', cell('D1').val !== d0);
  eq('recalc: others kept', cell('E1').val, 10);
  const s = { name: 'Big', id: 9, cells: new Map() }, bb = { sheets: [s], names: {} };
  for (let r = 0; r < 100000; r++) { s.cells.set(r * SPE.MAXC, { v: r % 97 }); s.cells.set(r * SPE.MAXC + 1, { f: '=A' + (r + 1) + '*2' }) }
  s.cells.set(2, { f: '=SUM(B1:B100000)' }); SPE.recalc(bb, null); s.cells.get(500 * SPE.MAXC).v = 1000; let t = Date.now(); SPE.recalc(bb, [[s, 500, 0]]); t = Date.now() - t;
  console.log(`one change in 100k rows: ${t} ms`); ok('one change in 100k rows: within 100 ms', t < 100, t); eq('...the total follows', s.cells.get(2).val, (Array.from({ length: 100000 }, (_, i) => i % 97).reduce((a, b) => a + b, 0) - 500 % 97 + 1000) * 2);
}
console.log(`${pass}/${pass + fail} passed`); process.exit(fail ? 1 : 0);
