#!/usr/bin/env python3
# GPL-2.0-or-later. Usage: mkyomi.py PYKAKASI/src/data www/lib/ja-yomi.txt
# Build www/lib/ja-yomi.txt (kanji word -> hiragana reading) from the KAKASI / UniDic dictionaries shipped with
# pykakasi (GPL-3.0-or-later). Lines: WORD<TAB>READING[<TAB>T], T = okurigana class of the next kana.
import sys,re
src=sys.argv[1];out=sys.argv[2];seen=set();rows=[]
CJK=re.compile('[㐀-鿿豈-﫿々〆ヶ]')
for f in ('kakasidict.utf8','unidict_noun.utf8','unidict_adj.utf8'):
    for line in open(f'{src}/{f}',encoding='utf-8'):
        if line.startswith(';;'):continue
        t=line.strip().split(' ')
        if len(t)!=2:continue          # entries that need a context word are skipped
        y,k=t;tail=''
        if y and y[-1]<='z':tail=y[-1];y=y[:-1]
        if not CJK.search(k) or not y or (k,tail) in seen:continue
        seen.add((k,tail));rows.append(k+'\t'+y+('\t'+tail if tail else ''))
open(out,'w',encoding='utf-8').write('\n'.join(rows)+'\n')
print(len(rows),'entries')
