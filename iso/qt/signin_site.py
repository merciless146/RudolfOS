# fake websites for the Oracle sign-in test (any *.test host, port 8082)
import http.server,threading
PAGES={
'/':"<title>Home</title><h1>Fake site</h1><a id=login href='/login'>Sign in</a> <button id=google onclick=\"window.open('https://accounts.google.com/o/oauth2/v2/auth?client_id=x','_blank','width=500')\">Sign in with Google</button>",
'/login':"<title>Sign in</title><form id=f method=post action='/session'><input name=u value=me><input id=pw type=password name=p><button id=go>Sign in</button></form>",
'/spa':"<title>App</title><div id=box></div><script>setTimeout(()=>{document.getElementById('box').innerHTML='<input id=pw type=password>'},300)</script>",
'/xfo':"<title>Framed</title><p id=ok>shown</p>",
}
class H(http.server.BaseHTTPRequestHandler):
    def log_message(s,*a):pass
    def do_POST(s):
        s.send_response(200);s.send_header('Content-Type','text/html');s.end_headers();s.wfile.write(b"<title>Posted</title><p id=posted>posted</p>")
    def do_GET(s):
        p=s.path.split('?')[0];b=PAGES.get(p)
        s.send_response(200 if b else 404);s.send_header('Content-Type','text/html')
        if p=='/xfo':s.send_header('X-Frame-Options','DENY');s.send_header('Content-Security-Policy',"frame-ancestors 'none'")
        s.end_headers();s.wfile.write((b or 'nope').encode())
http.server.ThreadingHTTPServer(('',8082),H).serve_forever()
