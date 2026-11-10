'use strict';
// Deletion only: retain object keys, array order and every surviving value.
function reduce(graph) {
    const result = structuredClone(graph), removed = [];
    function drop(object, key, pointer) {
        if (Object.hasOwn(object, key)) {
            removed.push(pointer + '/' + key);
            delete object[key];
        }
    }
    for (const [name, repository] of Object.entries(result.repositories)) {
        const pointer = '/repositories/' + name.replaceAll('~', '~0').replaceAll('/', '~1');
        for (const key of ['commit', 'definition']) {drop(repository, key, pointer);}
    }
    for (const [id, work] of Object.entries(result.executions)) {
        const pointer = '/executions/' + id;
        for (const key of ['index', 'created', 'selection', 'packages']) {drop(work, key, pointer);}
        for (const key of ['id', 'verification_started_at', 'publish_assigned', 'publish_run_bound', 'retirement', 'uncertainty']) {
            drop(work.active, key, pointer + '/active');
        }
        for (const phase of ['build', 'publish']) {
            if (work[phase]) {drop(work[phase], 'send', pointer + '/' + phase);}
        }
    }
    return {graph: result, removed};
}
module.exports = {reduce};
