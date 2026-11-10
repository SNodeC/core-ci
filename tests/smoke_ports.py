"""Fixture boundary: four live port selections must be distinct and released together."""
import socket
from smoke import ports

for _ in range(100):
    selected = ports(4)
    assert len(set(selected)) == 4
    sockets = []
    try:
        for number in selected:
            sock = socket.socket()
            sockets.append(sock)
            sock.bind(('127.0.0.1', number))
    finally:
        for sock in sockets:
            sock.close()
print('100 groups: four distinct ephemeral ports, all released for their fixture listeners')
