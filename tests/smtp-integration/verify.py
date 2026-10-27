"""Opt-in local SMTP/TLS checks. Never connects to a real mail provider."""
import argparse
import base64
import email.policy
import email.parser
import hashlib
import http.server
import json
import os
from pathlib import Path
import socket
import socketserver
import ssl
import subprocess
import threading
import time

USER = 'fixture-user'
PASSWORD = 'synthetic-CREDENTIAL-MARKER:"\\-only'
SUBJECT = 'Deployment café 中文 😀 ' * 8
BODY = 'Hello café 中文 😀\n.\ntrailing space \n'


def generate_certificates(directory):
    directory.mkdir(parents=True, exist_ok=False)
    def openssl(*args):
        subprocess.run(['openssl', *map(str, args)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    openssl('req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '2', '-keyout', directory / 'ca.key', '-out', directory / 'ca.pem', '-subj', '/CN=TATR fixture CA', '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=critical,keyCertSign,cRLSign')
    openssl('req', '-new', '-newkey', 'rsa:2048', '-nodes', '-keyout', directory / 'server.key', '-out', directory / 'server.csr', '-subj', '/CN=localhost')
    extension = directory / 'server.ext'
    extension.write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost,IP:127.0.0.1\ncrlDistributionPoints=URI:http://127.0.0.1:18081/ca.crl\n', encoding='ascii')
    openssl('x509', '-req', '-in', directory / 'server.csr', '-CA', directory / 'ca.pem', '-CAkey', directory / 'ca.key', '-CAcreateserial', '-out', directory / 'server.pem', '-days', '2', '-extfile', extension)
    (directory / 'index.txt').write_text('', encoding='ascii')
    (directory / 'crlnumber').write_text('1000\n', encoding='ascii')
    config = directory / 'ca.conf'
    config.write_text('[ca]\ndefault_ca = local\n[local]\ndatabase = ' + str(directory / 'index.txt') + '\nprivate_key = ' + str(directory / 'ca.key') + '\ncertificate = ' + str(directory / 'ca.pem') + '\ndefault_md = sha256\ndefault_crl_days = 2\ncrlnumber = ' + str(directory / 'crlnumber') + '\n', encoding='ascii')
    openssl('ca', '-gencrl', '-config', config, '-out', directory / 'ca.crl.pem')
    openssl('crl', '-in', directory / 'ca.crl.pem', '-outform', 'DER', '-out', directory / 'ca.crl')



class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True
    block_on_close = False

    def __init__(self, certificates, mode, authentication=True):
        self.mode = mode
        self.authentication = authentication
        self.messages = []
        self.authenticated = False
        self.auth_attempted = False
        self.recipient_attempted = False
        self.tls_used = False
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version = ssl.TLSVersion.TLSv1_2
        self.context.load_cert_chain(certificates / 'server.pem', certificates / 'server.key')
        super().__init__(('127.0.0.1', 0), Handler)

    def handle_error(self, request, client_address):
        # Deliberate TLS/timeout/disconnect errors contain no useful test diagnostics.
        pass


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        connection = self.request
        connection.settimeout(8)
        reader = None
        def send(value):
            connection.sendall(value.encode('ascii'))
        try:
            if self.server.mode == 'implicit':
                connection = self.server.context.wrap_socket(connection, server_side=True)
                self.server.tls_used = True
            reader = connection.makefile('rb')
            if self.server.mode == 'timeout':
                time.sleep(3)
                return
            send('220 localhost TATR fixture\r\n')
            authenticated = not self.server.authentication
            while True:
                raw = reader.readline(65537)
                if not raw:
                    return
                command = raw.decode('ascii').rstrip('\r\n')
                verb, _, rest = command.partition(' ')
                verb = verb.upper()
                if verb in ('EHLO', 'HELO'):
                    send('250-localhost\r\n')
                    if not self.server.tls_used and self.server.mode != 'no_starttls':
                        send('250-STARTTLS\r\n')
                    send('250 AUTH PLAIN LOGIN\r\n')
                elif verb == 'STARTTLS':
                    send('220 Ready for TLS\r\n')
                    reader.close()
                    connection = self.server.context.wrap_socket(connection, server_side=True)
                    reader = connection.makefile('rb')
                    self.server.tls_used = True
                elif verb == 'AUTH':
                    self.server.auth_attempted = True
                    mechanism, _, value = rest.partition(' ')
                    if mechanism.upper() == 'PLAIN':
                        if not value:
                            send('334 \r\n')
                            value = reader.readline().decode('ascii').strip()
                        fields = base64.b64decode(value).split(b'\0')
                        username, password = fields[-2].decode('utf-8'), fields[-1].decode('utf-8')
                    elif mechanism.upper() == 'LOGIN':
                        if value:
                            username = base64.b64decode(value).decode('utf-8')
                        else:
                            send('334 VXNlcm5hbWU6\r\n')
                            username = base64.b64decode(reader.readline().strip()).decode('utf-8')
                        send('334 UGFzc3dvcmQ6\r\n')
                        password = base64.b64decode(reader.readline().strip()).decode('utf-8')
                    else:
                        send('504 Unsupported authentication\r\n')
                        continue
                    authenticated = (self.server.mode != 'auth_rejected' and username == USER and password == PASSWORD)
                    self.server.authenticated = authenticated
                    send('235 Authenticated\r\n' if authenticated else '535 Authentication rejected\r\n')
                elif verb == 'MAIL':
                    send('250 Sender accepted\r\n' if authenticated else '530 Authentication required\r\n')
                elif verb == 'RCPT':
                    self.server.recipient_attempted = True
                    send('550 Recipient rejected\r\n' if self.server.mode == 'recipient_rejected' and 'second@' in rest else '250 Recipient accepted\r\n')
                elif verb == 'DATA':
                    send('354 End with dot\r\n')
                    message = bytearray()
                    while True:
                        line = reader.readline(65537)
                        if line in (b'.\r\n', b'.\n'):
                            break
                        if not line:
                            return
                        if line.startswith(b'..'):
                            line = line[1:]
                        message.extend(line)
                    self.server.messages.append(bytes(message))
                    send('250 Message accepted\r\n')
                elif verb == 'QUIT':
                    send('221 Bye\r\n')
                    return
                elif verb in ('RSET', 'NOOP'):
                    send('250 OK\r\n')
                else:
                    send('502 Command unsupported\r\n')
        finally:
            if reader:
                reader.close()
            connection.close()


def quote(value):
    return json.dumps(str(value), ensure_ascii=False)


def run_case(args, name, mode, expected_status, *, auth=True, trust=True, hook=False, policy='warn', task_fails=False):
    directory = args.output / name
    directory.mkdir()
    temporary = (Path('/tmp') / ('tatr-smtp-live-' + hashlib.sha256(str(directory).encode()).hexdigest())) if os.name != 'nt' else directory / 'private temp'
    temporary.mkdir()
    server = Server(args.certificates, mode, auth)
    serving = threading.Thread(target=server.serve_forever, daemon=True)
    serving.start()
    try:
        port = server.server_address[1]
        if mode == 'refused':
            with socket.socket() as reserve:
                reserve.bind(('127.0.0.1', 0))
                port = reserve.getsockname()[1]
        config = 'version = 1\n'
        section = 'tasks.mail'
        if hook:
            config += '[tasks.deploy]\n'
            if task_fails:
                config += 'exec = [' + ', '.join(map(quote, [args.child, 'exit', '7'])) + ']\n'
            config += '[[hooks]]\nname = "email"\nwhen = "always"\ntargets = ["deploy"]\non_error = ' + quote(policy) + '\n'
            section = 'hooks'
        config += f'[{section}.smtp]\nhost = "127.0.0.1"\nport = {port}\ntls = ' + quote('implicit' if mode == 'implicit' else 'starttls') + '\n'
        config += 'from = "sender@example.com"\nto = ["first@example.com", "second@example.com"]\nsubject = ' + quote(SUBJECT.strip()) + '\nclient = ' + quote(args.curl) + '\n'
        config += 'connect_timeout_seconds = ' + ('5' if mode == 'refused' else '1') + '\ntimeout_seconds = ' + ('1' if mode == 'timeout' else '5') + '\n'
        if not hook:
            config += 'body = ' + quote(BODY) + '\n'
        if trust:
            config += 'ca_file = ' + quote(args.certificates / 'ca.pem') + '\n'
        if auth:
            config += f'[{section}.smtp.auth]\nusername_env = "SMTP_USERNAME"\npassword_env = "SMTP_PASSWORD"\n'
        path = directory / 'tatr.toml'
        path.write_text(config, encoding='utf-8')
        environment = os.environ.copy()
        environment.update(SMTP_USERNAME=USER, SMTP_PASSWORD=PASSWORD, TMPDIR=str(temporary), TMP=str(temporary), TEMP=str(temporary))
        # Deliberately hostile defaults must not affect an SMTP action.
        curl_home = directory / 'curl-home'
        curl_home.mkdir()
        (curl_home / '.curlrc').write_text('proxy = "http://127.0.0.1:1"\n', encoding='ascii')
        environment.update(CURL_HOME=str(curl_home), ALL_PROXY='http://127.0.0.1:1', HTTPS_PROXY='http://127.0.0.1:1')
        if not trust:
            for key in ('CURL_CA_BUNDLE', 'SSL_CERT_FILE', 'SSL_CERT_DIR'):
                environment.pop(key, None)
        result = subprocess.run([str(args.tatr), '--file', str(path), 'run', 'deploy' if hook else 'mail'], env=environment, capture_output=True, timeout=15)
        output = result.stdout + result.stderr
        if PASSWORD.encode() in output or b'CREDENTIAL-MARKER' in output:
            raise AssertionError('SMTP credential appeared in output')
        (directory / 'stdout.log').write_bytes(result.stdout)
        (directory / 'actions.log').write_bytes(result.stderr)
        if result.returncode != expected_status:
            raise AssertionError(f'expected exit {expected_status}, got {result.returncode}; see {directory / "actions.log"}')
        expected_client = {'auth_rejected': 67, 'recipient_rejected': 55, 'no_starttls': 64,
                           'untrusted': 60, 'timeout': 28, 'refused': 7}.get(mode)
        if expected_client is not None and f'exit_code={expected_client}'.encode() not in result.stderr:
            raise AssertionError(f'wrong SMTP failure stage; expected curl {expected_client}, see actions.log')
        if mode == 'auth_rejected' and not server.auth_attempted:
            raise AssertionError('authentication failure test did not reach authentication')
        if mode == 'recipient_rejected' and not server.recipient_attempted:
            raise AssertionError('recipient failure test did not reach recipient handling')
        accepted = mode in ('starttls', 'implicit')
        if accepted:
            if len(server.messages) != 1 or not server.tls_used or (auth and not server.authenticated):
                raise AssertionError('expected exactly one accepted message over verified TLS/auth')
            raw = server.messages[0]
            message = email.parser.BytesParser(policy=email.policy.default).parsebytes(raw)
            if message.defects or str(message['Subject']) != SUBJECT.strip() or not message['Date'] or not message['Message-ID']:
                raise AssertionError('invalid MIME headers or encoded subject')
            content = message.get_content().replace('\r\n', '\n')
            if hook:
                expected_outcome = 'child_failure' if task_fails else 'success'
                if 'Target: deploy' not in content or f'Outcome: {expected_outcome}' not in content:
                    raise AssertionError('hook message lost original result')
            elif content != BODY:
                raise AssertionError('decoded MIME body changed')
            if [address.addr_spec for address in message['To'].addresses] != ['first@example.com', 'second@example.com']:
                raise AssertionError('recipient headers changed')
            for line in raw.split(b'\r\n'):
                if len(line) > 998:
                    raise AssertionError('MIME line exceeds hard limit')
            (directory / 'received.eml').write_bytes(raw)
        elif server.messages:
            raise AssertionError('failure scenario accepted an unexpected message')
        if list(temporary.iterdir()):
            raise AssertionError('temporary message files leaked')
        return {'scenario': name, 'result': 'PASS', 'exit': result.returncode, 'accepted': len(server.messages)}
    finally:
        server.shutdown()
        server.server_close()
        serving.join()
        if os.name != 'nt' and temporary.exists() and not list(temporary.iterdir()):
            temporary.rmdir()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--generate-certificates', type=Path)
    parser.add_argument('--tatr', type=Path)
    parser.add_argument('--child', type=Path)
    parser.add_argument('--curl', type=Path)
    parser.add_argument('--certificates', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.generate_certificates:
        generate_certificates(args.generate_certificates)
        return
    for key in ('tatr', 'child', 'curl', 'certificates', 'output'):
        value = getattr(args, key)
        if value is None:
            parser.error('missing --' + key)
        setattr(args, key, value.resolve())
    args.output.mkdir(parents=True, exist_ok=False)
    versions = subprocess.check_output([str(args.curl), '--version'], text=True)
    (args.output / 'versions.txt').write_text(versions, encoding='utf-8')
    (args.output / 'tatr.sha256').write_text(hashlib.sha256(args.tatr.read_bytes()).hexdigest(), encoding='ascii')
    scenarios = [
        ('starttls-auth', 'starttls', 0, {}),
        ('implicit-auth', 'implicit', 0, {}),
        ('relay', 'starttls', 0, {'auth': False}),
        ('auth-rejected', 'auth_rejected', 1, {}),
        ('recipient-rejected', 'recipient_rejected', 1, {}),
        ('required-starttls', 'no_starttls', 1, {}),
        ('untrusted-certificate', 'untrusted', 1, {'trust': False}),
        ('timeout', 'timeout', 1, {}),
        ('connection-refused', 'refused', 1, {}),
        ('hook-success', 'starttls', 0, {'hook': True}),
        ('hook-original-failure', 'starttls', 1, {'hook': True, 'task_fails': True}),
        ('hook-warning', 'auth_rejected', 0, {'hook': True}),
        ('hook-required', 'auth_rejected', 1, {'hook': True, 'policy': 'fail'}),
    ]
    # Schannel verifies revocation. Serve only the generated fixture CRL locally;
    # never alter system trust or disable verification to make a test pass.
    class CRLHandler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path != '/ca.crl':
                self.send_error(404)
                return
            contents = (args.certificates / 'ca.crl').read_bytes()
            self.send_response(200)
            self.send_header('Content-Type', 'application/pkix-crl')
            self.send_header('Content-Length', str(len(contents)))
            self.end_headers()
            self.wfile.write(contents)
        def log_message(self, *args):
            pass
    crl_server = http.server.ThreadingHTTPServer(('127.0.0.1', 18081), CRLHandler)
    crl_thread = threading.Thread(target=crl_server.serve_forever, daemon=True)
    crl_thread.start()
    results = []
    for name, mode, status, options in scenarios:
        try:
            result = run_case(args, name, mode, status, **options)
        except Exception as error:
            result = {'scenario': name, 'result': 'FAIL', 'reason': str(error)}
        results.append(result)
        print(json.dumps(result), flush=True)
    crl_server.shutdown()
    crl_server.server_close()
    crl_thread.join()
    (args.output / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    if any(result['result'] != 'PASS' for result in results):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
