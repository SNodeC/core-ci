"""Real socket pressure against an isolated controller, without production hooks."""
import copy
import http.client
import json
import socket
import subprocess
import time


def check_delivery(port, graph, seed):
    fixture = copy.deepcopy(graph)
    template = fixture["executions"][-1]
    count = 24
    for index in range(count):
        work = copy.deepcopy(template)
        work["id"] = f"pressure-{index}"
        work["attempts"] = [{"id": f"pressure-{index}:1", "state": "publishing", "revision": 1,
                              "created": "2026-10-31T09:00:00Z", "facts": {}}]
        fixture["executions"].append(work)
    repository = next(iter(fixture["repositories"].values()))
    repository["definition"]["fixture_padding"] = "x" * (4 * 1024 * 1024)
    seed(fixture)
    slow = socket.socket()
    slow.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
    slow.settimeout(5)
    slow.connect(("127.0.0.1", port))
    slow_port = slow.getsockname()[1]
    slow.sendall(b"GET /events HTTP/1.1\r\nHost: localhost\r\n\r\n")
    # Read only headers; leave the deliberately large initial snapshot blocked.
    headers = b""
    while not headers.endswith(b"\r\n\r\n"):
        headers += slow.recv(1)
    assert b"200 OK" in headers
    healthy = http.client.HTTPConnection("127.0.0.1", port, timeout=15)
    healthy.request("GET", "/events")
    events = healthy.getresponse()
    assert events.status == 200
    started = time.monotonic()
    sizes = []
    try:
        for index in range(count):
            request = http.client.HTTPConnection("127.0.0.1", port, timeout=15)
            body = json.dumps({"execution": f"pressure-{index}", "acknowledgement": "Acknowledged unknown outcome. " + "a" * 3900})
            request.request("POST", "/api/retire", body, {"Content-Type": "application/json"})
            response = request.getresponse()
            assert response.status == 200 and not json.loads(response.read())["error"]
            request.close()
            if index == 0:
                initial = events.readline()
                assert initial.startswith(b"data: ") and json.loads(initial[6:]) == fixture
                assert events.readline() == b"\n"
            update = events.readline()
            while update.startswith(b":") or update in (b"\n", b"\r\n"):
                update = events.readline()
            assert update.startswith(b"data: "), update[:100]
            changes = json.loads(update[6:])
            sizes.append(len(update))
            assert isinstance(changes, list) and changes and len(update) < 5000
            assert all(change["path"].startswith(f"/executions/{index + len(graph['executions'])}/attempts/") for change in changes)
            assert events.readline() == b"\n"
        assert time.monotonic() - started < 25, "Slow reader must not stall independent committed operations or delivery"
        peers = subprocess.check_output(["ss", "-tn", "state", "established", f"( sport = :{port} )"], text=True)
        assert f"127.0.0.1:{slow_port}" not in peers, "Over-limit SSE peer must be closed without draining its backlog"
        again = http.client.HTTPConnection("127.0.0.1", port, timeout=15)
        again.request("GET", "/events")
        refreshed = again.getresponse()
        snapshot = json.loads(refreshed.readline()[6:])
        assert all(work["attempts"][-1]["state"] == "failed" for work in snapshot["executions"][-count:])
        again.close()
        print(f"Real SSE: {len(initial):,}-byte initial snapshot; {count} ordered updates of {min(sizes)}–{max(sizes)} bytes; "
              f"slow peer disconnected, healthy peer/reconnect correct; elapsed {time.monotonic() - started:.2f}s")
    finally:
        healthy.close()
        slow.close()
        seed(graph)
