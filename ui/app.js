'use strict';
const element = id => document.getElementById(id);
let graph = {repositories: {}, executions: []};
let selected = null, events = null, busy = false;
const expanded = new Set();

function node(tag, text = '', className = '') {
    return Object.assign(document.createElement(tag), {textContent: text, className});
}
function button(text, action, className = '') {
    const result = node('button', text, className);
    result.type = 'button';
    result.addEventListener('click', action);
    return result;
}
function view(work) {
    const active = work.attempts.at(-1), build = active.facts.build_run, publish = active.facts.publish_run;
    const buildJob = build?.jobs?.find(job => job.id === build.job_id);
    const publishJob = publish?.jobs?.find(job => job.id === publish.job_id);
    const progress = active.state === 'building' ? build : publish;
    const running = (progress?.jobs || []).flatMap(job => job.steps || []).find(step => step.status === 'in_progress');
    const send = active.facts[active.state === 'building' ? 'build_send' : 'publish_send'];
    const activity = active.state === 'publishing' && publishJob?.conclusion === 'success' ? 'Awaiting publisher receipt'
        : running ? `Last reported: ${running.name}` : [progress?.status, progress?.conclusion].filter(Boolean).join(' · ')
            || (active.state === 'waiting' ? 'Exact prerequisites pending'
                : send?.outcome === 'accepted' ? 'Awaiting GitHub evidence' : 'Dispatch unconfirmed');
    return {active, build, publish, buildJob, publishJob, activity};
}
function state(work) {
    const {active, activity} = view(work), result = node('span', '', 'execution-state');
    result.append(node('span', active.state, 'state ' + active.state));
    if (!['published', 'failed'].includes(active.state)) {
        result.append(Object.assign(node('span', activity, 'secondary activity'), {title: activity}));
    }
    return result;
}
function feedback(value, failed = false) {
    element('feedback').textContent = typeof value === 'string' ? value : JSON.stringify(value, null, 2);
    element('feedback').className = failed ? 'error' : '';
    element('feedback-summary').textContent = failed ? 'Operation failed — inspect details before repeating' : 'Operation recorded';
    element('feedback-details').open = true;
}
async function api(operation, body) {
    let result;
    try {
        const response = await fetch(`/api/${operation}`, {
            method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body)
        });
        result = await response.json();
        if (!response.ok || result.error) {
            throw new Error(result.error || `HTTP ${response.status}`);
        }
    } catch (error) {
        throw new Error(`${error.message}. Inspect current graph before repeating the request.`);
    }
    return result;
}
async function operation(action) {
    if (!busy) {
        busy = true;
        document.querySelectorAll('button, form input, form textarea, form select').forEach(control => { control.disabled = true; });
        try {
            await action();
        } catch (error) {
            feedback(error.message, true);
        } finally {
            busy = false;
            document.querySelectorAll('button, form input, form textarea, form select').forEach(control => { control.disabled = false; });
            renderRepositories();
            renderWork();
        }
    }
}
function tab(name) {
    for (const value of ['work', 'repositories']) {
        element(value + '-panel').hidden = value !== name;
        element(value + '-tab').setAttribute('aria-pressed', String(value === name));
    }
}
function showExecution(id) {
    expanded.add(id);
    element('search').value = '';
    element('state-filter').value = 'all';
    tab('work');
    renderWork();
    element('work-' + id)?.scrollIntoView({block: 'center', inline: 'nearest'});
}
const dateFormat = new Intl.DateTimeFormat(undefined, {dateStyle: 'short'});
const timeFormat = new Intl.DateTimeFormat(undefined, {timeStyle: 'medium', hourCycle: 'h23'});
const fullDateFormat = new Intl.DateTimeFormat(undefined, {dateStyle: 'full', timeStyle: 'medium', hourCycle: 'h23'});
function timestamp(value, pending = false) {
    const date = new Date(value || '');
    let result = node('span', pending ? 'Pending' : 'Not recorded', 'secondary');
    if (Number.isFinite(date.getTime())) {
        result = node('time', '', 'timestamp');
        Object.assign(result, {dateTime: date.toISOString(), title: fullDateFormat.format(date)});
        result.setAttribute('aria-label', result.title);
        result.append(node('span', timeFormat.format(date)), node('span', dateFormat.format(date)));
    }
    return result;
}
function link(label, url) {
    const allowed = typeof url === 'string' && url.startsWith('https://github.com/');
    return allowed ? Object.assign(node('a', label + ' ↗'), {href: url, target: '_blank', rel: 'noopener noreferrer'}) : node('span');
}
function renderExecution(work, panel, open) {
    const {active, build, publish, buildJob, publishJob, activity} = view(work);
    const columns = node('div', '', 'detail-grid'), facts = node('div'), values = node('dl'), timings = node('table', '', 'timings');
    values.append(node('dt', 'Activity'), node('dd', ['published', 'failed'].includes(active.state) ? active.state : activity));
    values.append(node('dt', 'Attempt'), node('dd', active.id));
    const heading = timings.createTHead().insertRow();
    for (const label of ['Job', 'Started', 'Finished', 'Duration', 'Delay']) {
        const cell = node('th', label);
        cell.scope = 'col';
        heading.append(cell);
    }
    const body = timings.createTBody();
    for (const [label, job, previous] of [['Build', buildJob], ['Publisher job', publishJob, buildJob],
        ['Publication recorded', {started_at: active.verification_started_at, completed_at: active.proof?.verified_at}, publishJob]]) {
        const row = body.insertRow(), title = node('th', label);
        title.scope = 'row';
        row.append(title);
        const start = job?.status === 'queued' ? null : job?.started_at;
        const elapsed = [Date.parse(job?.completed_at) - Date.parse(start),
            label === 'Build' ? null : Date.parse(start) - Date.parse(previous?.completed_at)];
        for (const value of [timestamp(start), timestamp(job?.completed_at), ...elapsed.map((ms, column) =>
            node('span', ms === null ? '' : Number.isFinite(ms) && (column > 0 || ms >= 0)
                ? `${ms < 0 ? '−' : ''}${Math.floor(Math.abs(ms) / 60000)} min ${Math.floor(Math.abs(ms) / 1000) % 60} s`
                : 'Not recorded'))]) {
            const cell = node('td');
            cell.append(value);
            row.append(cell);
        }
    }
    facts.append(values, timings);
    if (active.failure || active.uncertainty) {
        facts.append(node('p', active.failure || `Unresolved: ${active.uncertainty.reason}`, active.failure ? 'error' : 'secondary'));
    }
    const dependencies = node('div'), list = node('ul', '', 'dependencies');
    for (const id of work.prerequisites) {
        const prerequisite = graph.executions.find(candidate => candidate.id === id), item = node('li');
        const control = node('a', prerequisite ? `${prerequisite.source.package} ${prerequisite.source.tag} · ${prerequisite.target.id}` : id);
        Object.assign(control, {href: '#work-' + id, onclick: () => showExecution(id)});
        control.dataset.go = id;
        item.append(control);
        if (prerequisite) {
            item.append(state(prerequisite));
        }
        list.append(item);
    }
    dependencies.append(node('h2', 'Exact prerequisite executions'), list);
    if (!work.prerequisites.length) {
        dependencies.append(node('p', 'Root work — no prerequisites', 'secondary'));
    }
    columns.append(facts, dependencies);
    const actions = node('div', '', 'actions');
    actions.append(link('GitHub build run', build?.identity?.html_url), link('Build logs', buildJob?.html_url),
        link('GitHub publication run', publish?.identity?.html_url), link('Publisher logs', publishJob?.html_url));
    if (active.state === 'published') {
        const directory = active.receipt.packages[0].path.split('/').slice(0, -1).map(encodeURIComponent).join('/');
        actions.append(link('Packages', `https://github.com/${active.receipt.feed.repository}/tree/${active.proof.feed_revision}/${directory}`));
    }
    for (const [name, label, enabled] of [
        ['reconcile', 'Reconcile', Boolean(active.assignment)],
        ['retry', 'Retry definite failure', active.state === 'failed' && active.failure_kind === 'definite'],
        ['retire', 'Acknowledge publication retirement', active.state === 'publishing' && !active.facts.publish_run_id]
    ]) {
        if (enabled) {
            const control = button(label, () => operation(async () => {
                const body = {execution: work.id};
                if (name === 'retire') {
                    body.acknowledgement = prompt('Acknowledge the unknown remote publication outcome. '
                        + 'This does not cancel remote work, permit retry or dispatch a replacement. Record your acknowledgement:');
                }
                if (name !== 'retire' || body.acknowledgement) {
                    feedback(await api(name, body));
                }
            }));
            control.dataset.operation = name;
            control.dataset.id = work.id;
            control.disabled = busy;
            actions.append(control);
        }
    }
    const technical = node('details');
    technical.open = open;
    technical.append(node('summary', 'Frozen inputs, attempts and execution evidence'), node('pre', JSON.stringify(work, null, 2)));
    const content = panel.appendChild(node('div', '', 'execution-content'));
    content.append(columns, actions, technical);
}
function renderWork() {
    const query = element('search').value.toLowerCase(), filter = element('state-filter').value, levels = new Map(), groups = new Map(), created = new Map();
    const technicalOpen = new Set([...document.querySelectorAll('.execution-details details[open]')].map(detail => detail.closest('td').id));
    for (const [index, work] of graph.executions.entries()) {
        const time = work.attempts[0].created;
        if (!created.has(time)) {
            created.set(time, index);
        }
        groups.set(work.id, work.selection ?? created.get(time));
        levels.set(work.id, 1 + Math.max(-1, ...work.prerequisites.map(id => levels.get(id))));
    }
    for (const work of [...graph.executions].reverse()) {
        for (const id of work.prerequisites) {
            groups.set(id, Math.max(groups.get(id), groups.get(work.id)));
        }
    }
    const visible = graph.executions.filter(work => {
        const active = work.attempts.at(-1), target = work.target;
        const matches = filter === 'all' || (filter === 'active' && active.state !== 'published') || active.state === filter;
        return matches && [work.source.repository, work.source.tag, target.distribution, target.suite, target.arch, target.build.format]
            .join(' ').toLowerCase().includes(query);
    }).reverse().sort((a, b) => groups.get(b.id) - groups.get(a.id) || levels.get(b.id) - levels.get(a.id));
    element('graph').replaceChildren();
    for (const work of visible) {
        const {active, buildJob, publishJob} = view(work), target = work.target;
        const row = node('tr', '', 'execution-row'), repository = node('th'), distribution = node('td');
        row.id = 'work-' + work.id;
        repository.scope = 'row';
        row.setAttribute('aria-selected', String(expanded.has(work.id)));
        const toggle = button(expanded.has(work.id) ? '▾' : '▸', () => {
            if (!expanded.delete(work.id)) {
                expanded.add(work.id);
            }
            renderWork();
        }, 'chevron');
        toggle.dataset.expand = work.id;
        toggle.setAttribute('aria-expanded', String(expanded.has(work.id)));
        toggle.setAttribute('aria-label', 'Build details for ' + work.source.package);
        toggle.setAttribute('aria-controls', 'execution-' + work.id);
        const control = node('td'), identity = node('span', '', 'package-identity');
        control.append(toggle);
        identity.append(Object.assign(node('strong', work.source.package), {title: work.source.repository}),
            node('span', work.source.tag, 'package-version'));
        repository.append(identity, node('span', work.source.repository.split('/')[0] + ' · r' + active.revision, 'secondary revision'));
        distribution.append(node('strong', target.distribution + ' ' + target.suite),
            node('span', target.arch + ' · ' + target.build.format, 'secondary'));
        const status = node('td');
        status.append(state(work));
        row.append(control, repository, distribution, status);
        for (const [phase, job] of [['Build', buildJob], ['Publisher job', publishJob],
            ['Publication recorded', {started_at: active.verification_started_at, completed_at: active.proof?.verified_at}]]) {
            const cell = node('td', '', phase === 'Publication recorded' ? 'work-verified' : 'work-job'), pair = node('span', '', 'job-pair');
            for (const [label, value] of [['started', job?.status === 'queued' ? null : job?.started_at], ['finished', job?.completed_at]]) {
                const stamp = timestamp(value, !['published', 'failed'].includes(active.state));
                stamp.setAttribute('aria-label', phase + ' ' + label + ': ' + (stamp.getAttribute('aria-label') || stamp.textContent));
                pair.append(stamp);
            }
            cell.append(pair);
            row.append(cell);
        }
        element('graph').append(row);
        if (expanded.has(work.id)) {
            const details = node('tr'), panel = node('td', '', 'execution-details');
            panel.colSpan = 7;
            panel.id = 'execution-' + work.id;
            renderExecution(work, panel, technicalOpen.has(panel.id));
            details.append(panel);
            element('graph').append(details);
        }
    }
    element('visible-count').textContent = `${visible.length} of ${graph.executions.length} executions`;
    element('counts').textContent = ['building', 'publishing', 'waiting', 'failed', 'published'].map(value =>
        `${graph.executions.filter(work => work.attempts.at(-1).state === value).length} ${value}`).join(' · ');
}
function selectRepository(name = null, entry = graph.repositories[name]) {
    selected = name;
    element('repository').value = name || '';
    element('branch').value = entry?.ref || '';
    element('definition').value = entry ? JSON.stringify(entry.definition, null, 2) : '';
    element('revision').textContent = entry ? `Committed configuration: ${entry.ref} · ${entry.commit.slice(0, 12)}`
        : 'No committed revision selected';
    element('tag').value = '';
    renderRepositories();
}
function renderRepositories() {
    const names = Object.keys(graph.repositories), owners = new Set(names.map(name => name.split('/')[0]));
    element('owner').textContent = owners.size === 1 ? [...owners][0] : 'Local administration';
    element('repositories').replaceChildren();
    for (const name of names) {
        const control = button(graph.repositories[name].definition.title, () => selectRepository(name));
        control.append(node('span', name, 'secondary'));
        control.setAttribute('aria-pressed', String(name === selected));
        control.disabled = busy;
        element('repositories').append(control);
    }
    element('request-build').disabled = busy || !graph.repositories[selected];
}
function connect() {
    if (events) {
        events.close();
    }
    element('connection').textContent = 'Connecting…';
    element('reconnect').hidden = true;
    events = new EventSource('/events');
    events.onmessage = event => {
        const update = JSON.parse(event.data);
        if (Array.isArray(update)) {
            for (const {op, path, value} of update) {
                const keys = path.slice(1).split('/').map(key => key.replace(/~1/g, '/').replace(/~0/g, '~'));
                const key = keys.pop(), parent = keys.reduce((object, name) => object[name], graph);
                if (Array.isArray(parent) && op === 'add') {
                    parent.splice(key === '-' ? parent.length : Number(key), 0, value);
                } else if (op === 'remove') {
                    if (Array.isArray(parent)) {
                        parent.splice(Number(key), 1);
                    } else {
                        delete parent[key];
                    }
                } else {
                    parent[key] = value;
                }
            }
        } else {
            graph = update;
        }
        element('connection').textContent = 'Live · GitHub job events';
        element('reconnect').hidden = true;
        renderRepositories();
        renderWork();
    };
    events.onerror = () => {
        element('connection').textContent = 'Reconnecting…';
        element('reconnect').hidden = false;
    };
}
for (const name of ['work', 'repositories']) {
    element(name + '-tab').addEventListener('click', () => tab(name));
}
element('reconnect').addEventListener('click', connect);
element('search').addEventListener('input', renderWork);
element('state-filter').addEventListener('change', renderWork);
element('enroll').addEventListener('click', () => selectRepository());
element('editor').addEventListener('submit', event => {
    event.preventDefault();
    operation(async () => {
        const name = element('repository').value.trim(), ref = element('branch').value.trim();
        feedback(await api(graph.repositories[name] ? 'refresh' : 'enroll', {repository: name, ref}));
        const response = await fetch('/api/graph');
        if (!response.ok) {
            throw new Error('Could not read committed definition');
        }
        selectRepository(name, (await response.json()).repositories[name]);
    });
});
element('request-build').addEventListener('click', () => operation(async () => {
    feedback(await api('request', {repository: element('repository').value.trim(), tag: element('tag').value.trim()}));
}));
selectRepository();
connect();
