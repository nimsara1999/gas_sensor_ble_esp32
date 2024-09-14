from http.server import BaseHTTPRequestHandler, HTTPServer
import urllib.parse
import socket

class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):
    
    def do_GET(self):
        # Parse GET request path
        parsed_path = urllib.parse.urlparse(self.path)
        print(f"Received GET request for: {parsed_path.path}")
        if parsed_path.query:
            print(f"Query parameters: {parsed_path.query}")
        
        # Send response
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(b"GET request received")

    def do_POST(self):
        # Read the content length to determine the size of the incoming data
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)  # Read the post data
        
        # Print POST request details
        print(f"Received POST request on {self.path}")
        print(f"POST data: {post_data.decode('utf-8')}")
        
        # Send response
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(b"POST request received")
        
def run(server_class=HTTPServer, handler_class=SimpleHTTPRequestHandler, host="127.0.0.1", port=8080):
    server_address = ("", port)
    hostname = socket.gethostname()
    local_ip = socket.gethostbyname(hostname)
    httpd = server_class(server_address, handler_class)
    print(f"Starting http server on {local_ip}:{port}")
    httpd.serve_forever()


if __name__ == '__main__':
    run()
