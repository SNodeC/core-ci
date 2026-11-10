'use strict';
const element = id => document.getElementById(id);
let graph = {repositories: {}, executions: {}, ready: false};
let selected = null, events = null, busy = false, renderPending = false;
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
function state(work) {
    const {active} = work, result = node('span', '', 'execution-state');
    result.append(node('span', active.state === 'building' && work.build?.conclusion === 'cancelled' ? 'cancelled' : active.state, 'state ' + active.state));
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
    if (operation === 'retry') {
        const work = graph.executions[body.execution];
        body = {package: work.source.package, target: work.target.id};
    } else if (operation === 'refresh') {
        body = {};
    }
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
function timestamp(value, message = 'Not recorded') {
    const date = new Date(value || '');
    let result = Object.assign(node('span', message, message === '—' || message === 'Not recorded'
        ? 'secondary' : 'secondary activity'), {title: message});
    if (Number.isFinite(date.getTime())) {
        result = node('time', '', 'timestamp');
        Object.assign(result, {dateTime: date.toISOString(), title: fullDateFormat.format(date)});
        result.setAttribute('aria-label', result.title);
        result.append(node('span', timeFormat.format(date)), node('span', dateFormat.format(date)));
    }
    return result;
}
function phases(work) {
    const {active, build, publish} = work;
    const buildJob = build?.job, publishJob = publish?.job;
    const terminal = ['published', 'failed'].includes(active.state);
    const verification = {started_at: active.verification_started_at, completed_at: active.proof?.verified_at};
    const verifying = publishJob?.conclusion === 'success';
    const uncertaintyStage = active.uncertainty?.stage || (active.publish_assigned ? 'publish' : 'build');
    const rows = [['Build', buildJob, build, null, active.state === 'waiting' || active.state === 'building'],
        ['Publisher job', publishJob, publish, buildJob, active.state === 'publishing' && !verifying],
        ['Publication recorded', verification, publish, publishJob, active.state === 'publishing' && verifying]];
    return rows.map(([label, job, run, previous, current], index) => {
        const waiting = ['requested', 'queued', 'pending', 'waiting'].includes(job?.status);
        const start = waiting ? null : job?.started_at, finish = job?.completed_at;
        const unresolved = active.uncertainty && uncertaintyStage === (index === 0 ? 'build' : 'publish');
        const message = !terminal && current ? unresolved ? active.uncertainty.reason
            : active.state === 'waiting' ? 'Waiting for prerequisites'
            : run?.step || (job?.status && job.status !== 'completed' ? job.status : '')
                || (run?.status && run.status !== 'completed' ? run.status : '')
                || (index === 2 ? 'Awaiting publisher receipt'
                    : run?.send === 'accepted' ? 'Awaiting GitHub evidence' : 'Dispatch unconfirmed') : '—';
        return {label, start, finish, previous, message, cells: [timestamp(start, start ? '—' : message),
            timestamp(finish, start && !finish ? message : '—')]};
    });
}
function link(label, url) {
    const allowed = typeof url === 'string' && url.startsWith('https://github.com/');
    return allowed ? Object.assign(node('a', label + ' ↗'), {href: url, target: '_blank', rel: 'noopener noreferrer'}) : node('span');
}
function attemptDetail(attempt) {
    const u = attempt.uncertainty, retirement = attempt.retirement;
    const stage = u?.stage || (attempt.publish_assigned === undefined ? 'stage not recorded' : attempt.publish_assigned ? 'publish' : 'build');
    return [attempt.failure_kind, attempt.failure, u ? `Unresolved ${stage} ${u.kind || 'evidence'}: ${u.reason}`
        + ` — First recorded for this reason: ${fullDateFormat.format(new Date(u.at))}` : '', retirement
        ? `${retirement.acknowledgement} · ${fullDateFormat.format(new Date(retirement.at))} · Remote outcome: ${retirement.remote_outcome}`
        : ''].filter(Boolean).join(' · ');
}
function renderExecution(work, panel) {
    const {active, build, publish} = work, phaseRows = phases(work);
    const buildJob = build?.job, publishJob = publish?.job;
    const activity = active.uncertainty?.reason || phaseRows.find(phase => phase.message !== '—')?.message || active.state;
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
    for (const {label, start, finish, previous, cells} of phaseRows) {
        const row = body.insertRow(), title = node('th', label);
        title.scope = 'row';
        row.append(title);
        const elapsed = [Date.parse(finish) - Date.parse(start),
            label === 'Build' ? null : Date.parse(start) - Date.parse(previous?.completed_at)];
        for (const value of [...cells, ...elapsed.map((ms, column) =>
            node('span', ms === null ? '' : Number.isFinite(ms) && (column > 0 || ms >= 0)
                ? `${ms < 0 ? '−' : ''}${Math.floor(Math.abs(ms) / 60000)} min ${Math.floor(Math.abs(ms) / 1000) % 60} s`
                : 'Not recorded'))]) {
            const cell = node('td');
            cell.append(value);
            row.append(cell);
        }
    }
    facts.append(values, timings);
    if (active.failure || active.uncertainty || active.retirement) {
        facts.append(node('p', attemptDetail(active), active.failure ? 'error' : 'secondary'));
    }
    const dependencies = node('div'), list = node('ul', '', 'dependencies');
    for (const id of work.prerequisites) {
        const prerequisite = graph.executions[id], item = node('li');
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
        actions.append(link('Packages', work.packages));
    }
    for (const [name, label, enabled] of [
        ['reconcile', 'Reconcile', active.assigned],
        ['retry', 'Retry definite failure', active.state === 'failed' && active.failure_kind === 'definite'],
        ['retire', 'Acknowledge publication retirement', active.state === 'publishing' && !active.publish_run_bound]
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
    const content = panel.appendChild(node('div', '', 'execution-content'));
    content.append(columns, actions);
    for (const prior of work.previous) {
        content.append(node('p', `Attempt ${prior.id} · r${prior.revision} · ${prior.state} · ${attemptDetail(prior)}`, 'secondary'));
    }
}
function renderWork() {
    const query = element('search').value.toLowerCase(), filter = element('state-filter').value, levels = new Map(), groups = new Map(), created = new Map();
    const items = Object.values(graph.executions).sort((a, b) => a.index - b.index);
    for (const work of items) {
        const time = work.created, index = work.index;
        if (!created.has(time)) {
            created.set(time, index);
        }
        groups.set(work.id, work.selection ?? created.get(time));
        levels.set(work.id, 1 + Math.max(-1, ...work.prerequisites.map(id => levels.get(id))));
    }
    for (const work of [...items].reverse()) {
        for (const id of work.prerequisites) {
            groups.set(id, Math.max(groups.get(id), groups.get(work.id)));
        }
    }
    const visible = items.filter(work => {
        const active = work.active, target = work.target;
        const matches = filter === 'all' || (filter === 'active' && active.state !== 'published') || active.state === filter;
        return matches && [work.source.repository, work.source.tag, target.distribution, target.suite, target.arch, target.build.format]
            .join(' ').toLowerCase().includes(query);
    }).reverse().sort((a, b) => groups.get(b.id) - groups.get(a.id) || levels.get(b.id) - levels.get(a.id));
    element('graph').replaceChildren();
    for (const work of visible) {
        const {active, target} = work;
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
        for (const {label: phase, cells} of phases(work)) {
            const cell = node('td', '', phase === 'Publication recorded' ? 'work-verified' : 'work-job'), pair = node('span', '', 'job-pair');
            for (const [index, stamp] of cells.entries()) {
                const label = index === 0 ? 'started' : 'finished';
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
            renderExecution(work, panel);
            details.append(panel);
            element('graph').append(details);
        }
    }
    element('visible-count').textContent = `${visible.length} of ${items.length} executions`;
    element('counts').textContent = ['building', 'publishing', 'waiting', 'failed', 'published'].map(value =>
        `${items.filter(work => work.active.state === value).length} ${value === 'building' ? 'build phase' : value}`).join(' · ');
}
function selectRepository(name = null, entry = graph.repositories[name]) {
    selected = name;
    element('repository').value = name || '';
    element('branch').value = entry?.ref || '';
    element('tag').value = '';
    renderRepositories();
}
function renderRepositories() {
    const entry = graph.repositories[selected];
    element('definition').value = entry ? JSON.stringify(entry.definition, null, 2) : '';
    element('revision').textContent = entry ? `Committed configuration: ${entry.ref} · ${entry.commit.slice(0, 12)}`
        : selected ? 'Awaiting committed definition' : 'No committed revision selected';
    const names = Object.keys(graph.repositories), owners = new Set(names.map(name => name.split('/')[0]));
    element('owner').textContent = owners.size === 1 ? [...owners][0] : 'Local administration';
    element('repositories').replaceChildren();
    for (const name of names) {
        const control = button(graph.repositories[name].title, () => selectRepository(name));
        control.append(node('span', name, 'secondary'));
        control.setAttribute('aria-pressed', String(name === selected));
        control.disabled = busy;
        element('repositories').append(control);
    }
    element('request-build').disabled = busy || !graph.repositories[selected];
}
function mapModel(model) {
    const packages = new Map(model.packages.map(item => [item.name, item]));
    const units = model.units.filter(unit => unit.status !== 'NoRelease');
    const current = unit => unit.inflight || (unit.status === 'Failed' ? unit.failure : null)
        || unit.next || unit.published || {};
    const identity = unit => `${unit.package}~${unit.target}~${current(unit).version || ''}`;
    const byUnit = new Map(units.map(unit => [unit.package + '~' + unit.target, unit]));
    const depth = unit => 1 + Math.max(-1, ...unit.prerequisites.map(name => {
        const parent = byUnit.get(name + '~' + unit.target);
        return parent ? depth(parent) : -1;
    }));
    units.sort((a, b) => a.order - b.order || depth(a) - depth(b));
    const repositories = {}, executions = {};
    for (const item of model.packages) {
        const source = item.sourceRepo, repository = source.owner + '/' + source.repo;
        repositories[repository] = {title: item.title, ref: source.branch, commit: item.commit};
    }
    const states = {Waiting: 'waiting', Blocked: 'waiting', Ready: 'building', Dispatching: 'building',
        Building: 'building', AwaitingPublish: 'publishing', PublishDispatching: 'publishing',
        Publishing: 'publishing', Settled: 'published', Failed: 'failed'};
    const run = value => value ? {...value, identity: {html_url: value.html_url}} : undefined;
    for (const unit of units) {
        const item = packages.get(unit.package), chosen = current(unit), id = identity(unit);
        const tag = unit.tag || (item.released?.version === chosen.version ? item.released.tag : chosen.version);
        const buildId = `${unit.package}~${unit.target}~${chosen.version || ''}~r${chosen.revision}`;
        const time = value => value == null ? undefined : new Date(value * 1000).toISOString();
        executions[id] = {id, index: unit.order, selection: 0,
            source: {package: unit.package, repository: item.sourceRepo.owner + '/' + item.sourceRepo.repo, tag},
            target: {id: unit.target, distribution: unit.distribution, suite: unit.suite, arch: unit.arch, build: {format: unit.format}},
            active: {id: buildId, revision: chosen.revision, state: states[unit.status],
                assigned: unit.assigned, verification_started_at: time(unit.publishDispatchedAt),
                ...(unit.status === 'Failed' ? {failure: unit.failure?.reason, failure_kind: 'definite'} : {}),
                ...(unit.publishedAt == null ? {} : {proof: {verified_at: time(unit.publishedAt)}})},
            previous: [], prerequisites: unit.prerequisites.map(name => byUnit.get(name + '~' + unit.target)).filter(Boolean).map(identity),
            build: run(unit.build), publish: run(unit.publish)};
    }
    return {ready: model.ready, repositories, executions};
}
function connect() {
    if (events) {
        events.close();
    }
    element('connection').textContent = 'Connecting…';
    element('reconnect').hidden = true;
    events = new EventSource('/events');
    const receive = event => {
        const model = JSON.parse(event.data);
        document.querySelector('header strong').title = model.userEmail || '';
        graph = mapModel(model);
        if (!renderPending) {
            renderPending = true;
            requestAnimationFrame(() => {
                renderPending = false;
                element('connection').textContent = graph.ready ? 'Live · GitHub job events' : 'Loading…';
                element('reconnect').hidden = true;
                renderRepositories();
                renderWork();
            });
        }
    };
    events.addEventListener('snapshot', receive);
    events.addEventListener('update', receive);
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
        selected = name;
    });
});
element('request-build').addEventListener('click', () => operation(async () => {
    feedback(await api('request', {repository: element('repository').value.trim(), tag: element('tag').value.trim()}));
}));
selectRepository();
connect();
