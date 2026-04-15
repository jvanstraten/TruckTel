import sys
import json
from websockets.sync.client import connect

with connect("ws://localhost:8080/api/ws/" + sys.argv[1] if len(sys.argv) > 1 else "") as websocket:
    while True:
        msg = websocket.recv()
        msg_json = json.loads(msg)
        msg_pretty = json.dumps(msg_json, indent=4)
        print(msg_pretty)
