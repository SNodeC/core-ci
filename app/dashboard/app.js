const byId = id => document.getElementById(id);
const connection = byId("connection");
const events = new EventSource("/events");
let state;

function node(tag, text) {
    const element = document.createElement(tag);
    element.textContent = text ?? "";
    return element;
}

async function action(name, body) {
    try {
        const response = await fetch("/api/" + name, {
            method: "POST",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify(body)
        });
        const result = await response.json();
        connection.textContent = result.message || (response.ok ? "Saved" : "Action failed");
    } catch {
        connection.textContent = "Connection lost while submitting the action";
    }
}

function runLink(run, repository, label) {
    const element = node(run && repository ? "a" : "span", label);
    if (run && repository) {
        element.href = "https://github.com/" + repository + "/actions/runs/" + run;
    }
    return element;
}

function render() {
    const reservations = state.reservations;
    byId("session").textContent = state.email || "";
    byId("pause").textContent = state.paused ? "Resume" : "Pause";
    byId("reservations").textContent = `Pass ${state.pass} · ${reservations.queued} queued · ${reservations.running} running · ${reservations.publication} publication · ${reservations.slots} slots` + (state.feed_status ? ` · ${state.feed_status}` : "");
    const publications = state.publications.slice().reverse().map(publication => {
        const item = node("li");
        item.append(runLink(publication.run, state.workflow_repository, `${publication.id} / ${publication.attempt}`));
        item.append(" · " + (publication.reconciled ? "reconciled" : publication.state));
        return item;
    });
    byId("publications").replaceChildren(...publications);
    const filter = byId("filter").value.trim().toLowerCase();
    const queues = new Map();
    const rows = [];
    for (const item of [...state.items].sort((a, b) => a.order - b.order)) {
        const status = item.condition || item.state;
        if (!queues.has(item.repo)) {
            queues.set(item.repo, new Map());
        }
        const queue = queues.get(item.repo);
        queue.set(status, (queue.get(status) || 0) + 1);
        if (!`${item.package} ${item.target} ${item.version}`.toLowerCase().includes(filter)) {
            continue;
        }
        const row = node("tr");
        row.append(node("td", item.package), node("td", item.target));
        row.append(node("td", item.version + (item.revision ? "-" + item.revision : "")));
        const disposition = node("td", status + (item.reason ? " · " + item.reason : ""));
        if (item.retryable) {
            const retry = node("button", "Retry");
            retry.addEventListener("click", () => action("retry", {item: item.id}));
            disposition.append(" ", retry);
        }
        row.append(disposition);
        const build = node("td");
        if (item.attempt) {
            build.append(runLink(item.run, state.workflow_repository, `Attempt ${item.attempt}`));
        }
        row.append(build);
        const timings = {...item.timings};
        if (item.github_queue !== undefined) {
            timings.queue = item.github_queue;
        }
        row.append(node("td", Object.entries(timings).map(([phase, duration]) => `${phase} ${duration}s`).join(" · ")));
        row.append(node("td", (item.bindings || []).map(binding => `${binding.package} ${binding.version}-${binding.revision}`).join(", ")));
        const feed = node("td", item.feed_commit?.slice(0, 12));
        feed.title = item.feed_commit || "";
        row.append(feed);
        rows.push(row);
    }
    byId("queues").replaceChildren(...[...queues].map(([repo, queue]) => {
        const items = [...queue].map(([status, count]) => `${count} ${status}`);
        return node("p", repo + " · " + items.join(" · "));
    }));
    byId("matrix").replaceChildren(...rows);
}

function update(event) {
    state = JSON.parse(event.data);
    connection.textContent = state.load_error || (state.ready ? "Live" : "Recovering state");
    render();
}

events.addEventListener("snapshot", update);
events.addEventListener("update", update);
events.onerror = () => { connection.textContent = "Reconnecting"; };
byId("filter").addEventListener("input", () => { if (state) { render(); } });
byId("pause").addEventListener("click", () => { if (state) { action("dispatch-paused", {paused: !state.paused}); } });
byId("reconcile").addEventListener("click", () => action("reconcile", {}));
byId("admit").addEventListener("submit", event => {
    event.preventDefault();
    action("admit-tag", Object.fromEntries(new FormData(event.currentTarget)));
});
