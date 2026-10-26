"use strict";
const element = (id) => document.getElementById(id);
let repositories = [], graph = {executions: [], mutations_allowed: false};
let selected = null, executionId = null, workflowPreview = null, events = null, dirty = false, busy = false;

function node(tag, text = "", className = "") {
    const result = document.createElement(tag);
    result.textContent = text;
    result.className = className;
    return result;
}
function button(text, action, className = "") {
    const result = node("button", text, className);
    result.type = "button";
    result.addEventListener("click", action);
    return result;
}
function state(execution) {
    const result = node("span", execution.superseded_by ? "superseded" : execution.state, "state " + execution.state);
    return result;
}
function feedback(value, failed = false) {
    element("feedback").textContent = typeof value === "string" ? value : JSON.stringify(value, null, 2);
    element("feedback").className = failed ? "error" : "";
    const committed = value && typeof value === "object" ? value.github_committed : undefined;
    element("feedback-summary").textContent = committed === true ?
        "GitHub content committed" + (failed ? "; controller/view update failed" : "") : committed === null || value?.uncertain === true ?
        "Operation outcome uncertain — inspect before retrying" : failed ? "Operation failed — technical details" : "Operation details";
    element("feedback-details").open = true;
}
async function api(path, body) {
    let response, result;
    try {
        response = await fetch(path, {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify(body)});
        result = await response.json();
    } catch (failure) {
        const error = new Error("Controller response unavailable; inspect current state before retrying.");
        error.result = {error: error.message, uncertain: true};
        throw error;
    }
    if (!response.ok) {
        const error = new Error(JSON.stringify(result, null, 2));
        error.result = result;
        throw error;
    }
    return result;
}
async function operation(action) {
    if (!busy) {
        busy = true;
        document.querySelectorAll("form button, form input, form textarea, #repositories button, .actions button, #enroll, #reconcile, #request-build")
            .forEach((control) => { control.disabled = true; });
        try {
            await action();
        } catch (error) {
            feedback(error.result || error.message, true);
        } finally {
            busy = false;
            document.querySelectorAll("button, form input, form textarea").forEach((control) => { control.disabled = false; });
            renderWork();
        }
    }
}
function tab(name) {
    for (const value of ["work", "repositories"]) {
        element(value + "-panel").hidden = value !== name;
        element(value + "-tab").setAttribute("aria-pressed", String(value === name));
    }
}
function showExecution(id) {
    executionId = id;
    element("search").value = "";
    element("state-filter").value = "all";
    tab("work");
    renderWork();
}
function timestamp(value) {
    const date = new Date(value || "");
    return Number.isFinite(date.getTime()) ? new Intl.DateTimeFormat(undefined, {
        dateStyle: "short", timeStyle: "long"
    }).format(date) : "Not recorded";
}
function duration(job) {
    const seconds = (Date.parse(job?.completed_at) - Date.parse(job?.started_at)) / 1000;
    return Number.isFinite(seconds) && seconds >= 0 ? Math.floor(seconds / 60) + " min " + Math.floor(seconds % 60) + " s" : "Not recorded";
}
function renderExecution(execution, panel, open) {
    const columns = node("div", "", "detail-grid"), facts = node("div"), values = node("dl");
    for (const [label, value] of [["Activity", execution.activity], ["Selection", execution.automatic ? "Automatic" : "Explicit request"],
        ["Build started", timestamp(execution.build_job?.started_at)], ["Build finished", timestamp(execution.build_job?.completed_at)],
        ["Build duration", duration(execution.build_job)]]) {
        values.append(node("dt", label), node("dd", value));
    }
    facts.append(values,
        node("p", execution.failure || execution.blockers.join("; ") || "No current blockers", execution.failure ? "error" : "secondary"));
    const dependencies = node("div"), list = node("ul", "", "dependencies");
    for (const id of execution.prerequisites) {
        const prerequisite = graph.executions.find((row) => row.id === id);
        const item = node("li");
        item.append(button(prerequisite ? prerequisite.repository + " " + prerequisite.tag : id,
            () => showExecution(id), "row-select"));
        if (prerequisite) {
            item.append(state(prerequisite));
        }
        list.append(item);
    }
    dependencies.append(node("h2", "Prerequisite executions — current state"), list);
    if (list.children.length === 0) {
        dependencies.append(node("p", "No prerequisites", "secondary"));
    }
    columns.append(facts, dependencies);
    const actions = node("div", "", "actions");
    for (const [label, run] of [["Build logs", execution.build_run], ["Publication logs", execution.publication_run]]) {
        if (run && typeof run.html_url === "string" && run.html_url.startsWith("https://github.com/")) {
            const link = node("a", label + " ↗");
            Object.assign(link, {href: run.html_url, target: "_blank", rel: "noopener noreferrer"});
            actions.append(link);
        }
    }
    if (execution.state === "failed" && !execution.superseded_by && graph.mutations_allowed) {
        const retry = button("Retry with a new rebuild revision", () => operation(async () => {
            feedback(await api("/api/work/retry", {execution: execution.id}));
        }), "primary");
        retry.disabled = busy;
        actions.append(retry);
    }
    const technical = node("details");
    technical.open = open;
    technical.append(node("summary", "Source, configuration and execution identities"), node("pre", JSON.stringify(execution, null, 2)));
    panel.append(columns, actions, technical);
}
function renderWork() {
    const query = element("search").value.toLowerCase(), filter = element("state-filter").value;
    const technicalOpen = document.querySelector(".execution-details details")?.open || false;
    const order = {publishing: 0, building: 1, waiting: 2, failed: 3, published: 3};
    const visible = graph.executions.filter((row) => {
        const target = row.target_row;
        const matches = filter === "all" || (filter === "active" && row.state !== "published" && !row.superseded_by) || row.state === filter;
        return matches && [row.repository, row.tag, target.distribution, target.suite, target.arch, target.build.format]
            .join(" ").toLowerCase().includes(query);
    }).sort((a, b) => (a.superseded_by ? 5 : order[a.state]) - (b.superseded_by ? 5 : order[b.state]) || b.revision - a.revision);
    if (!visible.some((row) => row.id === executionId)) {
        executionId = null;
    }
    element("graph").replaceChildren();
    for (const execution of visible) {
        const target = execution.target_row, row = node("tr"), repository = node("td"), distribution = node("td");
        row.setAttribute("aria-selected", String(execution.id === executionId));
        const toggle = button(execution.id === executionId ? "▾" : "▸", () => {
            executionId = executionId === execution.id ? null : execution.id;
            renderWork();
        }, "row-select chevron");
        toggle.setAttribute("aria-expanded", String(execution.id === executionId));
        toggle.setAttribute("aria-label", "Build details for " + execution.label);
        toggle.setAttribute("aria-controls", "execution-" + execution.id);
        const control = node("td");
        control.append(toggle);
        repository.append(node("span", execution.repository), node("span", execution.tag, "secondary"));
        distribution.append(node("span", target.distribution), node("span", target.suite, "secondary"));
        const status = node("td");
        status.append(state(execution));
        row.append(control, repository, distribution, node("td", target.arch), node("td", target.build.format),
            node("td", "r" + execution.revision, "revision"), status, node("td", timestamp(execution.build_job?.started_at), "build-time"));
        element("graph").append(row);
        if (execution.id === executionId) {
            const details = node("tr"), panel = node("td", "", "execution-details");
            panel.colSpan = 8;
            panel.id = "execution-" + execution.id;
            renderExecution(execution, panel, technicalOpen);
            details.append(panel);
            element("graph").append(details);
        }
    }
    element("visible-count").textContent = visible.length + " of " + graph.executions.length + " executions";
    element("counts").textContent = ["building", "publishing", "waiting", "failed"].map((value) =>
        graph.executions.filter((row) => row.state === value && !row.superseded_by).length + " " + value).join(" · ");
}
function fields() {
    const definition = JSON.parse(element("definition").value);
    element("title").value = definition.title;
    element("package").value = definition.package;
    element("dependencies").value = definition.depends_on.join(", ");
    element("targets").replaceChildren();
    for (const target of definition.matrix) {
        const row = node("tr");
        for (const value of [target.id, target.distribution + " " + target.suite, target.arch, JSON.stringify(target.runner)]) {
            row.append(node("td", value));
        }
        element("targets").append(row);
    }
}
function selectRepository(repository = null) {
    if (!dirty || window.confirm("Discard the unsaved definition edits?")) {
        selected = repository;
        dirty = false;
        workflowPreview = null;
        element("repository").value = repository?.repository || "";
        element("branch").value = repository?.branch || "";
        element("definition").value = repository ? JSON.stringify(repository.definition, null, 2) : "";
        element("revision").textContent = repository ? "Committed configuration: " + repository.branch + " · " + repository.revision.slice(0, 12) :
            "No committed revision selected";
        for (const id of ["title", "package", "dependencies", "workflow-current", "workflow-proposed", "tag"]) {
            element(id).value = "";
        }
        element("targets").replaceChildren();
        if (repository) {
            fields();
        }
        renderRepositories();
    }
}
function renderRepositories() {
    const owners = new Set(repositories.map((row) => row.repository.split("/")[0]));
    element("owner").textContent = owners.size === 1 ? Array.from(owners)[0] : "Local administration";
    element("repositories").replaceChildren();
    for (const repository of repositories) {
        const control = button(repository.definition.title, () => selectRepository(repository));
        control.append(node("span", repository.repository, "secondary"));
        control.setAttribute("aria-pressed", String(repository.repository === selected?.repository));
        control.disabled = busy;
        element("repositories").append(control);
    }
    const remote = repositories.find((row) => row.repository === selected?.repository);
    element("remote-revision").textContent = remote && remote.revision !== selected.revision ?
        "A newer committed definition is available. Your draft and its original revision are preserved." : "";
}
function connect() {
    if (events) {
        events.close();
    }
    element("connection").textContent = "Connecting…";
    element("reconnect").hidden = true;
    events = new EventSource("/api/events");
    events.addEventListener("graph", (event) => {
        graph = JSON.parse(event.data);
        element("connection").textContent = "Live";
        element("reconnect").hidden = true;
        renderWork();
    });
    events.addEventListener("repositories", (event) => {
        repositories = JSON.parse(event.data).repositories;
        renderRepositories();
    });
    events.addEventListener("unavailable", (event) => { feedback(JSON.parse(event.data), true); });
    events.onerror = () => {
        element("connection").textContent = events.readyState === EventSource.CLOSED ? "Disconnected" : "Reconnecting…";
        element("reconnect").hidden = false;
    };
}
for (const name of ["work", "repositories"]) {
    element(name + "-tab").addEventListener("click", () => tab(name));
}
element("reconnect").addEventListener("click", connect);
element("search").addEventListener("input", renderWork);
element("state-filter").addEventListener("change", renderWork);
element("enroll").addEventListener("click", () => selectRepository());
element("reconcile").addEventListener("click", () => operation(async () => { feedback(await api("/api/work/reconcile", {})); }));
for (const id of ["repository", "branch", "definition", "title", "package", "dependencies"]) {
    element(id).addEventListener("input", () => {
        dirty = true;
        workflowPreview = null;
        if (id === "repository" || id === "branch") {
            selected = null;
            element("revision").textContent = "Read the committed definition before updating this repository/ref";
        }
    });
}
for (const id of ["title", "package", "dependencies"]) {
    element(id).addEventListener("change", () => {
        try {
            const definition = JSON.parse(element("definition").value);
            definition.title = element("title").value;
            definition.package = element("package").value;
            definition.depends_on = element("dependencies").value.split(",").map((value) => value.trim()).filter(Boolean);
            element("definition").value = JSON.stringify(definition, null, 2);
            fields();
        } catch (error) {
            feedback("Repair the definition JSON before editing its fields: " + error.message, true);
        }
    });
}
element("definition").addEventListener("change", () => {
    try { fields(); } catch (error) { feedback(error.message, true); }
});
async function adoptCommitted(result) {
    feedback(result);
    try {
        const response = await fetch("/api/repositories");
        if (!response.ok) {
            throw new Error("Could not read the committed controller view");
        }
        repositories = (await response.json()).repositories;
        dirty = false;
        selectRepository(repositories.find((row) => row.repository === result.repository));
    } catch (error) {
        feedback({...result, error: error.message}, true);
    }
}
element("import").addEventListener("click", () => operation(async () => {
    try {
        const result = await api("/api/repositories/refresh", {repository: element("repository").value, branch: element("branch").value});
        await adoptCommitted(result);
    } catch (error) {
        if (error.result && typeof error.result.content === "string" && error.result.blob_sha) {
            const effective = repositories.find((row) => row.repository === element("repository").value);
            selected = {revision: effective?.revision || "", blob_sha: error.result.blob_sha};
            element("definition").value = error.result.content;
            dirty = true;
            element("definition-details").open = true;
            element("revision").textContent = "Read GitHub revision: " + error.result.github_revision + " · repair invalid JSON before saving";
        }
        throw error;
    }
}));
element("editor").addEventListener("submit", (event) => {
    event.preventDefault();
    operation(async () => {
        const result = await api("/api/repositories/save", {
            repository: element("repository").value, branch: element("branch").value,
            revision: selected?.revision || "", blob_sha: selected?.blob_sha || "", definition: JSON.parse(element("definition").value)
        });
        await adoptCommitted(result);
    });
});
element("request-build").addEventListener("click", () => operation(async () => {
    feedback(await api("/api/work/request", {repository: element("repository").value, tag: element("tag").value}));
}));
element("workflow-preview").addEventListener("click", () => operation(async () => {
    workflowPreview = await api("/api/repositories/workflow", {repository: element("repository").value});
    element("workflow-current").value = workflowPreview.current;
    element("workflow-proposed").value = workflowPreview.proposed;
    feedback("Workflow preview at " + workflowPreview.head + " · proposed blob " + workflowPreview.proposed_blob_sha);
}));
element("workflow-write").addEventListener("click", () => operation(async () => {
    if (!workflowPreview) {
        throw new Error("Preview the enrolled repository workflow before committing.");
    }
    const result = await api("/api/repositories/workflow", {
        repository: workflowPreview.repository, head: workflowPreview.head, blob_sha: workflowPreview.blob_sha, write: true
    });
    workflowPreview = null;
    feedback(result);
}));
connect();
