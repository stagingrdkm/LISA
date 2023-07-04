#!/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Liberty Global Service B.V.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import http.server
import socketserver
import time

class MyHttpRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        time.sleep(100)
        self.send_response(202)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(b"Accepted")
    def do_HEAD(self):
        time.sleep(100)
        self.send_response(202)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(b"Accepted")

PORT = 8897

with socketserver.TCPServer(("", PORT), MyHttpRequestHandler) as httpd:
    print("Server started at localhost:{0}".format(PORT))
    httpd.serve_forever()