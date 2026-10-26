"use strict";
const element = (id) => document.getElementById(id);
let selected = null;
let workflowPreview = null;
let repositories = [];

function feedback(value, failed = false) {
    element("feedback").textContent = typeof value === "string" ? value : JSON.stringify(value, null, 2);
    element("feedback").className = failed ? "error" : "";
    const message = typeof value === "string" ? value : failed ? "Operation failed — technical details" : "Operation completed — technical details";
    element("feedback-summary").textContent = message.replace(/[a-f0-9]{40,64}/g, (hash) => hash.slice(0, 12));
    element("feedback-details").open = failed;
}

async function api(path, body) {
    const options = body === undefined ? {} : {
        method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify(body)
    };
    const response = await fetch(path, options);
    const result = await response.json();
    if (!response.ok) {
        const error = new Error(JSON.stringify(result, null, 2));
        error.result = result;
        throw error;
    }
    return result;
}

function select(repository) {
    selected = repository;
    workflowPreview = null;
    element("repository").value = repository.repository;
    element("branch").value = repository.branch;
    element("revision").textContent = "Committed configuration: " + repository.branch + " · " + repository.revision.slice(0, 12);
    element("revision").title = repository.revision;
    element("definition").value = JSON.stringify(repository.definition, null, 2);
}

async function reload() {
    const [result, graph] = await Promise.all([api("/api/repositories"), api("/api/graph")]);
    repositories = result.repositories;
    element("graph").replaceChildren();
    if (graph.executions.length === 0) {
        element("graph").textContent = "No source executions selected yet.";
    }
    const names = new Map(graph.executions.map((execution) => [execution.id, execution.label]));
    const order = {publishing: 0, building: 1, waiting: 2, failed: 3, published: 4};
    graph.executions.sort((left, right) =>
        (left.superseded_by ? 5 : order[left.state]) - (right.superseded_by ? 5 : order[right.state]) || right.revision - left.revision);
    for (const execution of graph.executions) {
        const card = document.createElement("article");
        card.id = "execution-" + execution.id;
        const title = document.createElement("h3");
        title.textContent = execution.label;
        const state = document.createElement("p");
        state.textContent = execution.activity + " · " + execution.tag + " · rebuild " + execution.revision;
        const source = document.createElement("pre");
        const technical = document.createElement("details");
        const summary = document.createElement("summary");
        summary.textContent = "Technical details";
        source.textContent = "Execution: " + execution.id + "\nSource: " + execution.source_commit + "\nDefinition: " + execution.definition_revision;
        const reasons = document.createElement("p");
        reasons.textContent = execution.failure || execution.blockers.join("; ");
        technical.append(summary, source);
        card.append(title, state, reasons, technical);
        for (const prerequisite of execution.prerequisites) {
            const link = document.createElement("a");
            link.href = "#execution-" + prerequisite;
            link.textContent = "Prerequisite: " + names.get(prerequisite) + " ";
            card.append(link);
        }
        for (const run of [execution.build_run, execution.publication_run]) {
            if (run && typeof run.html_url === "string" && run.html_url.startsWith("https://github.com/")) {
                const link = document.createElement("a");
                link.href = run.html_url;
                link.textContent = " GitHub execution log ";
                link.rel = "noopener noreferrer";
                card.append(link);
            }
        }
        if (execution.state === "failed" && !execution.superseded_by) {
            const retry = document.createElement("button");
            retry.textContent = "Retry with a new rebuild revision";
            retry.addEventListener("click", () => operation(async () => {
                const result = await api("/api/work/retry", {execution: execution.id});
                await reload();
                feedback(result);
            }));
            card.append(retry);
        }
        element("graph").append(card);
    }

    element("repositories").replaceChildren();
    if (repositories.length === 0) {
        element("repositories").textContent = "No repositories enrolled.";
    }
    for (const repository of repositories) {
        const card = document.createElement("article");
        const title = document.createElement("h3");
        title.textContent = repository.definition.title;
        const name = document.createElement("p");
        name.textContent = repository.repository;
        const dependencies = document.createElement("p");
        dependencies.textContent = "Depends on: " + (repository.definition.depends_on.join(", ") || "none");
        const revision = document.createElement("details");
        const revisionTitle = document.createElement("summary");
        revisionTitle.textContent = "Definition revision";
        const revisionHash = document.createElement("code");
        revisionHash.textContent = repository.revision;
        revision.append(revisionTitle, revisionHash);
        const targets = document.createElement("ul");
        for (const row of repository.definition.matrix) {
            const target = document.createElement("li");
            target.textContent = row.id + " · " + row.distribution + " / " + row.suite + " / " + row.arch;
            targets.append(target);
        }
        const edit = document.createElement("button");
        edit.textContent = "Edit";
        edit.addEventListener("click", () => select(repository));
        const tag = document.createElement("input");
        tag.placeholder = "Exact version tag; empty selects highest";
        tag.setAttribute("aria-label", "Version tag for " + repository.repository);
        const build = document.createElement("button");
        build.textContent = "Request build";
        build.addEventListener("click", () => operation(async () => {
            const result = await api("/api/work/request", {repository: repository.repository, tag: tag.value});
            await reload();
            feedback(result);
        }));
        card.append(title, name, dependencies, revision, targets, edit, tag, build);
        element("repositories").append(card);
    }
}

async function operation(action) {
    const buttons = document.querySelectorAll("button");
    buttons.forEach((button) => { button.disabled = true; });
    try {
        await action();
    } catch (error) {
        feedback(error.message, true);
    } finally {
        buttons.forEach((button) => { button.disabled = false; });
    }
}

element("reconcile").addEventListener("click", () => operation(async () => {
    const result = await api("/api/work/reconcile", {});
    await reload();
    feedback(result);
}));
element("reload").addEventListener("click", () => operation(reload));
element("repository").addEventListener("input", () => {
    selected = null;
    element("revision").textContent = "No committed revision selected";
});
element("import").addEventListener("click", () => operation(async () => {
    let result;
    try {
        result = await api("/api/repositories/refresh", {
            repository: element("repository").value, branch: element("branch").value
        });
    } catch (error) {
        const body = error.result;
        if (body && typeof body.content === "string" && body.blob_sha) {
            const effective = repositories.find((row) => row.repository === element("repository").value);
            selected = {revision: effective ? effective.revision : "", blob_sha: body.blob_sha};
            element("definition").value = body.content;
            element("revision").textContent = "Read GitHub revision: " + body.github_revision + " · repair the invalid definition before saving";
        }
        throw error;
    }
    await reload();
    const repository = repositories.find((row) => row.repository === result.repository);
    if (repository) {
        select(repository);
    }
    feedback(result);
}));
element("editor").addEventListener("submit", (event) => {
    event.preventDefault();
    operation(async () => {
        const result = await api("/api/repositories/save", {
            repository: element("repository").value,
            branch: element("branch").value,
            revision: selected ? selected.revision : "",
            blob_sha: selected ? selected.blob_sha : "",
            definition: JSON.parse(element("definition").value)
        });
        await reload();
        select(repositories.find((row) => row.repository === result.repository));
        feedback(result);
    });
});
operation(reload);

element("branch").addEventListener("input", () => { workflowPreview = null; });
element("repository").addEventListener("input", () => { workflowPreview = null; });
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
