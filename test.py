from websockets.sync.client import connect
import time

def hello():
    with connect("ws://localhost:8080/ws") as websocket:
        websocket.send("Hello world!")
        message = websocket.recv()
        print(f"Received: {message}")
        while True:
            time.sleep(1)

hello()
