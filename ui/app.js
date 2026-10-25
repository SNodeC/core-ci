"use strict";
const element = (id) => document.getElementById(id);
let selected = null;
let repositories = [];

function feedback(value, failed = false) {
    element("feedback").textContent = typeof value === "string" ? value : JSON.stringify(value, null, 2);
    element("feedback").className = failed ? "error" : "";
}

async function api(path, body) {
    const options = body === undefined ? {} : {
        method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify(body)
    };
    const response = await fetch(path, options);
    const result = await response.json();
    if (!response.ok) {
        throw new Error(JSON.stringify(result, null, 2));
    }
    return result;
}

function select(repository) {
    selected = repository;
    element("repository").value = repository.repository;
    element("branch").value = repository.branch;
    element("revision").textContent = "Committed revision: " + repository.revision;
    element("definition").value = JSON.stringify(repository.definition, null, 2);
}

async function reload() {
    const result = await api("/api/repositories");
    repositories = result.repositories;
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
        dependencies.textContent = "Depends on: " + (repository.definition.depends_on.join(" → ") || "none");
        const revision = document.createElement("pre");
        revision.textContent = "Definition: " + repository.revision;
        const targets = document.createElement("ul");
        for (const row of repository.definition.matrix) {
            const target = document.createElement("li");
            target.textContent = row.id + " · " + row.distribution + " / " + row.suite + " / " + row.arch;
            targets.append(target);
        }
        const edit = document.createElement("button");
        edit.textContent = "Edit";
        edit.addEventListener("click", () => select(repository));
        card.append(title, name, dependencies, revision, targets, edit);
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

element("reload").addEventListener("click", () => operation(reload));
element("repository").addEventListener("input", () => {
    selected = null;
    element("revision").textContent = "No committed revision selected";
});
element("import").addEventListener("click", () => operation(async () => {
    const result = await api("/api/repositories/refresh", {
        repository: element("repository").value, branch: element("branch").value
    });
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
            definition: JSON.parse(element("definition").value)
        });
        await reload();
        select(repositories.find((row) => row.repository === result.repository));
        feedback(result);
    });
});
operation(reload);
