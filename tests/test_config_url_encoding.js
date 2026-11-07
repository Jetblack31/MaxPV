// Offline regression test: execute the real administration save function.
// No device, broker or real credentials are used.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const root = process.argv[2] || path.resolve(__dirname, '..');
const html = fs.readFileSync(path.join(root, 'MaxPV3/data/admin.html'), 'utf8');
const source = html.match(/function saveConfigMaxPV\(\) \{[\s\S]*?\n        \}/)[0];

function save(functionSource, user, password) {
    let request;
    const context = {
        document: { getElementById: id => ({ value:
            id === 'mqtt_pass' ? password : id === 'mqtt_user' ? user : '1' }) },
        XMLHttpRequest: function () {
            this.open = (method, url) => {
                assert.equal(method, 'GET');
                // Browser URL serialization drops the fragment from the request.
                const parsed = new URL(url, 'http://offline.invalid/');
                request = parsed.pathname + parsed.search;
            };
            this.send = () => {};
        },
    };
    for (const name of source.matchAll(/\b(\w+)\.checked/g)) {
        context[name[1]] = { checked: false };
    }
    vm.runInNewContext(functionSource + '\nsaveConfigMaxPV();', context);
    // Query splitting and one URL decoding pass, as in ESPAsyncWebServer.
    // Test percent sequences are valid; malformed raw escapes are not modeled.
    const params = new URLSearchParams(request.split('?')[1]);
    return JSON.parse(params.get('value'));
}

const cases = [
    'Simple123', 'abc&def', 'abc#def', 'abc+def', 'abc%41def',
    'abc=def?ghi', 'a"b\\c', "a'b!c$d(e)*f", 'avec espace',
    'été-☀-日本語', 'a&b#c+d%20=e?f', 'x'.repeat(39),
];
for (const value of cases) {
    const result = save(source, value, value);
    assert.equal(result.mqtt_user, value);
    assert.equal(result.mqtt_pass, value);
}
console.log(`PASS: ${cases.length} synthetic username/password round trips`);

const original = source.replace('encodeURIComponent(value_param)', 'value_param');
for (const value of ['abc&def', 'abc#def', 'abc+def', 'abc%41def']) {
    let corrupted = false;
    try {
        const result = save(original, 'test-user', value);
        corrupted = result.mqtt_pass !== value;
    } catch (error) {
        if (!(error instanceof SyntaxError)) throw error;
        corrupted = true;
    }
    assert.ok(corrupted, 'Original code must reproduce corruption');
}
console.log('PASS: original code reproduces all 4 reserved-character failures');
