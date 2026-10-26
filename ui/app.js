"use strict";
const element = (id) => document.getElementById(id);
let repositories = [], graph = {executions: [], mutations_allowed: false};
let selected = null, executionId = null, workflowPreview = null, events = null, dirty = false, busy = false, targetIndex = null;

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
        document.querySelectorAll("form button, form input, form textarea, form select, #repositories button, " +
            ".actions button, #enroll, #reconcile, #request-build")
            .forEach((control) => { control.disabled = true; });
        try {
            await action();
        } catch (error) {
            feedback(error.result || error.message, true);
        } finally {
            busy = false;
            document.querySelectorAll("button, form input, form textarea, form select").forEach((control) => { control.disabled = false; });
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
        ["Build duration", duration(execution.build_job)],
        ["Publication verified", timestamp(execution.receipt?.publication_verified_utc)]]) {
        values.append(node("dt", label), node("dd", value));
    }
    facts.append(values,
        node("p", execution.failure || execution.blockers.join("; ") || "No current blockers", execution.failure ? "error" : "secondary"));
    const summary = execution.receipt?.summary, tests = summary?.tests, reported = node("dl");
    const compilers = (summary?.compilers || []).map((compiler) => compiler.name + " " + compiler.version).join("; ");
    reported.append(node("dt", "Compilers"), node("dd", compilers || "Not recorded"));
    reported.append(node("dt", "Tests"), node("dd", tests ?
        tests.passed + " passed, " + tests.failed + " failed, " + tests.skipped + " skipped" : "Not recorded"));
    if (tests?.coverage_percent !== undefined) {
        reported.append(node("dt", "Coverage"), node("dd", tests.coverage_percent + "%"));
    }
    if (typeof tests?.report_url === "string" && tests.report_url.startsWith("https://")) {
        const link = node("a", "Test report ↗");
        Object.assign(link, {href: tests.report_url, target: "_blank", rel: "noopener noreferrer"});
        const value = node("dd");
        value.append(link);
        reported.append(node("dt", "Report"), value);
    }
    facts.append(node("h2", "Hook-reported build summary"), reported);
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
            link.addEventListener("click", (event) => {
                if (event.button === 0 && !event.ctrlKey && !event.metaKey && !event.shiftKey && !event.altKey) {
                    event.preventDefault();
                    window.open(link.href, "_blank", "popup,width=1100,height=800,resizable=yes,scrollbars=yes,noopener,noreferrer");
                }
            });
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
function preview(definition) {
    element("definition").value = JSON.stringify(definition, null, 2);
    dirty = true;
    workflowPreview = null;
}
function dependencyChoices() {
    const definition = JSON.parse(element("definition").value), self = element("repository").value.split("/").pop();
    const enrolled = repositories.map((row) => row.repository.split("/").pop());
    const names = new Set([...definition.depends_on, ...enrolled.filter((name) => name !== self)]);
    element("dependencies").replaceChildren();
    for (const name of names) {
        const label = node("label"), checkbox = node("input");
        Object.assign(checkbox, {type: "checkbox", value: name, checked: definition.depends_on.includes(name), disabled: busy});
        label.append(checkbox, node("span", name + (name === self ? " (invalid: self dependency)" : enrolled.includes(name) ? "" : " (not enrolled)")));
        element("dependencies").append(label);
    }
    if (names.size === 0) {
        element("dependencies").append(node("p", "No other repositories enrolled", "secondary"));
    }
}
function fields() {
    const definition = JSON.parse(element("definition").value);
    element("definition-controls").disabled = false;
    for (const id of ["title", "package"]) {
        element(id).value = definition[id];
    }
    for (const family of ["native", "openwrt"]) {
        element(family + "-hook").value = definition.build[family] || "";
        element(family + "-hook").required = definition.matrix.some((row) => row.family === family);
    }
    dependencyChoices();
    element("targets").replaceChildren();
    definition.matrix.forEach((target, index) => {
        const row = node("tr"), actions = node("td", "", "row-actions");
        for (const value of [target.id, target.distribution + " " + target.suite, target.arch, JSON.stringify(target.runner)]) {
            row.append(node("td", value));
        }
        actions.append(button("Edit", () => editTarget(index)), button("Duplicate", () => editTarget(index, true)), button("Remove", () => {
            definition.matrix.splice(index, 1);
            preview(definition);
            fields();
        }));
        row.append(actions);
        element("targets").append(row);
    });
    element("matrix-message").textContent = definition.matrix.length ? "" : "Add at least one target before saving.";
}
function freshDefinition() {
    preview({schema: 1, title: "", package: "", depends_on: [], build: {}, matrix: []});
    fields();
}
function targetFamily() {
    const native = element("target-family").value === "native", previous = element("target-format").value;
    element("native-fields").disabled = !native;
    element("native-fields").hidden = !native;
    element("openwrt-fields").disabled = native;
    element("openwrt-fields").hidden = native;
    element("target-format").replaceChildren();
    for (const format of native ? ["deb", "rpm"] : ["ipk", "apk"]) {
        const option = node("option", format);
        option.value = format;
        element("target-format").append(option);
    }
    if (Array.from(element("target-format").options).some((option) => option.value === previous)) {
        element("target-format").value = previous;
    }
}
function editTarget(index = null, duplicate = false) {
    const matrix = JSON.parse(element("definition").value).matrix;
    const target = index === null ? {id: "", family: "native", distribution: "", suite: "", arch: "", runner: "", build: {format: "deb"}} : matrix[index];
    targetIndex = duplicate ? null : index;
    element("target-heading").textContent = duplicate ? "Duplicate target" : index === null ? "Add target" : "Edit target";
    for (const key of ["id", "family", "distribution", "suite", "arch"]) {
        element("target-" + key).value = key === "id" && duplicate ? "" : target[key];
    }
    element("target-runner-kind").value = Array.isArray(target.runner) ? "labels" : "name";
    element("target-runner").value = Array.isArray(target.runner) ? target.runner.join("\n") : target.runner;
    for (const key of ["image", "platform", "target"]) {
        element("build-" + key).value = target.build[key] || "";
    }
    targetFamily();
    element("target-format").value = target.build.format;
    element("target-error").textContent = "";
    element("target-editor").showModal();
}
function applyTarget(event) {
    event.preventDefault();
    const definition = JSON.parse(element("definition").value), target = {};
    for (const key of ["id", "family", "distribution", "suite", "arch"]) {
        target[key] = element("target-" + key).value;
    }
    target.runner = element("target-runner-kind").value === "labels" ? element("target-runner").value.split("\n") : element("target-runner").value;
    target.build = {format: element("target-format").value};
    for (const key of target.family === "native" ? ["image", "platform"] : ["target"]) {
        target.build[key] = element("build-" + key).value;
    }
    const duplicate = definition.matrix.some((row, index) => index !== targetIndex && row.id === target.id);
    const runners = Array.isArray(target.runner) ? target.runner : [target.runner];
    if (duplicate || runners.length > 32 || runners.some((label) => !label || label.length > 256) ||
        (!Array.isArray(target.runner) && target.runner.includes("\n"))) {
        element("target-error").textContent = duplicate ? "Target ID already exists; choose a unique ID." :
            "Use one runner name, or up to 32 nonempty labels (one per line, at most 256 characters each).";
    } else {
        if (targetIndex === null) {
            definition.matrix.push(target);
        } else {
            definition.matrix[targetIndex] = target;
        }
        preview(definition);
        fields();
        element("target-editor").close();
    }
}
function selectRepository(repository = null) {
    if (!dirty || window.confirm("Discard the unsaved definition edits?")) {
        selected = repository;
        dirty = false;
        workflowPreview = null;
        element("repository").value = repository?.repository || "";
        element("branch").value = repository?.branch || "";
        if (repository) {
            element("definition").value = JSON.stringify(repository.definition, null, 2);
            fields();
        } else {
            freshDefinition();
            dirty = false;
        }
        element("revision").textContent = repository ? "Committed configuration: " + repository.branch + " · " + repository.revision.slice(0, 12) :
            "No committed revision selected";
        for (const id of ["workflow-current", "workflow-proposed", "tag"]) {
            element(id).value = "";
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
        if (!element("definition-controls").disabled) {
            dependencyChoices();
        }
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
element("editor").addEventListener("input", (event) => {
    const id = event.target.id;
    dirty = true;
    workflowPreview = null;
    if (id === "repository" || id === "branch") {
        selected = null;
        element("revision").textContent = "Read the committed definition before updating this repository/ref";
    }
    if (id !== "repository" && id !== "branch") {
        const definition = JSON.parse(element("definition").value);
        definition.title = element("title").value;
        definition.package = element("package").value;
        definition.depends_on = Array.from(element("dependencies").querySelectorAll("input:checked"), (input) => input.value);
        for (const family of ["native", "openwrt"]) {
            if (element(family + "-hook").value) {
                definition.build[family] = element(family + "-hook").value;
            } else {
                delete definition.build[family];
            }
        }
        preview(definition);
    }
    if (!element("definition-controls").disabled && (id === "repository" || event.target.type === "checkbox")) {
        dependencyChoices();
    }
});
element("new-definition").addEventListener("click", () => {
    if (window.confirm("Replace the definition draft with an empty form? GitHub is unchanged until you commit.")) {
        freshDefinition();
    }
});
element("add-target").addEventListener("click", () => editTarget());
element("target-family").addEventListener("change", targetFamily);
element("target-form").addEventListener("submit", applyTarget);
element("cancel-target").addEventListener("click", () => element("target-editor").close());
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
            element("definition-controls").disabled = true;
            dirty = true;
            element("definition-details").open = true;
            element("revision").textContent = "Read GitHub revision: " + error.result.github_revision +
                " · use Start new definition to replace the unsupported document";
        }
        throw error;
    }
}));
element("editor").addEventListener("submit", (event) => {
    event.preventDefault();
    operation(async () => {
        if (JSON.parse(element("definition").value).matrix.length === 0) {
            throw new Error("Add at least one target before committing the definition.");
        }
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
selectRepository();
connect();
