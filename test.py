from websockets.sync.client import connect
import time

def hello():
    with connect("ws://localhost:8080/ws") as websocket:
        websocket.send("Hello world!")
        while True:
            message = websocket.recv()
            print(f"Received: {message}")

hello()
