'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(require('node:path').join(__dirname, '../ui/app.js'), 'utf8');
const document = {createElement() {
    return {textContent: '', children: [], append(child) { this.children.push(child); }, addEventListener() {}};
}};
const context = vm.createContext({document});
vm.runInContext(source.slice(0, source.indexOf('function feedback')), context);
for (const phase of ['building', 'publishing', 'waiting', 'failed', 'published']) {
    for (const conclusion of [null, 'success', 'failure', 'cancelled', 'timed_out']) {
        const work = {active: {state: phase}, build: {conclusion}, publish: {}};
        context.work = work;
        const badge = vm.runInContext('state(work)', context);
        const expected = phase === 'building' && conclusion === 'cancelled' ? 'cancelled' : phase;
        assert.equal(badge.children[0].textContent, expected);
        assert.equal(badge.children[0].className, 'state ' + phase);
        assert.equal(work.active.state, phase, 'Presentation must not mutate assignment state');
    }
}
console.log('25 cancellation presentation cases passed; assignment states preserved');
