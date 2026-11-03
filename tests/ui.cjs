'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {execFileSync} = require('node:child_process');
const http = require('node:http');
const {chromium} = require(`${process.env.CI_BROWSER_MODULES}/playwright`);

async function readProjection(port) {
  const response = await fetch(`http://127.0.0.1:${port}/events`);
  assert.equal(response.status, 200);
  const reader = response.body.getReader(), decoder = new TextDecoder();
  let buffer = '', snapshot;
  try {
    while (!snapshot?.ready) {
      const chunk = await reader.read();
      assert.ok(!chunk.done, 'Initial snapshot must complete');
      buffer += decoder.decode(chunk.value, {stream: true});
      while (buffer.includes('\n\n')) {
        const end = buffer.indexOf('\n\n'), message = buffer.slice(0, end).trim();
        buffer = buffer.slice(end + 2);
        if (message.startsWith('data: ')) {
          const value = JSON.parse(message.slice(6));
          if (!Array.isArray(value)) {
            snapshot = value;
          } else {
            for (const change of value) {
              if (change.path === '/ready') { snapshot.ready = change.value; }
              else { snapshot.executions[change.value.id] = change.value; }
            }
          }
        }
      }
    }
  } finally {
    await reader.cancel();
  }
  return snapshot;
}

function seedGraph(value) {
  execFileSync('python3', [require('node:path').join(__dirname, 'row_fixture.py'),
    process.env.CI_ROWS_BUILD, process.env.CI_ROWS_ROOT, process.argv[3], 'runtime'],
    {input: JSON.stringify(value)});
}

async function main() {
  const graph = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
  const bundle = graph.executions.at(-2);
  const independent = graph.executions.at(-1);
  const foundation = graph.executions[0];
  const browser = await chromium.launch({args: ['--no-sandbox']});
  try {
    const context = await browser.newContext({viewport: {width: 1440, height: 900}, locale: 'en-US', timezoneId: 'Europe/Vienna'});
    const page = await context.newPage();
    const errors = [];
    const messages = [];
    const cdp = await context.newCDPSession(page);
    await cdp.send('Network.enable');
    cdp.on('Network.eventSourceMessageReceived', event => messages.push(JSON.parse(event.data)));
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(`http://127.0.0.1:${process.argv[4]}/`);
    await page.locator('#connection').filter({hasText: 'Live'}).waitFor();
    assert.equal(await page.locator('tr[id^="work-"]').count(), 5);
    assert.equal(await page.locator('#repositories > button').count(), 5);
    const state = page.locator(`#work-${bundle.id} > td`).nth(2);
    const width = (await state.boundingBox()).width;
    assert.ok(Math.abs(width - 128) < 2, `Stable compact state width: ${width}`);
    assert.equal(await page.locator('.work-table > thead .work-verified .job-pair').innerText(), 'Started\nFinished');
    assert.equal(await page.locator(`#work-${foundation.id} > .work-verified .job-pair > *`).count(), 2);
    assert.deepEqual(await page.locator('.execution-row').first().evaluate(row =>
      [...row.cells].slice(4).map(cell => cell.getBoundingClientRect().width)), [184, 184, 184]);
    const rowHeight = (await page.locator(`#work-${bundle.id}`).boundingBox()).height;
    const rowOrder = await page.locator('.execution-row').evaluateAll(rows => rows.map(row => row.id));
    assert.deepEqual(rowOrder, [bundle, graph.executions[2], graph.executions[1], independent, foundation].map(work => `work-${work.id}`));
    assert.equal(await page.locator(`#work-${foundation.id} > .work-verified time`).first().getAttribute('datetime'),
      new Date(foundation.attempts.at(-1).verification_started_at).toISOString());
    const expectedDate = await page.evaluate(value => {
      const date = new Date(value);
      return [new Intl.DateTimeFormat(undefined, {timeStyle: 'medium', hourCycle: 'h23'}).format(date),
        new Intl.DateTimeFormat(undefined, {dateStyle: 'short'}).format(date)];
    }, '2026-10-31T09:00:00Z');
    assert.deepEqual(await page.locator(`#work-${foundation.id} .timestamp`).first().locator('span').allTextContents(), expectedDate);
    assert.ok(await page.locator('.execution-row time > span:first-child').evaluateAll(elements =>
      elements.every(element => element.scrollWidth <= element.clientWidth + 1)));
    assert.equal(await page.locator('.chevron').first().evaluate(element => getComputedStyle(element).textDecorationLine), 'none');
    const headings = await page.locator('.work-table > thead strong').evaluateAll(elements =>
      elements.map(element => ({align: getComputedStyle(element).textAlign, weight: getComputedStyle(element).fontWeight})));
    assert.ok(headings.every(heading => heading.align === 'center' && heading.weight === headings[0].weight));
    await page.locator(`[data-expand="${bundle.id}"]`).click();
    await page.locator(`[data-expand="${independent.id}"]`).click();
    assert.equal(await page.locator('.chevron[aria-expanded="true"]').count(), 2);
    assert.equal(await page.locator(`[data-go="${foundation.id}"]`).first().evaluate(element => element.tagName), 'A');
    assert.equal(await page.locator(`[data-go="${foundation.id}"]`).first().getAttribute('href'), `#work-${foundation.id}`);
    await page.locator(`[data-go="${foundation.id}"]`).first().click();
    assert.equal(await page.locator('.chevron[aria-expanded="true"]').count(), 3);
    await page.waitForFunction(id => {
      const row = document.getElementById(`work-${id}`).getBoundingClientRect();
      return row.top >= 0 && row.bottom <= window.innerHeight;
    }, foundation.id);
    const timings = page.locator(`#execution-${foundation.id} .timings`);
    assert.deepEqual(await timings.locator('thead th').allTextContents(), ['Job', 'Started', 'Finished', 'Duration', 'Delay']);
    assert.deepEqual(await timings.locator('tbody tr').evaluateAll(rows => rows.map(row => row.cells[4].textContent)),
      ['', '−5 min 0 s', '1 min 0 s']);
    assert.deepEqual(await timings.locator('tbody tr').evaluateAll(rows => rows.map(row => row.cells[3].textContent)),
      ['5 min 0 s', '5 min 0 s', '54 min 0 s']);
    assert.deepEqual(await page.locator(`#execution-${bundle.id} .timings tbody tr`).evaluateAll(rows =>
      rows.map(row => row.cells[4].textContent)), ['', 'Not recorded', 'Not recorded']);
    const packageLinks = page.locator(`#execution-${foundation.id} .actions a`).filter({hasText: /^Packages ↗$/});
    const receipt = foundation.attempts.at(-1).receipt;
    const directory = receipt.packages[0].path.split('/').slice(0, -1).map(encodeURIComponent).join('/');
    assert.deepEqual(await packageLinks.evaluateAll(links => links.map(link => link.href)),
      [`https://github.com/${receipt.feed.repository}/tree/${foundation.attempts.at(-1).proof.feed_revision}/${directory}`]);
    assert.deepEqual(await packageLinks.allTextContents(), ['Packages ↗']);
    assert.equal(await page.locator(`#execution-${bundle.id} .actions a`).filter({hasText: /^Packages ↗$/}).count(), 0);
    assert.ok(await page.locator(`#execution-${foundation.id} .actions`).evaluate(actions => {
      const button = actions.querySelector('button').getBoundingClientRect();
      return [...actions.querySelectorAll('a')].every(link => {
        const box = link.getBoundingClientRect();
        return box.top >= button.bottom || box.bottom <= button.top ||
          Math.abs(box.top + box.height / 2 - button.top - button.height / 2) < 1;
      });
    }));
    assert.equal(await page.locator(`#work-${bundle.id} .activity`).textContent(), 'Compile fixture');
    await page.locator('#search').fill('No matching source');
    assert.equal(await page.locator('tr[id^="work-"]').count(), 0);
    await page.locator('#search').fill('');
    await page.locator('#repositories-tab').click();
    await page.locator('#repositories > button').first().click();
    assert.equal(await page.locator('#definition').getAttribute('readonly'), '');
    assert.equal(await page.locator('#branch').inputValue(), 'reviewed');
    await page.locator('#work-tab').click();
    bundle.attempts.at(-1).facts.build_run.jobs[0].steps[0].name = 'Package fixture with a deliberately long GitHub description '.repeat(4);
    receipt.packages.push({...receipt.packages[0], file: 'second.deb', path: receipt.packages[0].path.replace(/[^/]+$/, 'second.deb')});
    seedGraph(graph);
    await page.reload();
    await page.locator('#connection').filter({hasText: 'Live'}).waitFor();
    for (const work of [bundle, independent, foundation]) {
      await page.locator(`[data-expand="${work.id}"]`).click();
    }
    const beforeRetirement = messages.length;
    page.once('dialog', dialog => dialog.accept('Fixture operator acknowledges unknown publication; no replacement.'));
    await page.locator(`[data-operation="retire"][data-id="${independent.id}"]`).click();
    await page.waitForFunction(id => document.querySelector(`#work-${id} .activity`)?.textContent.includes('Package fixture'), bundle.id);
    assert.deepEqual(await packageLinks.allTextContents(), ['Packages ↗']);
    assert.equal(await page.locator(`#work-${bundle.id} .state`).textContent(), 'building');
    assert.equal(await page.locator(`#work-${independent.id} .state`).textContent(), 'failed');
    assert.ok((await page.locator(`#execution-${independent.id}`).textContent()).includes('Fixture operator acknowledges unknown'));
    const retirementUpdates = messages.slice(beforeRetirement);
    assert.equal(retirementUpdates.length, 1);
    assert.ok(Array.isArray(retirementUpdates[0]));
    assert.ok(JSON.stringify(retirementUpdates[0]).length < 1500);
    assert.ok(retirementUpdates[0].every(change => change.path.startsWith(`/executions/${independent.id}/active/`)));
    assert.equal(await page.locator('.chevron[aria-expanded="true"]').count(), 3);
    assert.equal((await state.boundingBox()).width, width);
    assert.equal((await page.locator(`#work-${bundle.id}`).boundingBox()).height, rowHeight);
    assert.deepEqual(await page.locator('.execution-row').evaluateAll(rows => rows.map(row => row.id)), rowOrder);
    const activity = page.locator(`#work-${bundle.id} .activity`);
    assert.equal(await activity.getAttribute('title'), await activity.textContent());
    assert.equal(await activity.textContent(), bundle.attempts.at(-1).facts.build_run.jobs[0].steps[0].name);
    await activity.hover();
    assert.equal(await activity.evaluate(element => getComputedStyle(element).cursor), 'pointer');
    assert.ok(await activity.evaluate(element => element.scrollWidth > element.clientWidth &&
      getComputedStyle(element).textOverflow === 'ellipsis' && getComputedStyle(element).whiteSpace === 'nowrap'));
    await page.locator('#feedback-summary').filter({hasText: 'Operation recorded'}).waitFor();
    await page.screenshot({path: process.env.CI_UI_SCREENSHOT, fullPage: true});
    assert.deepEqual(errors, []);
    const history = Array.from({length: 3}, (_, index) => {
      const work = structuredClone(foundation);
      work.id = `historical-${index}`;
      work.attempts[0].id = `historical-${index}:1`;
      work.source.package = `historical-${index}`;
      work.prerequisites = index ? [`historical-${index - 1}`] : [];
      work.attempts[0].created = '2026-10-30T09:00:00Z';
      work.selection = 0;
      return work;
    });
    for (const work of graph.executions) { work.selection = 3; }
    graph.executions.unshift(...history);
    seedGraph(graph);
    const local = await browser.newContext({locale: 'en-GB', timezoneId: 'America/New_York'});
    const localPage = await local.newPage();
    await localPage.goto(`http://127.0.0.1:${process.argv[4]}/`);
    await localPage.waitForSelector('.execution-row time');
    const historyOrder = await localPage.locator('.execution-row').evaluateAll(rows => rows.map(row => row.id));
    assert.deepEqual(historyOrder.slice(0, 5), rowOrder);
    assert.deepEqual(historyOrder.slice(5), ['work-historical-2', 'work-historical-1', 'work-historical-0']);
    assert.ok(await localPage.locator('.execution-row time').evaluateAll(elements => elements.every(element =>
      element.firstElementChild.textContent ===
        new Intl.DateTimeFormat(undefined, {timeStyle: 'medium', hourCycle: 'h23'}).format(new Date(element.dateTime)))));
    assert.equal(await localPage.evaluate(() => new Intl.DateTimeFormat(undefined, {timeStyle: 'medium'}).resolvedOptions().hour12), false);
    await localPage.setViewportSize({width: 560, height: 900});
    await localPage.locator(`[data-expand="${foundation.id}"]`).click();
    assert.ok(await localPage.locator(`#execution-${foundation.id} .timings tbody tr`).evaluateAll(rows => rows.every(row =>
      new Set([...row.cells].slice(1).map(cell => cell.getBoundingClientRect().top)).size === 1)));
    await local.close();
    graph.executions.splice(0, history.length);
    seedGraph(graph);
    await context.close();
    let stream, reconnect;
    const fixture = await readProjection(process.argv[4]);
    let initial = true, pendingPost, graphFetches = 0;
    const endpoint = http.createServer(async (request, response) => {
      if (request.url === '/events') {
        response.writeHead(200, {'Content-Type': 'text/event-stream', 'Cache-Control': 'no-store'});
        response.write(`data: ${JSON.stringify({repositories: fixture.repositories, executions: {}, ready: false})}\n\n`);
        if (!initial) {
          for (const work of Object.values(fixture.executions).sort((a, b) => a.index - b.index)) {
            response.write(`data: ${JSON.stringify([{op: 'add', path: `/executions/${work.id}`, value: work}])}\n\n`);
          }
          response.write('data: [{"op":"replace","path":"/ready","value":true}]\n\n');
        }
        initial = false;
        stream = response;
        if (reconnect) { reconnect(); }
      } else if (request.method === 'POST') {
        pendingPost = response;
      } else {
        if (request.url === '/api/graph') { graphFetches += 1; }
        const source = await fetch(`http://127.0.0.1:${process.argv[4]}${request.url}`);
        response.writeHead(source.status, {'Content-Type': source.headers.get('Content-Type')});
        response.end(Buffer.from(await source.arrayBuffer()));
      }
    });
    await new Promise(resolve => endpoint.listen(0, '127.0.0.1', resolve));
    const protocol = await browser.newContext({locale: 'de-DE', timezoneId: 'Europe/Vienna'});
    try {
      const changing = await protocol.newPage();
      changing.on('pageerror', error => errors.push(error.message));
      await changing.goto(`http://127.0.0.1:${endpoint.address().port}/`);
      await changing.locator('#connection').filter({hasText: 'Loading'}).waitFor();
      for (const work of Object.values(fixture.executions).sort((a, b) => a.index - b.index)) {
        stream.write(`data: ${JSON.stringify([{op: 'add', path: `/executions/${work.id}`, value: work}])}\n\n`);
      }
      stream.write('data: [{"op":"replace","path":"/ready","value":true}]\n\n');
      await changing.locator('#connection').filter({hasText: 'Live'}).waitFor();
      const at = '2026-10-31T14:23:45Z', started = Date.now();
      const repository = Object.keys(fixture.repositories)[0];
      const path = repository.replace(/~/g, '~0').replace(/\//g, '~1');
      const old = {id: 'old-attempt', revision: 0, state: 'failed', failure_kind: 'retirement',
        retirement: {acknowledgement: '<em>Owner acknowledged unknown</em>', at, remote_outcome: 'unknown'}};
      const updates = [
        {op: 'replace', path: `/executions/${bundle.id}/build/step`, value: 'Link changed fixture'},
        {op: 'replace', path: `/executions/${bundle.id}/build/job/started_at`, value: at},
        {op: 'add', path: `/executions/${foundation.id}/previous/-`, value: old},
        {op: 'remove', path: `/executions/${foundation.id}/previous/0`},
        {op: 'add', path: `/executions/${foundation.id}/previous/-`, value: old},
        {op: 'replace', path: `/repositories/${path}/title`, value: 'Updated repository title'},
        {op: 'replace', path: `/repositories/${path}/definition/title`, value: 'Updated repository title'}
      ];
      fixture.executions[bundle.id].build.step = 'Link changed fixture';
      fixture.executions[bundle.id].build.job.started_at = at;
      fixture.repositories[repository].title = 'Updated repository title';
      fixture.repositories[repository].definition.title = 'Updated repository title';
      fixture.executions[foundation.id].previous = [old];
      stream.write(`data: ${JSON.stringify(updates)}\n\n`);
      await changing.locator(`#work-${bundle.id} .activity`).filter({hasText: 'Link changed fixture'}).waitFor();
      const visibleAfter = Date.now() - started;
      assert.equal(await changing.locator(`#work-${bundle.id} .state`).textContent(), 'building');
      assert.equal(await changing.locator(`#work-${bundle.id} time`).first().getAttribute('datetime'), new Date(at).toISOString());
      await changing.locator(`[data-expand="${bundle.id}"]`).click();
      assert.equal(await changing.locator('.execution-details pre').count(), 0);
      await changing.locator(`[data-expand="${foundation.id}"]`).click();
      assert.ok((await changing.locator(`#execution-${foundation.id}`).textContent()).includes('<em>Owner acknowledged unknown</em>'));
      assert.equal(await changing.locator(`#execution-${foundation.id} em`).count(), 0);
      assert.equal(await changing.locator(`[data-operation="retry"][data-id="${foundation.id}"]`).count(), 0);
      assert.equal(await changing.locator('#repositories > button').filter({hasText: 'Updated repository title'}).count(), 1);
      const uncertaintyLine = changing.locator(`#work-${bundle.id} .activity`);
      const dimensions = await changing.locator(`#work-${bundle.id}`).boundingBox();
      const uncertainty = {at, stage: 'build', kind: 'evidence', reason: 'Artifact evidence unavailable '.repeat(10)};
      fixture.executions[bundle.id].active.uncertainty = uncertainty;
      stream.write(`data: ${JSON.stringify([{op: 'add', path: `/executions/${bundle.id}/active/uncertainty`, value: uncertainty}])}\n\n`);
      await uncertaintyLine.filter({hasText: uncertainty.reason}).waitFor();
      assert.equal(await uncertaintyLine.textContent(), uncertainty.reason);
      assert.equal(await uncertaintyLine.getAttribute('title'), uncertainty.reason);
      assert.equal(await changing.locator(`#work-${bundle.id} .state`).textContent(), 'building');
      assert.equal((await changing.locator(`#work-${bundle.id}`).boundingBox()).height, dimensions.height);
      assert.ok(await uncertaintyLine.evaluate(element => getComputedStyle(element).textOverflow === 'ellipsis' &&
        getComputedStyle(element).whiteSpace === 'nowrap'));
      assert.ok((await changing.locator(`#execution-${bundle.id}`).textContent()).includes('First recorded for this reason:'));
      assert.ok((await changing.locator(`#execution-${bundle.id}`).textContent()).includes('Unresolved build evidence:'));
      stream.write(`data: ${JSON.stringify([{op: 'replace', path: `/executions/${bundle.id}/build/status`, value: 'in_progress'}])}\n\n`);
      await uncertaintyLine.filter({hasText: uncertainty.reason}).waitFor();
      delete fixture.executions[bundle.id].active.uncertainty;
      fixture.executions[bundle.id].active.state = 'publishing';
      stream.write(`data: ${JSON.stringify([{op: 'remove', path: `/executions/${bundle.id}/active/uncertainty`},
        {op: 'replace', path: `/executions/${bundle.id}/active/state`, value: 'publishing'}])}\n\n`);
      await changing.locator(`#work-${bundle.id} .state`).filter({hasText: 'publishing'}).waitFor();
      assert.ok(!(await uncertaintyLine.textContent()).includes('Unresolved'));
      assert.equal((await changing.locator(`#work-${bundle.id}`).boundingBox()).height, dimensions.height);
      assert.equal(await changing.locator(`[data-operation="reconcile"][data-id="${bundle.id}"]`).count(), 1);
      stream.write(`data: ${JSON.stringify([{op: 'replace', path: `/executions/${bundle.id}/active/state`, value: 'failed'},
        {op: 'add', path: `/executions/${bundle.id}/active/failure_kind`, value: 'definite'}])}\n\n`);
      await changing.locator(`[data-operation="retry"][data-id="${bundle.id}"]`).waitFor();
      stream.write(`data: ${JSON.stringify([{op: 'replace', path: `/executions/${bundle.id}/active/failure_kind`, value: 'retirement'}])}\n\n`);
      await changing.locator(`[data-operation="retry"][data-id="${bundle.id}"]`).waitFor({state: 'detached'});
      stream.write(`data: ${JSON.stringify([{op: 'replace', path: `/executions/${bundle.id}/active/state`, value: 'publishing'},
        {op: 'remove', path: `/executions/${bundle.id}/active/failure_kind`},
        {op: 'replace', path: `/executions/${bundle.id}/active/publish_run_bound`, value: false}])}\n\n`);
      await changing.locator(`[data-operation="retire"][data-id="${bundle.id}"]`).waitFor();
      stream.write(`data: ${JSON.stringify([{op: 'replace', path: `/executions/${bundle.id}/active/publish_run_bound`, value: true}])}\n\n`);
      await changing.locator(`[data-operation="retire"][data-id="${bundle.id}"]`).waitFor({state: 'detached'});
      fixture.executions[bundle.id].active.publish_run_bound = true;
      console.log('Chromium: uncertainty line, full tooltip, first-recorded time, SSE clearance and stable layout passed');
      await changing.locator('#repositories-tab').click();
      await changing.locator('#repositories > button').first().click();
      await changing.locator('#branch').fill('unsaved branch');
      await changing.locator('#tag').fill('v9.0.0');
      stream.write(`data: ${JSON.stringify([{op: 'replace', path: `/executions/${bundle.id}/build/step`, value: 'Unrelated progress'}])}\n\n`);
      await changing.waitForFunction(() => document.querySelector('#connection').textContent.includes('Live'));
      assert.equal(await changing.locator('#branch').inputValue(), 'unsaved branch');
      assert.equal(await changing.locator('#tag').inputValue(), 'v9.0.0');
      await changing.locator('#repository').fill('Fixture/new');
      await changing.locator('#import').click();
      await new Promise(resolve => {
        const wait = () => pendingPost ? resolve() : setTimeout(wait, 10);
        wait();
      });
      pendingPost.writeHead(200, {'Content-Type': 'application/json'});
      pendingPost.end('{"error":""}');
      pendingPost = null;
      await changing.locator('#revision').filter({hasText: 'Awaiting committed definition'}).waitFor();
      const metadata = {title: 'New definition', ref: 'committed branch', commit: 'a'.repeat(40), definition: {title: 'New definition'}};
      fixture.repositories['Fixture/new'] = metadata;
      stream.write(`data: ${JSON.stringify([{op: 'add', path: '/repositories/Fixture~1new', value: metadata}])}\n\n`);
      await changing.waitForFunction(() => document.querySelector('#definition').value.includes('New definition'));
      assert.equal(await changing.locator('#branch').inputValue(), 'unsaved branch');
      assert.equal(await changing.locator('#tag').inputValue(), 'v9.0.0');
      await changing.locator('#import').click();
      await new Promise(resolve => {
        const wait = () => pendingPost ? resolve() : setTimeout(wait, 10);
        wait();
      });
      metadata.definition.title = 'SSE before POST response';
      stream.write(`data: ${JSON.stringify([{op: 'replace', path: '/repositories/Fixture~1new', value: metadata}])}\n\n`);
      await changing.waitForFunction(() => document.querySelector('#definition').value.includes('SSE before POST response'));
      pendingPost.writeHead(200, {'Content-Type': 'application/json'});
      pendingPost.end('{"error":""}');
      pendingPost = null;
      await changing.locator('#import').waitFor({state: 'visible'});
      assert.equal(await changing.locator('#branch').inputValue(), 'unsaved branch');
      assert.equal(graphFetches, 0, 'The UI never fetches the raw graph');
      console.log('Chromium: progressive Loading/Live, array patches, plain-text history and both definition refresh orders passed');
      const reconnected = new Promise(resolve => { reconnect = resolve; });
      stream.destroy();
      await Promise.race([reconnected, new Promise((_, reject) => setTimeout(() => reject(new Error('No SSE reconnect')), 7000))]);
      await changing.locator('#connection').filter({hasText: 'Live'}).waitFor();
      assert.equal(await changing.locator(`#work-${bundle.id} .state`).textContent(), 'publishing');
      console.log(`Chromium field-only progress/timestamp update observed within ${visibleAfter} ms; `
        + `update/reconnect scenario ${Date.now() - started} ms; updates=${JSON.stringify(updates).length} bytes`);
      assert.deepEqual(errors, []);
    } finally {
      await protocol.close();
      endpoint.closeAllConnections();
      await new Promise(resolve => endpoint.close(resolve));
    }
    console.log('Chromium: copied layout, filters/tabs, read-only repositories, local dates, multiple expansion/scroll, real retirement and within-state SSE progress passed');
  } finally {
    await browser.close();
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
