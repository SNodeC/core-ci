'use strict';
const fs = require('fs'), path = require('path'), readline = require('readline');
const {chromium} = require(process.env.CORECI_PLAYWRIGHT_PATH || 'playwright-core');
async function run() {
    const browser = await chromium.launch({executablePath: '/opt/google/chrome/chrome', headless: true,
        args: ['--no-sandbox', '--disable-background-networking']});
    const output = process.argv[3];
    const consoleLog = [];
    let page;
    try {
        if (browser.version() !== '156.0.8078.79') {throw new Error('Reference browser version mismatch');}
        const context = await browser.newContext({ignoreHTTPSErrors: true, viewport: {width: 1280, height: 800}, colorScheme: 'light', locale: 'en-US', timezoneId: 'UTC'});
        page = await context.newPage();
        page.on('pageerror', error => consoleLog.push({type: 'exception', message: error.message}));
        page.on('console', message => {if (['error','warning'].includes(message.type())) {consoleLog.push({type: message.type(), message: message.text()});}});
        const input = readline.createInterface({input: process.stdin});
        for await (const line of input) {
            const command = JSON.parse(line);
            try {
                if (command.action === 'login') {
                    await page.goto(process.argv[2], {waitUntil: 'domcontentloaded'});
                    await page.waitForFunction(() => graph.ready);
                    if (await page.locator('header strong').getAttribute('title') !== 'same@example.invalid') {throw new Error('Email display missing');}
                } else if (command.action === 'wait') {
                    await page.waitForFunction(({target, state, job}) => Object.values(graph.executions).some(work => work.target.id === target && work.active.state === state && (!job || work.build?.job?.status === job)), command, {timeout: 40000});
                } else if (command.action === 'focus') {
                    await page.evaluate(target => {
                        const work = Object.values(graph.executions).find(item => item.target.id === target);
                        element('search').value = work.target.distribution + ' ' + work.target.suite + ' ' + work.target.arch;
                        renderWork();
                    }, command.target);
                    await page.locator('[data-expand]').first().click();
                } else if (command.action === 'retry') {
                    await page.getByRole('button', {name: 'Retry definite failure'}).click();
                } else if (command.action === 'refresh') {
                    await page.locator('#repositories-tab').click();
                    await page.locator('#repositories button').first().click();
                    await page.locator('#import').click();
                    await page.waitForFunction(() => element('feedback').textContent.includes('Declarations loaded'));
                } else if (command.action === 'save') {
                    await page.evaluate(() => document.fonts.ready);
                    await page.screenshot({path: path.join(output, command.name + '.png'), fullPage: true, animations: 'disabled'});
                } else if (command.action === 'finish') {
                    fs.writeFileSync(path.join(output, 'console.json'), JSON.stringify(consoleLog, null, 2));
                    fs.writeFileSync(path.join(output, 'metadata.json'), JSON.stringify({browser: browser.version(), viewport: {width:1280,height:800}, scheme:'light',locale:'en-US',timezone:'UTC',consoleErrors:consoleLog.length},null,2));
                    await context.close();
                    input.close();
                }
                process.stdout.write(JSON.stringify({ok: true}) + '\n');
            } catch (error) {
                process.stdout.write(JSON.stringify({ok: false, error: error.message}) + '\n');
            }
        }
    } finally {
        await browser.close();
    }
}
run().catch(error => {console.error(error.message);process.exitCode=1;});
