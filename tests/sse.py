"""Real socket pressure against an isolated controller, without production hooks."""
import copy
import http.client
import json
import socket
import subprocess
import time


def read_event(events):
    line = events.readline()
    while line.startswith(b":") or line in (b"\n", b"\r\n"):
        line = events.readline()
    assert line.startswith(b"data: "), line[:100]
    assert events.readline() in (b"\n", b"\r\n")
    return json.loads(line[6:]), len(line) + 1


def apply(view, changes):
    for change in changes:
        tokens = [token.replace("~1", "/").replace("~0", "~") for token in change["path"].split("/")[1:]]
        parent = view
        for token in tokens[:-1]:
            parent = parent[int(token)] if isinstance(parent, list) else parent[token]
        token = tokens[-1]
        if isinstance(parent, list):
            index = len(parent) if token == "-" else int(token)
            if change["op"] == "add":
                parent.insert(index, change["value"])
            elif change["op"] == "remove":
                parent.pop(index)
            else:
                parent[index] = change["value"]
        elif change["op"] == "remove":
            del parent[token]
        else:
            parent[token] = change["value"]


def read_snapshot(events):
    view, size = read_event(events)
    assert view["ready"] is False and view["executions"] == {}
    sizes = [size]
    while not view["ready"]:
        changes, size = read_event(events)
        assert isinstance(changes, list)
        apply(view, changes)
        sizes.append(size)
    return view, sizes


def check_delivery(port, graph, seed):
    fixture = copy.deepcopy(graph)
    template = fixture["executions"][-1]
    count = 3000
    for index in range(count):
        work = copy.deepcopy(template)
        work["id"] = f"pressure-{index}"
        active = work["attempts"][-1]
        active.update(id=f"pressure-{index}:1", state="publishing", revision=1)
        active.pop("retirement", None)
        active.pop("failure_kind", None)
        for key in ["publish_run", "publish_run_id"]:
            active["facts"].pop(key, None)
        active["assignment"]["execution"] = work["id"]
        active["assignment"]["attempt"] = active["id"]
        work["attempts"] = [active]
        fixture["executions"].append(work)
    seed(fixture)
    slow = socket.socket()
    slow.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
    slow.settimeout(5)
    slow.connect(("127.0.0.1", port))
    slow_port = slow.getsockname()[1]
    slow.sendall(b"GET /events HTTP/1.1\r\nHost: localhost\r\n\r\n")
    headers = b""
    while not headers.endswith(b"\r\n\r\n"):
        headers += slow.recv(1)
    assert b"200 OK" in headers
    healthy = http.client.HTTPConnection("127.0.0.1", port, timeout=40)
    healthy.request("GET", "/events")
    events = healthy.getresponse()
    assert events.status == 200
    started = time.monotonic()
    sizes = []
    try:
        peers = subprocess.check_output(["ss", "-tn", "state", "established", f"( sport = :{port} )"], text=True)
        assert f"127.0.0.1:{slow_port}" in peers, "Initial snapshot allowance must keep the slow peer connected"
        changes_count = 24
        for index in range(changes_count):
            request = http.client.HTTPConnection("127.0.0.1", port, timeout=40)
            body = json.dumps({"execution": f"pressure-{index}", "acknowledgement": "Acknowledged unknown outcome. " + "a" * 3900})
            request.request("POST", "/api/retire", body, {"Content-Type": "application/json"})
            response = request.getresponse()
            assert response.status == 200 and not json.loads(response.read())["error"]
            request.close()
            if index == 0:
                displayed, initial_sizes = read_snapshot(events)
                assert len(displayed["executions"]) == len(fixture["executions"])
                assert displayed["executions"]["pressure-0"]["active"]["state"] == "publishing", "Initial snapshot stays consistent"
            changes, size = read_event(events)
            sizes.append(size)
            assert changes and size < 5000
            assert all(change["path"].startswith(f"/executions/pressure-{index}/active/") for change in changes)
            apply(displayed, changes)
            assert displayed["executions"][f"pressure-{index}"]["active"]["failure_kind"] == "retirement"
        peers = subprocess.check_output(["ss", "-tn", "state", "established", f"( sport = :{port} )"], text=True)
        assert f"127.0.0.1:{slow_port}" not in peers, "Over-limit SSE peer must close without draining the initial backlog"
        again = http.client.HTTPConnection("127.0.0.1", port, timeout=40)
        again.request("GET", "/events")
        refreshed = again.getresponse()
        snapshot, _ = read_snapshot(refreshed)
        assert snapshot == displayed, "Reconnect reconstructs exact current compact state"
        refreshed.close()
        again.close()
        print(f"Real compact SSE: {sum(initial_sizes):,} startup event bytes, largest {max(initial_sizes)}; "
              f"{changes_count} ordered updates of {min(sizes)}–{max(sizes)} bytes; "
              f"slow peer closed, healthy peer/reconnect correct; elapsed {time.monotonic() - started:.2f}s")
    finally:
        events.close()
        healthy.close()
        slow.close()
        seed(graph)
