'use strict';
const fs = require('fs'), path = require('path'), http = require('http');
const {chromium} = require(process.env.CORECI_PLAYWRIGHT_PATH || 'playwright-core');
const root = path.resolve(__dirname, '../..'), data = path.join(root, 'tests/data/dashboard');
const output = process.argv[2], mode = process.argv[3];
fs.mkdirSync(output, {recursive: true});
let fixture, baseline = false;
const server = http.createServer((req, res) => {
    const target = new URL(req.url, 'http://localhost').pathname;
    if (target === '/events') {
        res.writeHead(200, {'Content-Type': 'text/event-stream', 'Cache-Control': 'no-store'});
        res.write((baseline ? '' : 'event: snapshot\n') + 'data: ' + JSON.stringify(fixture) + '\n\n');
    } else if (target.startsWith('/api/')) {
        res.writeHead(400, {'Content-Type': 'application/json'});
        res.end(JSON.stringify({error: 'Not supported by core-ci: fixture operation'}));
    } else if (target === '/favicon.ico') {
        res.writeHead(204); res.end();
    } else {
        const name = target === '/' ? 'index.html' : target.slice(1);
        const base = baseline ? path.join(data, 'reference/assets') : path.join(root, 'app/dashboard');
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
        if (mode === 'references') {
            const references = JSON.parse(fs.readFileSync(path.join(data, 'reference/screenshots.json')));
            const masks = {};
            for (const variant of ['baseline', 'new']) {
                const original = variant !== 'new';
                let context = null, page = null, group = '';
                for (const record of references) {
                    const key = JSON.stringify([record.viewport, record.scheme]);
                    if (key !== group) {
                        if (context) {await context.close();}
                        group = key;
                        const captured = JSON.parse(fs.readFileSync(path.join(data, 'reference/fixtures', record.fixtureSha256 + '.json')));
                        baseline = original;
                        fixture = original ? captured.graph : JSON.parse(fs.readFileSync(path.join(data, 'models', record.file.replace('.png', '.json'))));
                        context = await browser.newContext({viewport: record.viewport, colorScheme: record.scheme,
                            locale: record.locale, timezoneId: record.timezone, deviceScaleFactor: record.deviceScaleFactor});
                        page = await context.newPage();
                        page.on('pageerror', error => consoleLog.push({group, variant, error: error.message}));
                        page.on('console', message => {if (message.type() === 'error') {consoleLog.push({group, variant, error: message.text()});}});
                        await page.goto(url, {waitUntil: 'domcontentloaded'});
                        await page.waitForFunction(() => graph.ready);
                        await page.evaluate(() => document.fonts.ready);
                    }
                    if (record.view.startsWith('work-') && !['work-all', 'work-details'].includes(record.view)) {
                        await page.locator('#state-filter').selectOption(record.view.slice(5));
                    } else if (record.view === 'work-details') {
                        await page.locator('#state-filter').selectOption('all');
                        await page.locator('[data-expand]').first().click();
                    } else if (record.view === 'repositories') {
                        await page.locator('#repositories-tab').click();
                    } else if (record.view === 'repository-definition') {
                        await page.locator('#repositories button').first().click();
                    }
                    await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
                    const values = page.locator('.execution-content dl dd').nth(1);
                    const masked = record.view === 'work-details' ? [values] : [];
                    masks[record.file] ||= {};
                    masks[record.file][variant] = masked.length ? await values.boundingBox() : null;
                    masks[record.file][variant + 'Ink'] = masked.length ? await values.evaluate(element => {
                        const box = element.getBoundingClientRect(), range = document.createRange();
                        const canvas = document.createElement('canvas').getContext('2d');
                        canvas.font = getComputedStyle(element).font;
                        let right = box.right;
                        const text = element.firstChild;
                        for (let index = 0; text && index < text.length; ++index) {
                            range.setStart(text, index); range.setEnd(text, index + 1);
                            const glyph = range.getBoundingClientRect();
                            right = Math.max(right, glyph.left + canvas.measureText(text.data[index]).actualBoundingBoxRight);
                        }
                        return {x: box.x, y: box.y, width: right - box.x, height: box.height};
                    }) : null;
                    await page.screenshot({path: path.join(output, (original ? 'baseline-' : '') + record.file), fullPage: true,
                        animations: 'disabled'});
                    if (!original) {records.push(record.file);}
                }
                if (context) {await context.close();}
            }
            fs.writeFileSync(path.join(output, 'G5-masks.json'), JSON.stringify(masks, null, 2));
        } else {
            const statuses = ['NoRelease', 'Waiting', 'Blocked', 'Ready', 'Dispatching', 'Building', 'AwaitingPublish', 'PublishDispatching', 'Publishing', 'Settled', 'Failed'];
            const samples = [JSON.parse(fs.readFileSync(path.join(data, 'models/work-all-1280x800-light.json')))];
            const packages = statuses.map((status, index) => ({name: 'p' + index, title: status, commit: 'a'.repeat(40), sourceRepo: {owner: 'Example', repo: 'r' + index, branch: 'main'}, released: {tag: 'v1.0.0', version: '1.0.0'}}));
            const units = statuses.map((status, index) => ({package: 'p' + index, target: 't', feed: 'f', status, detail: null, position: null,
                order: index + 1, assigned: ['Building','AwaitingPublish','PublishDispatching','Publishing','Settled','Failed'].includes(status),
                publishDispatchedAt: status === 'Settled' ? 1 : null, publishedAt: status === 'Settled' ? 1 : null,
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
        }
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
