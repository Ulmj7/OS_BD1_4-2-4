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
    assert c.js('document.querySelectorAll("#graph .edge").length')==10
    assert c.js('document.querySelectorAll("#graph .node").length')==4
    assert c.js('document.documentElement.scrollWidth <= innerWidth')
    c.screenshot('01_color_example.png')
    c.js('send("color delete index=1")')
    assert c.js('history[cursor].state.structures[0].count')==3
    c.screenshot('02_color_delete.png')
    c.js('stepHistory(cursor-1)')
    assert c.js('document.getElementById("operation-fields").disabled && document.getElementById("reset").disabled && !document.getElementById("history-notice").hidden')
    c.js('stepHistory(history.length-1)')
    assert not c.js('document.getElementById("operation-fields").disabled')
    c.navigate(BASE+'/?run=reset;collatz%20run%20start=27&theme=dark',3)
    assert c.js('document.querySelectorAll("#graph .node").length')==112
    assert c.js('document.querySelectorAll("#graph .edge").length')==226
    c.screenshot('03_collatz_27_dark.png')
    c.js('send("collatz run start=0")')
    assert c.js('document.getElementById("message").classList.contains("error")')
    c.screenshot('04_error.png')
    c.navigate(BASE+'/?run=reset;stack%20push%20value=10;stack%20push%20value=42;stack%20push%20value=-2',5)
    assert 'TOP' in c.js('document.getElementById("graph").textContent')
    c.screenshot('05_stack.png')
    c.js('send("queue enqueue value=10").then(()=>send("queue enqueue value=42"))')
    assert 'FRONT' in c.js('document.getElementById("graph").textContent') and 'REAR' in c.js('document.getElementById("graph").textContent')
    c.screenshot('06_queue.png')
    c.call('Emulation.setDeviceMetricsOverride',{'width':390,'height':1100,'deviceScaleFactor':1,'mobile':True})
    c.navigate(BASE+'/?run=reset;color%20example',3)
    assert c.js('document.documentElement.scrollWidth <= innerWidth')
    c.screenshot('07_mobile.png')
    assert not c.errors, c.errors
    print('PASS browser: real Color/Collatz/Stack/Queue graph; 112 Collatz nodes/226 arrows; read-only history; error state; mobile no overflow; no JS exceptions; 7 screenshots in',OUT)
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
