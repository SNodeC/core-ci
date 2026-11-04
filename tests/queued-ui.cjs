'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(require('node:path').join(__dirname, '../ui/app.js'), 'utf8');
const context = vm.createContext({});
vm.runInContext(source.slice(source.indexOf('function displayState'), source.indexOf('function state')), context);
function shown(work) {
    context.work = work;
    const before = JSON.stringify(work);
    const value = vm.runInContext('displayState(work)', context);
    assert.equal(JSON.stringify(work), before, 'Presentation must retain authoritative state and evidence');
    return value;
}
for (const phase of ['building', 'publishing', 'waiting', 'failed', 'published']) {
    for (const status of ['requested', 'queued', 'pending', 'waiting', 'in_progress', 'completed']) {
        assert.equal(shown({active: {state: phase}, build: {job: {status}}}),
            phase === 'building' && ['requested', 'queued', 'pending', 'waiting'].includes(status) ? 'queued' : phase);
    }
}
assert.equal(shown({active: {state: 'building'}, build: {status: 'queued'}}), 'queued');
assert.equal(shown({active: {state: 'building'}, build: {status: 'queued', job: {status: 'in_progress'}}}), 'building');
assert.equal(shown({active: {state: 'building'}, build: {conclusion: 'cancelled', job: {status: 'completed'}}}), 'cancelled');
const works = [
    ...Array.from({length: 20}, () => ({active: {state: 'building'}, build: {job: {status: 'in_progress'}}})),
    ...Array.from({length: 56}, () => ({active: {state: 'building'}, build: {job: {status: 'queued'}}})),
    ...Array.from({length: 102}, () => ({active: {state: 'waiting'}}))
];
const counts = {};
for (const work of works) {
    const value = shown(work);
    counts[value] = (counts[value] || 0) + 1;
}
assert.deepEqual(counts, {building: 20, queued: 56, waiting: 102});
console.log('Queued/running presentation passed: 20 building, 56 queued, 102 prerequisites waiting; authority unchanged');
