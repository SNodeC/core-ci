'use strict';
const element = id => document.getElementById(id);
let graph = {repositories: {}, executions: {}, ready: false};
let events = null, busy = false, renderPending = false;
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
    result.append(node('span', active.state, 'state ' + active.state));
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
    const rows = [['Build', work.build, null], ['Publish', work.publish, work.build],
        ['Publication recorded', work.recorded, work.build]];
    return rows.map(([label, phase, previous]) => {
        const start = phase?.start, finish = phase?.end;
        const message = phase?.state || 'pending';
        return {label, start, finish, previous, message, cells: [timestamp(start, start ? '—' : message),
            timestamp(finish, start && !finish ? message : '—')]};
    });
}
function link(label, url) {
    const allowed = typeof url === 'string' && url.startsWith('https://github.com/');
    return allowed ? Object.assign(node('a', label + ' ↗'), {href: url, target: '_blank', rel: 'noopener noreferrer'}) : node('span');
}
function renderExecution(work, panel) {
    const {active, build, publish} = work, phaseRows = phases(work);
    const activity = phaseRows.find(phase => phase.message !== '—')?.message || active.state;
    const columns = node('div', '', 'detail-grid'), facts = node('div'), values = node('dl'), timings = node('table', '', 'timings');
    values.append(node('dt', 'Activity'), node('dd', ['published', 'failed'].includes(active.state) ? active.state : activity));
    values.append(node('dt', 'Attempt'), node('dd', active.id));
    for (const [label, value] of [['Source commit', work.source.commit], ['CI commit', work.source.ciCommit]]) {
        values.append(node('dt', label), node('dd', value || 'Not recorded'));
    }
    values.append(node('dt', 'Dependency versions'), node('dd', work.dependencies.map(item =>
        `${item.package} ${item.version}-${item.revision}`).join(', ') || 'None'));
    if (work.feedCommit) {
        values.append(node('dt', 'Feed commit'), node('dd', work.feedCommit));
    }
    const heading = timings.createTHead().insertRow();
    for (const label of ['Phase', 'State', 'Started', 'Finished', 'Duration', 'Delay']) {
        const cell = node('th', label);
        cell.scope = 'col';
        heading.append(cell);
    }
    const body = timings.createTBody();
    for (const {label, start, finish, previous, cells, message} of phaseRows) {
        const row = body.insertRow(), title = node('th', label);
        title.scope = 'row';
        row.append(title);
        const elapsed = [Date.parse(finish) - Date.parse(start),
            label === 'Build' ? null : Date.parse(start) - Date.parse(previous?.end)];
        for (const value of [node('span', message), ...cells, ...elapsed.map((ms, column) =>
            node('span', ms === null ? '' : Number.isFinite(ms) && (column > 0 || ms >= 0)
                ? `${ms < 0 ? '−' : ''}${Math.floor(Math.abs(ms) / 60000)} min ${Math.floor(Math.abs(ms) / 1000) % 60} s`
                : 'Not recorded'))]) {
            const cell = node('td');
            cell.append(value);
            row.append(cell);
        }
    }
    facts.append(values, timings);
    if (active.failure) {
        facts.append(node('p', active.failure, 'error'));
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
    actions.append(link('GitHub build run', build?.html_url), link('GitHub publication run', publish?.html_url));
    for (const [name, label, enabled] of [
        ['reconcile', 'Reconcile', true],
        ['rebuild', 'Rebuild', active.state === 'published'],
        ['retry', 'Retry failure', active.state === 'failed']
    ]) {
        if (enabled) {
            const control = button(label, () => operation(async () => {
                const body = {package: work.source.package, target: work.target.id};
                feedback(await api(name, body));
            }));
            control.dataset.operation = name;
            control.dataset.id = work.id;
            control.disabled = busy;
            actions.append(control);
        }
    }
    const content = panel.appendChild(node('div', '', 'execution-content'));
    content.append(columns, actions);

}
function renderWork() {
    const query = element('search').value.toLowerCase(), filter = element('state-filter').value;
    const items = Object.values(graph.executions).sort((a, b) => a.index - b.index);
    const visible = items.filter(work => {
        const active = work.active, target = work.target;
        const matches = filter === 'all' || (filter === 'active' && active.state !== 'published') || active.state === filter;
        return matches && [work.source.repository, work.source.tag, target.distribution, target.suite, target.arch, target.build.format]
            .join(' ').toLowerCase().includes(query);
    }).reverse().sort((a, b) => b.level - a.level || b.order - a.order);
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
            const cell = node('td', '', phase === 'Publication recorded' ? 'work-recorded' : 'work-phase'), pair = node('span', '', 'phase-pair');
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
    const panel = element('wave-panel'), wave = graph.wave;
    panel.replaceChildren();
    if (wave) {
        panel.append(node('h2', 'Wave ' + wave.id + ' · ' + wave.phase + (graph.dispatchPaused ? ' · Dispatch paused' : '')), link('GitHub wave run', wave.html_url),
            node('p', 'Overall totals: ' + Object.entries(graph.totals).map(([state, count]) => count + ' ' + state).join(' · ')),
            node('p', 'Latest wave/retry: ' + Object.entries(wave.counts).map(([state, count]) => count + ' ' + state).join(' · ')));
        const timing = node('p');
        timing.append(node('span', 'Started: '), timestamp(wave.started == null ? null : new Date(wave.started * 1000).toISOString()),
            node('span', ' · Finished: '), timestamp(wave.finished == null ? null : new Date(wave.finished * 1000).toISOString()));
        panel.append(timing);
    }
    const publicationPanel = element('publication-panel'), publication = graph.publication;
    publicationPanel.replaceChildren();
    if (publication) {
        publicationPanel.append(node('h2', 'Publication ' + publication.publication + ' · ' + publication.phase),
            link('GitHub publication run', publication.html_url));
        const items = node('ul');
        for (const item of publication.items) {
            items.append(node('li', item.target + ' · ' + item.wave + ' · ' + item.units.map(unit =>
                unit.package + ' ' + unit.version + '-' + unit.revision).join(', ')));
        }
        publicationPanel.append(items);
    }
    element('dispatch-paused').textContent = graph.dispatchPaused ? 'Resume dispatch' : 'Pause dispatch';
    element('dispatch-paused').setAttribute('aria-pressed', String(graph.dispatchPaused));
    element('visible-count').textContent = `${visible.length} of ${items.length} executions`;
    element('counts').textContent = ['planned', 'building', 'built', 'publishing', 'blocked', 'failed', 'published'].map(value =>
        `${items.filter(work => work.active.state === value).length} ${value === 'building' ? 'build phase' : value}`).join(' · ');
}
function renderRepositories() {
    const names = Object.keys(graph.repositories), owners = new Set(names.map(name => name.split('/')[0]));
    element('owner').textContent = owners.size === 1 ? [...owners][0] : 'Local administration';
    element('repositories').replaceChildren();
    for (const name of names) {
        const entry = graph.repositories[name], item = node('p', entry.title);
        item.append(node('span', `${name} · ${entry.ref} · ${entry.commit}`, 'secondary'));
        element('repositories').append(item);
    }
}
function mapModel(model) {
    const packages = new Map(model.packages.map(item => [item.name, item]));
    const units = model.units.filter(unit => packages.get(unit.package)?.released);
    const current = unit => unit.selection || unit.published || {};
    const identity = unit => `${unit.package}~${unit.target}~${current(unit).version || ''}`;
    const byUnit = new Map(units.map(unit => [unit.package + '~' + unit.target, unit]));
    const depth = unit => 1 + Math.max(-1, ...unit.prerequisites.map(name => {
        const parent = byUnit.get(name + '~' + unit.target);
        return parent ? depth(parent) : -1;
    }));
    units.sort((a, b) => depth(a) - depth(b) || a.order - b.order);
    const repositories = {}, executions = {};
    for (const item of model.packages) {
        const source = item.sourceRepo, repository = source.owner + '/' + source.repo;
        repositories[repository] = {title: item.title, ref: source.branch, commit: item.commit};
    }
    const time = value => value == null ? undefined : new Date(value * 1000).toISOString();
    const run = value => value ? {...value, start: time(value.start), end: time(value.end)} : undefined;
    for (const [index, unit] of units.entries()) {
        const item = packages.get(unit.package), chosen = current(unit), id = identity(unit);
        const tag = unit.tag || (item.released?.version === chosen.version ? item.released.tag : chosen.version);
        const buildId = unit.buildId;
        executions[id] = {id, index, level: depth(unit), order: unit.order, feedCommit: unit.published?.feedCommit,
            dependencies: chosen.dependencies || [],
            source: {commit: chosen.commit, ciCommit: chosen.ci_commit, package: unit.package, repository: item.sourceRepo.owner + '/' + item.sourceRepo.repo, tag},
            target: {id: unit.target, distribution: unit.distribution, suite: unit.suite, arch: unit.arch, build: {format: unit.format}},
            active: {id: buildId, revision: chosen.revision, state: unit.status.toLowerCase(),
                assigned: unit.assigned,
                ...(['Failed', 'Blocked'].includes(unit.status) ? {failure: unit.failure?.reason} : {})},
            recorded: {end: time(unit.publishedAt), state: unit.published ? 'succeeded' : 'pending'},
            prerequisites: unit.prerequisites.map(name => byUnit.get(name + '~' + unit.target)).filter(Boolean).map(identity),
            build: run(unit.build), publish: run(unit.publish)};
    }
    return {ready: model.ready, repositories, executions, wave: model.wave, publication: model.publication,
        dispatchPaused: model.dispatch_paused, totals: model.totals};
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
        if (model.loadError) {
            feedback(model.loadError, true);
        }
        if (!renderPending) {
            renderPending = true;
            requestAnimationFrame(() => {
                renderPending = false;
                element('connection').textContent = graph.ready ? 'Live · Workflow events' : 'Loading…';
                element('reconnect').hidden = true;
                renderRepositories();
                renderWork();
            });
        }
    };
    events.addEventListener('snapshot', receive);
    events.addEventListener('update', receive);
    events.onerror = () => {
        element('connection').textContent = events.readyState === EventSource.CLOSED ? 'Disconnected · reconnect required' : 'Reconnecting…';
        element('reconnect').hidden = false;
    };
}
for (const name of ['work', 'repositories']) {
    element(name + '-tab').addEventListener('click', () => tab(name));
}
element('reconnect').addEventListener('click', () => events?.readyState === EventSource.CLOSED ? location.reload() : connect());
element('search').addEventListener('input', renderWork);
element('state-filter').addEventListener('change', renderWork);
element('reload').addEventListener('click', () => operation(async () => {
    feedback(await api('reload', {}));
}));
element('dispatch-paused').addEventListener('click', () => operation(async () => {
    feedback(await api('dispatch-paused', {paused: !graph.dispatchPaused}));
}));
connect();
