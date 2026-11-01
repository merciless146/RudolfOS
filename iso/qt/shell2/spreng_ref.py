#!/usr/bin/env python3
# 1.4.5: reference results for SpreadsheetPro's formula engine, worked out here in Python with Excel's rules (round half
# away from zero, MOD takes the divisor's sign, sample STDEV, inclusive PERCENTILE, the 1900 date serials, …).
# Writes spreng_ref.json: [[formula, expected, cells], …]. spreng_ref_t.js checks the engine against it.
import json, math, random, datetime, statistics, os
random.seed(1450)
cases = []
def add(f, v, cells=None): cases.append([f, v, cells or {}])
EPOCH = datetime.date(1899, 12, 30)
serial = lambda d: (d - EPOCH).days
def xround(x, n=0):
    m = 10 ** n; y = abs(x) * m; r = math.floor(y + 0.5 + 1e-9)
    return math.copysign(r / m, x) if r else 0.0
# ---- arithmetic and precedence (Excel: unary minus binds tighter than ^, ^ is left to right) ----
for _ in range(90):
    a, b, c = random.randint(-20, 20), random.randint(1, 12), random.randint(-9, 9)
    k = random.randrange(6)
    if k == 0: add(f'={a}+{b}*{c}', a + b * c)
    elif k == 1: add(f'=({a}+{b})*{c}', (a + b) * c)
    elif k == 2: add(f'={a}/{b}', a / b)
    elif k == 3: add(f'={b}^2-{a}', b ** 2 - a)
    elif k == 4: add(f'=-{b}^2', (-b) ** 2)
    else: add(f'={a}*{b}%', a * b / 100)
add('=2^3^2', 64); add('=10-2-3', 5); add('=100/10/5', 2); add('="a"&1+2', 'a3'); add('=1+2&"x"', '3x')
for t, v in [('=1=1', True), ('=1<>1', False), ('="b">"A"', True), ('=2<10', True), ('="2"<"10"', False), ('=TRUE>1', True), ('="x"<1', False)]: add(t, v)
# ---- rounding ----
for _ in range(70):
    x = round(random.uniform(-1000, 1000), random.randint(1, 5)); n = random.randint(-2, 3)
    add(f'=ROUND({x},{n})', xround(x, n))
    m = 10 ** n
    if random.random() < .5: add(f'=ROUNDUP({x},{n})', math.copysign(math.ceil(abs(x) * m - 1e-9) / m, x) if x else 0.0)
    else: add(f'=ROUNDDOWN({x},{n})', math.copysign(math.floor(abs(x) * m + 1e-9) / m, x) if x else 0.0)
for x in [2.5, -2.5, 0.5, 1.005, 2.675, -0.5, 123.455]: add(f'=ROUND({x},{2 if x in (1.005, 2.675, 123.455) else 0})', xround(x, 2 if x in (1.005, 2.675, 123.455) else 0))
for _ in range(40):
    a = random.randint(-50, 50); b = random.choice([-7, -3, 2, 3, 5, 8])
    add(f'=MOD({a},{b})', a - b * math.floor(a / b))
    x = round(random.uniform(-50, 50), 2); add(f'=INT({x})', math.floor(x)); add(f'=TRUNC({x})', math.trunc(x))
for _ in range(20):
    x = round(random.uniform(0.1, 99), 3); add(f'=SQRT({x})', math.sqrt(x)); add(f'=LN({x})', math.log(x)); add(f'=LOG10({x})', math.log10(x))
    add(f'=ABS(-{x})', x); add(f'=EXP({x/20:.3f})', math.exp(round(x / 20, 3)))
for a, b in [(12, 18), (35, 21), (7, 5)]: add(f'=GCD({a},{b})', math.gcd(a, b)); add(f'=LCM({a},{b})', a * b // math.gcd(a, b))
for n in range(0, 11): add(f'=FACT({n})', math.factorial(n))
for x, s in [(2.1, .5), (7.3, 2), (-2.5, 1)]: add(f'=CEILING({x},{s})', math.ceil(x / s - 1e-12) * s)
# ---- statistics over a column of numbers ----
for t in range(12):
    n = random.randint(3, 12); vals = [random.randint(-20, 99) for _ in range(n)]; cells = {f'A{i+1}': v for i, v in enumerate(vals)}; r = f'A1:A{n}'
    add(f'=SUM({r})', sum(vals), cells); add(f'=AVERAGE({r})', sum(vals) / n, cells); add(f'=MEDIAN({r})', statistics.median(vals), cells)
    add(f'=MIN({r})', min(vals), cells); add(f'=MAX({r})', max(vals), cells); add(f'=COUNT({r})', n, cells)
    add(f'=STDEV({r})', statistics.stdev(vals), cells); add(f'=STDEVP({r})', statistics.pstdev(vals), cells); add(f'=VAR({r})', statistics.variance(vals), cells); add(f'=VARP({r})', statistics.pvariance(vals), cells)
    k = random.randint(1, n); add(f'=LARGE({r},{k})', sorted(vals, reverse=True)[k - 1], cells); add(f'=SMALL({r},{k})', sorted(vals)[k - 1], cells)
    p = random.choice([0, .1, .25, .5, .9, 1]); s = sorted(vals); pos = p * (n - 1); i = int(pos); add(f'=PERCENTILE({r},{p})', s[i] + (pos - i) * (s[i + 1] - s[i]) if i + 1 < n else s[i], cells)
    th = random.randint(0, 50); add(f'=COUNTIF({r},">{th}")', sum(v > th for v in vals), cells); add(f'=SUMIF({r},"<={th}")', sum(v for v in vals if v <= th), cells)
    x = vals[0]; add(f'=RANK({x},{r})', 1 + sum(v > x for v in vals), cells)
# ---- logic ----
for _ in range(20):
    a, b = random.randint(0, 9), random.randint(0, 9)
    add(f'=IF({a}>{b},"big","small")', 'big' if a > b else 'small'); add(f'=AND({a}>2,{b}<7)', a > 2 and b < 7); add(f'=OR({a}=0,{b}=0)', a == 0 or b == 0)
    add(f'=IFS({a}>=8,"A",{a}>=5,"B",TRUE,"C")', 'A' if a >= 8 else 'B' if a >= 5 else 'C'); add(f'=CHOOSE({a%3+1},"x","y","z")', 'xyz'[a % 3])
add('=IFERROR(1/0,"none")', 'none'); add('=XOR(TRUE,FALSE,TRUE)', False); add('=NOT(0)', True); add('=SWITCH(3,1,"a",3,"c","?")', 'c')
# ---- text ----
words = ['apple', 'Banana split', '  extra   spaces  ', 'MiXeD CaSe', 'rudolf os', 'a,b,c', 'x']
for w in words:
    q = '"' + w.replace('"', '""') + '"'
    add(f'=LEN({q})', len(w)); add(f'=UPPER({q})', w.upper()); add(f'=LOWER({q})', w.lower()); add(f'=TRIM({q})', ' '.join(w.split()))
    add(f'=LEFT({q},3)', w[:3]); add(f'=RIGHT({q},2)', w[-2:]); add(f'=MID({q},2,3)', w[1:4]); add(f'=REPT({q},2)', w * 2)
    add(f'=PROPER({q})', ''.join(c.upper() if i == 0 or not w[i - 1].isalpha() else c.lower() for i, c in enumerate(w)))
    add(f'=SUBSTITUTE({q},"a","@")', w.replace('a', '@')); add(f'=EXACT({q},UPPER({q}))', w == w.upper())
add('=FIND("o","rudolf os")', 4); add('=SEARCH("OS","rudolf os")', 8); add('=CONCAT("a","b",1)', 'ab1'); add('=TEXTJOIN("-",TRUE,"a","","b")', 'a-b'); add('=VALUE("3.5")', 3.5)
for x, f, out in [(1234.567, '0.00', '1234.57'), (1234.567, '#,##0', '1,235'), (0.256, '0%', '26%'), (-5, '0.0', '-5.0'), (0.5, '0.0%', '50.0%'), (1e6, '#,##0.00', '1,000,000.00')]: add(f'=TEXT({x},"{f}")', out)
# ---- dates ----
for _ in range(40):
    d = datetime.date(random.randint(1950, 2060), random.randint(1, 12), random.randint(1, 28)); s = serial(d)
    add(f'=DATE({d.year},{d.month},{d.day})', s); add(f'=YEAR({s})', d.year); add(f'=MONTH({s})', d.month); add(f'=DAY({s})', d.day)
    add(f'=WEEKDAY({s})', (d.isoweekday() % 7) + 1); add(f'=WEEKDAY({s},2)', d.isoweekday())
    m = random.randint(-30, 30); y, mo = divmod(d.month - 1 + m, 12); last = (datetime.date(d.year + y + (mo + 1) // 12, (mo + 1) % 12 + 1, 1) - datetime.timedelta(days=1)).day
    add(f'=EDATE({s},{m})', serial(datetime.date(d.year + y, mo + 1, min(d.day, last)))); add(f'=EOMONTH({s},{m})', serial(datetime.date(d.year + y, mo + 1, last)))
    e = d + datetime.timedelta(days=random.randint(0, 3000)); se = serial(e)
    add(f'=DATEDIF({s},{se},"D")', se - s); months = (e.year - d.year) * 12 + e.month - d.month - (1 if e.day < d.day else 0); add(f'=DATEDIF({s},{se},"M")', months); add(f'=DATEDIF({s},{se},"Y")', months // 12)
for _ in range(15):
    d = datetime.date(2026, random.randint(1, 12), random.randint(1, 28)); e = d + datetime.timedelta(days=random.randint(0, 60))
    add(f'=NETWORKDAYS({serial(d)},{serial(e)})', sum(1 for i in range((e - d).days + 1) if (d + datetime.timedelta(days=i)).weekday() < 5))
for h, m, s in [(0, 0, 0), (12, 30, 15), (23, 59, 59), (6, 5, 4)]: add(f'=TIME({h},{m},{s})', (h * 3600 + m * 60 + s) / 86400); add(f'=HOUR(TIME({h},{m},{s}))', h); add(f'=MINUTE(TIME({h},{m},{s}))', m)
# ---- finance (closed forms) ----
for _ in range(15):
    r = random.choice([.03, .045, .06, .08]) / 12; n = random.choice([12, 36, 60, 360]); pv = random.choice([1000, 25000, 200000])
    f_ = (1 + r) ** n; add(f'=PMT({r},{n},{pv})', -(r * pv * f_) / (f_ - 1))
    p = -round(random.uniform(50, 900), 2); add(f'=FV({r},{n},{p})', -(p * (f_ - 1) / r)); add(f'=PV({r},{n},{p})', -(p * (f_ - 1) / r) / f_)
for rate, flows in [(.1, [-10000, 3000, 4200, 6800]), (.05, [100, 200, 300]), (.08, [-500, 200, 200, 200])]:
    add(f'=NPV({rate},{",".join(map(str, flows))})', sum(v / (1 + rate) ** (i + 1) for i, v in enumerate(flows)))
# ---- lookups over a generated table ----
names = ['Ann', 'Bo', 'Cy', 'Di', 'Ed', 'Flo', 'Gus', 'Hal']
cells = {}
for i, n in enumerate(names): cells[f'A{i+1}'] = n; cells[f'B{i+1}'] = (i + 1) * 10; cells[f'C{i+1}'] = n.lower() + '@x'
for i, n in enumerate(names):
    add(f'=VLOOKUP("{n}",A1:C8,3,FALSE)', n.lower() + '@x', cells); add(f'=MATCH("{n}",A1:A8,0)', i + 1, cells); add(f'=INDEX(B1:B8,{i+1})', (i + 1) * 10, cells)
    add(f'=XLOOKUP({(i+1)*10},B1:B8,A1:A8)', n, cells); add(f'=VLOOKUP({(i+1)*10+5},B1:C8,2)', names[i].lower() + '@x', cells)
os.makedirs(os.path.dirname(os.path.abspath(__file__)), exist_ok=True)
json.dump(cases, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'spreng_ref.json'), 'w'))
print(len(cases), 'cases')
