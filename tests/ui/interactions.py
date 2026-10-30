#!/usr/bin/env python3
"""Optional real-browser check, adapted from the developer CDP script; stdlib only."""
import base64
import hashlib
import shutil
import tempfile
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import time
import urllib.request

BASE = os.environ.get('KDS_URL', 'http://127.0.0.1:8080').rstrip('/')
BROWSER = os.environ.get('KDS_BROWSER') or shutil.which('brave') or shutil.which('brave-browser') or shutil.which('chromium')
if not BROWSER:
    raise SystemExit('Brave/Chromium is required; set KDS_BROWSER to an installed executable.')
OUT=Path(os.environ.get('KDS_OUTPUT', '/tmp/kds-ui-check'))
OUT.mkdir(exist_ok=True)
class CDP:
    def __init__(self, url):
        from urllib.parse import urlsplit
        u=urlsplit(url); self.sock=socket.create_connection((u.hostname,u.port),timeout=10); self.buffer=b''; self.serial=0; self.errors=[]
        key=base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall(f'GET {u.path} HTTP/1.1\r\nHost: {u.netloc}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n'.encode())
        data=b''
        while b'\r\n\r\n' not in data: data+=self.sock.recv(65536)
        header,self.buffer=data.split(b'\r\n\r\n',1)
        assert header.startswith(b'HTTP/1.1 101 '), header
        assert base64.b64encode(hashlib.sha1((key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()) in header
    def read(self,n):
        while len(self.buffer)<n:
            chunk=self.sock.recv(65536)
            if not chunk: raise RuntimeError('websocket closed')
            self.buffer+=chunk
        out,self.buffer=self.buffer[:n],self.buffer[n:]; return out
    def call(self,method,params=None):
        self.serial+=1
        data=json.dumps({'id':self.serial,'method':method,'params':params or {}}).encode(); mask=os.urandom(4)
        header=bytes([0x81,0x80|len(data)]) if len(data)<126 else bytes([0x81,0xfe])+struct.pack('!H',len(data))
        self.sock.sendall(header+mask+bytes(b^mask[i%4] for i,b in enumerate(data)))
        message=b''
        while True:
            a,b=self.read(2); size=b&127
            if size==126: size=struct.unpack('!H',self.read(2))[0]
            elif size==127: size=struct.unpack('!Q',self.read(8))[0]
            key=self.read(4) if b&128 else None; chunk=self.read(size)
            if key: chunk=bytes(x^key[i%4] for i,x in enumerate(chunk))
            if a&15==8: raise RuntimeError('websocket close frame')
            if a&15==9: continue
            message+=chunk
            if not a&128: continue
            response=json.loads(message); message=b''
            if response.get('method')=='Runtime.exceptionThrown': self.errors.append(response)
            if response.get('id')==self.serial:
                assert 'error' not in response, response
                return response.get('result',{})
    def js(self,expression):
        r=self.call('Runtime.evaluate',{'expression':expression,'returnByValue':True,'awaitPromise':True})
        assert 'exceptionDetails' not in r, r
        return r['result'].get('value')
    def navigate(self,url,steps):
        self.call('Page.navigate',{'url':url})
        deadline=time.monotonic()+12
        while time.monotonic()<deadline:
            try:
                if self.js(f'document.readyState === "complete" && typeof cursor !== "undefined" && cursor === {steps-1} && !busy'): return
            except AssertionError: pass
            time.sleep(.05)
        raise AssertionError('page did not finish commands')
    def screenshot(self,name):
        time.sleep(.25)
        result=self.call('Page.captureScreenshot',{'format':'png','captureBeyondViewport':False})
        (OUT/name).write_bytes(base64.b64decode(result['data']))

profile = tempfile.TemporaryDirectory(prefix='kds-browser-')
log=(OUT/(Path(__file__).stem+'.log')).open('wb')
process=subprocess.Popen([BROWSER,'--headless','--no-sandbox','--disable-gpu','--disable-dev-shm-usage','--no-first-run','--disable-background-networking','--disable-component-update','--disable-sync','--no-default-browser-check','--remote-debugging-address=127.0.0.1','--remote-debugging-port=0','--user-data-dir='+profile.name,'about:blank'],stdout=log,stderr=log)
try:
    deadline=time.monotonic()+10
    while True:
        try:
            port = int((Path(profile.name)/'DevToolsActivePort').read_text().splitlines()[0]); targets=json.load(urllib.request.urlopen(f'http://127.0.0.1:{port}/json/list', timeout=2)); break
        except OSError:
            if time.monotonic()>deadline: raise
            time.sleep(.1)
    tab=next(x for x in targets if x['type']=='page'); c=CDP(tab['webSocketDebuggerUrl'])
    c.call('Page.enable'); c.call('Runtime.enable')
    urllib.request.urlopen(urllib.request.Request(BASE+'/api/restart', data=b'{}', headers={'Content-Type':'application/json'})).close()
    c.call('Emulation.setDeviceMetricsOverride',{'width':1500,'height':1350,'deviceScaleFactor':1,'mobile':False})
    c.navigate(BASE+'/?run=reset;color%20example',3)
    c.js('$("operation").value="search"; $("operation").dispatchEvent(new Event("change")); $("field-red").value="138"; $("operation-form").requestSubmit()')
    time.sleep(.15)
    assert c.js('history[cursor].command')=='color search red=138'
    assert c.js('history[cursor].effect.matched.length')==1
    assert c.js('history[cursor].effect.visited.length')==4
    c.js('$("operation").value="delete_index"; $("operation").dispatchEvent(new Event("change")); $("field-index").value="1"; $("operation-form").requestSubmit()')
    time.sleep(.1)
    assert c.js('history[cursor].command')=='color delete index=1'
    assert c.js('history[cursor].state.structures[0].count')==3
    c.js('$("operation").value="add_tail"; $("operation").dispatchEvent(new Event("change")); $("field-red").value="256"')
    assert not c.js('$("operation-form").checkValidity()')
    scale=c.js('camera.scale'); c.js('$("zoom-in").click()'); assert c.js('camera.scale')>scale
    y=c.js('camera.y'); c.js('frame.dispatchEvent(new KeyboardEvent("keydown",{key:"ArrowDown",bubbles:true}))'); assert c.js('camera.y')>y
    rect=c.js('(()=>{const r=frame.getBoundingClientRect();return {x:r.x,y:r.y}})()')
    x=c.js('camera.x')
    c.call('Input.dispatchMouseEvent',{'type':'mousePressed','x':rect['x']+100,'y':rect['y']+100,'button':'left','buttons':1,'clickCount':1})
    c.call('Input.dispatchMouseEvent',{'type':'mouseMoved','x':rect['x']+160,'y':rect['y']+100,'button':'left','buttons':1})
    c.call('Input.dispatchMouseEvent',{'type':'mouseReleased','x':rect['x']+160,'y':rect['y']+100,'button':'left','buttons':0,'clickCount':1})
    assert c.js('camera.x')<x
    theme=c.js('$("theme").getAttribute("aria-pressed")'); c.js('$("theme").click()'); assert c.js('$("theme").getAttribute("aria-pressed")')!=theme
    c.js('send("stack push value=9223372036854775807")')
    assert '9223372036854775807' in c.js('$("graph").textContent')
    c.js('stepHistory(cursor-1)'); assert c.js('$("operation-fields").disabled')
    for width in (320,390):
        c.call('Emulation.setDeviceMetricsOverride',{'width':width,'height':1100,'deviceScaleFactor':1,'mobile':True})
        assert c.js('document.documentElement.scrollWidth <= innerWidth')
    assert not c.errors,c.errors
    print('PASS UI controls: actual search/delete form submissions; RGB validation; zoom; keyboard and pointer pan; theme toggle; exact int64 display; history lock; 320/390px no overflow')
finally:
    process.terminate()
    try: process.wait(timeout=5)
    except subprocess.TimeoutExpired: process.kill(); process.wait()
    log.close()
    for attempt in range(20):
        try:
            profile.cleanup()
            break
        except OSError:
            if attempt == 19:
                raise
            time.sleep(.05)
