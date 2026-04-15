"""Utility to (manually) test a websocket connection from TruckTel."""

import json
import argparse
from websockets.sync.client import connect

parser = argparse.ArgumentParser(
    prog="Websocket test", description="Reads from a websocket stream from TruckTel."
)

parser.add_argument("path", type=str, help="Websocket query path")
parser.add_argument("-p", "--port", type=int, default=8080, help="port to connect to")
parser.add_argument("--host", type=str, default="localhost", help="host to connect to")

args = parser.parse_args()

url = f"ws://{args.host}:{args.port}/api/ws/{args.path}"
print(f"Connecting to {url}...")

try:
    with connect(url) as websocket:
        while True:
            msg = websocket.recv()
            msg_json = json.loads(msg)
            msg_pretty = json.dumps(msg_json, indent=4)
            print(msg_pretty)

except Exception as e:
    print(e)
