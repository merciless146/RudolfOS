# Drives WordPro's print dialog against printbench.py + fakeprinter.py (host test).
import sys, time
from playwright.sync_api import sync_playwright
URL = sys.argv[1]; SHOT = sys.argv[2]
def ok(n, c, d=''): print(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d else ''), flush=True)
with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page(viewport={'width': 1280, 'height': 800})
    errs = []; pg.on('pageerror', lambda e: errs.append(str(e)))
    pg.goto(URL); pg.wait_for_timeout(3000)
    pg.evaluate("()=>{let e=document.elementFromPoint(640,300);while(e&&e.parentElement!=document.body)e=e.parentElement;e&&e.remove()}"); pg.wait_for_timeout(400)
    pg.get_by_text('Guest', exact=True).click(); pg.wait_for_timeout(2500)
    pg.evaluate("wordpro()"); pg.wait_for_timeout(1500)
    pg.evaluate("""()=>{const e=document.querySelector('.wp-ed,[contenteditable=true]');let h='<h1>Quarterly report</h1><p>This is <b>bold</b>, <i>italic</i> and <span style="color:#c00">red</span> text.</p><table border=1><tr><td>Apples</td><td>12</td></tr><tr><td>Pears</td><td>7</td></tr></table>';
      for(let i=0;i<70;i++)h+='<p>Paragraph '+(i+1)+': the quick brown fox jumps over the lazy dog, again and again, to fill the pages.</p>';e.innerHTML=h;e.focus();e.dispatchEvent(new Event('input',{bubbles:true}))}""")
    pg.keyboard.press('Control+p'); pg.wait_for_timeout(500)
    ok('Ctrl+P opens the Rudolf print dialog', pg.query_selector('.prdlg') is not None)
    pg.wait_for_selector('.prdlg canvas.prpage', timeout=60000); n = pg.evaluate("document.querySelectorAll('.prdlg canvas.prpage').length")
    ok('preview shows the pages', n >= 2, n)
    pg.wait_for_function("[...document.querySelectorAll('.prdlg .pdst option')].some(o=>/Office Printer \\(pdf\\)/.test(o.textContent))", timeout=20000)
    ok('network printer found', True)
    pg.screenshot(path=SHOT + '-1.png')
    uri = pg.evaluate("[...document.querySelectorAll('.prdlg .pdst option')].find(o=>/Office Printer \\(pdf\\)/.test(o.textContent)).value")
    pg.select_option('.prdlg .pdst', uri); pg.wait_for_function("!document.querySelector('.prdlg .pgo').disabled", timeout=20000)
    ok('printer status shown', 'Ready' in pg.inner_text('.prdlg .pst'), pg.inner_text('.prdlg .pst'))
    pg.fill('.prdlg .pcop', '2'); pg.select_option('.prdlg .psid', 'two-sided-long-edge'); pg.screenshot(path=SHOT + '-2.png')
    pg.click('.prdlg .pgo'); pg.wait_for_selector('.prdlg', state='detached', timeout=60000); ok('PDF printer job sent', True)
    # raster-only printer added by address
    pg.keyboard.press('Control+p'); pg.wait_for_selector('.prdlg canvas.prpage', timeout=60000); pg.wait_for_timeout(500)
    pg.select_option('.prdlg .pdst', 'add'); pg.wait_for_selector('.rmbox input.t'); pg.fill('.rmbox input.t', '127.0.0.1:8632'); pg.keyboard.press('Enter')
    pg.wait_for_function("document.querySelector('.prdlg .pdst').value.includes('8632')&&!document.querySelector('.prdlg .pgo').disabled", timeout=30000)
    pg.select_option('.prdlg .pcol', 'monochrome'); pg.click('.prdlg .prng'); pg.fill('.prdlg .pfrom', '1'); pg.fill('.prdlg .pto', '2')
    t0 = time.time(); pg.click('.prdlg .pgo'); pg.wait_for_selector('.prdlg', state='detached', timeout=180000); ok('raster printer job sent', True, f'{time.time()-t0:.1f}s')
    # save as PDF
    pg.keyboard.press('Control+p'); pg.wait_for_selector('.prdlg canvas.prpage', timeout=60000)
    pg.select_option('.prdlg .pdst', 'pdf'); pg.wait_for_timeout(200); pg.click('.prdlg .pgo'); pg.wait_for_timeout(500)
    ok('Save as PDF offers the save dialog', 'Save as PDF' in pg.inner_text('body'), '')
    ok('no page errors', not errs, errs[:3]); b.close()
