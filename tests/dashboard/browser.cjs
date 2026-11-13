'use strict';
const fs = require('fs'), path = require('path'), http = require('http');
const {chromium} = require(process.env.CORECI_PLAYWRIGHT_PATH || 'playwright-core');
const root = path.resolve(__dirname, '../..'), data = path.join(root, 'tests/data/dashboard');
const output = process.argv[2], mode = process.argv[3];
fs.mkdirSync(output, {recursive: true});
let fixture;
const server = http.createServer((req, res) => {
    const target = new URL(req.url, 'http://localhost').pathname;
    if (target === '/events') {
        res.writeHead(200, {'Content-Type': 'text/event-stream', 'Cache-Control': 'no-store'});
        res.write('event: snapshot\n' + 'data: ' + JSON.stringify(fixture) + '\n\n');
    } else if (target.startsWith('/api/')) {
        res.writeHead(400, {'Content-Type': 'application/json'});
        res.end(JSON.stringify({error: 'Not supported by core-ci: fixture operation'}));
    } else if (target === '/favicon.ico') {
        res.writeHead(204); res.end();
    } else {
        const name = target === '/' ? 'index.html' : target.slice(1);
        const base = path.join(root, 'app/dashboard');
        if (['index.html', 'style.css', 'app.js'].includes(name)) {
            res.writeHead(200, {'Content-Type': {'index.html': 'text/html', 'style.css': 'text/css', 'app.js': 'application/javascript'}[name]});
            res.end(fs.readFileSync(path.join(base, name)));
        } else {
            res.writeHead(404); res.end();
        }
    }
});
async function run() {
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const url = 'http://127.0.0.1:' + server.address().port;
    const browser = await chromium.launch({executablePath: process.env.CORECI_CHROMIUM || '/opt/google/chrome/chrome',
        headless: true, args: ['--no-sandbox', '--disable-background-networking']});
    const consoleLog = [], records = [];
    try {
        if (browser.version() !== '156.0.8078.79') {
            throw new Error('Reference browser version mismatch: ' + browser.version());
        }
        const statuses = ['NoRelease', 'Waiting', 'Blocked', 'Ready', 'Dispatching', 'Building', 'AwaitingPublish', 'PublishDispatching', 'Publishing', 'Settled', 'Failed'];
        const samples = [JSON.parse(fs.readFileSync(path.join(data, 'models/work-all-1280x800-light.json')))];
        const packages = statuses.map((status, index) => ({name: 'p' + index, title: status, commit: 'a'.repeat(40), sourceRepo: {owner: 'Example', repo: 'r' + index, branch: 'main'}, released: {tag: 'v1.0.0', version: '1.0.0'}}));
        const units = statuses.map((status, index) => ({package: 'p' + index, target: 't', feed: 'f', status, detail: null, position: null,
            order: index + 1, assigned: ['Building','AwaitingPublish','PublishDispatching','Publishing','Settled','Failed'].includes(status),
            publishDispatchedAt: ['Publishing', 'Settled'].includes(status) ? 1 : null, publishedAt: status === 'Settled' ? 121 : null,
            distribution: 'test', suite: 'stable', arch: 'test', format: 'deb', prerequisites: [],
            inflight: ['Dispatching','Building','AwaitingPublish','PublishDispatching','Publishing'].includes(status)
                ? {version: '1.0.0', revision: 1, buildId: `p${index}~t~1.0.0~r1`, phase: status, runId: 1, attempt: 1} : null,
            next: {version: '1.0.0', revision: 1}, published: status === 'Settled' ? {version: '1.0.0', revision: 1, at: 1, runId: 1, feedCommit: 'a'.repeat(40)} : null,
            failure: status === 'Failed' ? {version: '1.0.0', revision: 1, reason: 'test failure', at: 1, runId: 1} : null,
            build: {runId: 1, html_url: 'https://github.com/Example/Recipes/actions/runs/1', status: index % 2 ? 'in_progress' : 'completed',
                job: {started_at: '2026-01-01T00:00:00Z', completed_at: index % 2 ? null : '2026-01-01T00:01:00Z', status: index % 2 ? 'in_progress' : 'completed', conclusion: index % 2 ? null : 'success'}}, publish: {}}));
        samples.push({schema: 1, generation: 1, ready: true, packages, units}, {schema: 1, generation: 0, ready: false, packages: [], units: []}, {schema: 1, generation: 1, ready: true, packages: [], units: []});
        const ordering = structuredClone(samples[1]);
        ordering.packages = ordering.packages.slice(0, 3);
        ordering.units = ordering.units.slice(0, 3).map((unit, index) => ({...unit,
            status: index === 1 ? 'Waiting' : 'Building', order: [100, 0, 200][index],
            prerequisites: index === 1 ? ['p0'] : [],
            inflight: index === 1 ? null : {...units[4].inflight, buildId: `p${index}~t~1.0.0~r1`}}));
        samples.push(ordering);
        for (const [index, sample] of samples.entries()) {
            fixture = sample;
            const context = await browser.newContext({viewport: {width: 1280, height: 900}, locale: 'en-US', timezoneId: 'UTC'});
            const page = await context.newPage();
            page.on('pageerror', error => consoleLog.push({sample: index, error: error.message}));
            page.on('console', message => {if (message.type() === 'error') {consoleLog.push({sample: index, error: message.text()});}});
            await page.goto(url, {waitUntil: 'domcontentloaded'});
            await page.waitForFunction(() => element('connection').textContent !== 'Connecting…');
            if (index === 1) {
                for (const status of ['Publishing', 'Settled']) {
                    const unit = sample.units.find(unit => unit.status === status);
                    const cell = page.locator(`[id="work-${unit.package}~t~1.0.0"] .work-verified`);
                    if (await cell.locator('time').count() !== (status === 'Settled' ? 1 : 0)) {
                        throw new Error(status + ': publication recording requires a committed receipt');
                    }
                    if (status === 'Settled' && await cell.locator('time').getAttribute('datetime') !== '1970-01-01T00:02:01.000Z') {
                        throw new Error('Publication recording must use the committed event time');
                    }
                    if (!await cell.locator('[aria-label="Publication recorded started: —"]').count()) {
                        throw new Error('Publisher dispatch must not invent a publication-recording start');
                    }
                }
            }
            if (index === 4) {
                const names = await page.locator('#graph > .execution-row .package-identity strong').allTextContents();
                if (JSON.stringify(names) !== JSON.stringify(['p1', 'p2', 'p0'])) {
                    throw new Error('Expected dependency level desc, then allocation order desc: ' + JSON.stringify(names));
                }
            }
            const toggles = page.locator('[data-expand]');
            for (const toggle of await toggles.all()) {await toggle.click();}
            await page.screenshot({path: path.join(output, 'g6-' + index + '.png'), fullPage: true});
            await context.close();
        }
        records.push('all status kinds', 'running/completed jobs', 'loading/empty models', 'expanded details');
        fs.writeFileSync(path.join(output, mode + '-console.json'), JSON.stringify(consoleLog, null, 2));
        fs.writeFileSync(path.join(output, mode + '-browser.json'), JSON.stringify({browser: browser.version(), records}, null, 2));
        if (consoleLog.length) {throw new Error('Browser console errors: ' + JSON.stringify(consoleLog));}
    } finally {
        await browser.close();
        server.closeAllConnections();
        await new Promise(resolve => server.close(resolve));
    }
}
run().catch(error => {console.error(error.message); process.exitCode = 1;});
