import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const header = fs.readFileSync(new URL('../lib/transit/src/wifi_setup_web.h', import.meta.url), 'utf8');
const script = header.match(/R"JS\(([\s\S]*?)\)JS"/)[1];
// An unquoted value adjacent to /> consumes the slash in HTML parsing. In SVG,
// that breaks the pupil radius and nests the following slash inside the circle.
const pageSource = fs.readFileSync(new URL('../src/wifi_setup.cpp', import.meta.url), 'utf8');
assert.match(pageSource, /<circle cx='12' cy='12' r='3'\/>/);
assert.doesNotMatch(pageSource, /=[^\s"'<>`=]+\/>/);
assert.match(pageSource, /<path data-password-slash visibility=hidden d='m3 3 18 18'\/>/);
// Source-level lifecycle checks complement native address tests. UDP/DHCP behavior
// still needs the physical phone procedure in docs/wifi-setup.md.
const startSource = pageSource.split('void WifiSetup::start() {')[1].split('void WifiSetup::tick(')[0];
assert.match(startSource, /if \(setup_\)\s*return;/);
assert.match(startSource, /if \(setup_\) \{\s*dns_\.setTTL\(30\);\s*dnsRunning_ = dns_\.start\(53, provisioning::SetupHostname, WiFi.softAPIP\(\)\);/);
assert.match(startSource, /advertiseSetupDns\(WiFi.softAPIP\(\)\)/);
const tickSource = pageSource.split('void WifiSetup::tick(')[1].split('bool WifiSetup::render(')[0];
assert.match(tickSource, /if \(setup_\) \{\s*if \(dnsRunning_\)\s*dns_\.processNextRequest\(\);/);
assert.match(tickSource, /if \(setup_ && success_ && finishAt_ != 0 && now >= finishAt_\) \{[\s\S]*?dns_\.stop\(\);\s*dnsRunning_ = false;\s*server_\.stop\(\);\s*WiFi\.softAPdisconnect\(true\);/);
function harness(initialState, fetcher, remembered = null) {
  const events = {}, clicks = {}, requests = [], timers = [], frames = [];
  const storage = new Map(remembered ? [['display-setup-ssid', remembered]] : []);
  const network = {value:'', options:[{value:''},{value:'Home Network'}], addEventListener(k, f) { events[k] = f; }};
  const ssid = {value:'', focus() { this.focused = true; }};
  network.focus = function() { this.focused = true; };
  const controls = Object.fromEntries(['[data-manual]', '[data-scanned]', '#manual-network', '[data-password]'].map(key =>
    [key, {hidden:false, setAttribute(k,v) { this[k] = v; }, addEventListener(k,f) { this[k] = f; }}]));
  const slash = {visibility:'hidden', setAttribute(k,v) { this[k] = v; }};
  controls['[data-password]'].querySelector = () => slash;
  const form = {elements:{network, ssid, password:{value:'test-only-password', type:'password'}, token:{value:'test-token'}},
    addEventListener(k, f) { events[k] = f; }, reset() { for (const x of Object.values(this.elements)) x.value = ''; }};
  const button = {addEventListener(k, f) { clicks[k] = f; }};
  const main = {innerHTML:initialState === 'configuration' ? '<form></form>' : '',
    querySelector(k) { return k === 'form' ? (initialState === 'configuration' ? form : null) : controls[k] ?? button; }};
  const location = {assign(v) { this.assigned = v; }, replace(v) { this.replaced = v; }};
  const context = {
    document:{querySelector:() => main}, location, URLSearchParams, AbortController,
    FormData:class { constructor(f) { this.values = Object.entries(f.elements).filter(([,v]) => !v.disabled).map(([k,v]) => [k,v.value]); }
      [Symbol.iterator]() { return this.values[Symbol.iterator](); } },
    sessionStorage:{getItem:k => storage.get(k) ?? null, setItem:(k,v) => storage.set(k,v)},
    setTimeout(fn, ms) { const t = {fn, ms}; timers.push(t); return t; },
    clearTimeout(t) { const i = timers.indexOf(t); if (i >= 0) timers.splice(i,1); },
    requestAnimationFrame(fn) { frames.push(fn); },
    async fetch(url, options) { requests.push({url, options}); return fetcher(url, options); }
  };
  vm.runInNewContext("const token='test-token';const initialState=" + JSON.stringify(initialState) + ';' + script, context);
  return {main, form, controls, slash, events, clicks, requests, frames, timers, storage, location,
    async retry() { const i = timers.findIndex(t => t.ms === 1000); assert.ok(i >= 0); await timers.splice(i,1)[0].fn(); await settle(); }};
}
const settle = () => new Promise(resolve => setImmediate(resolve));
const response = state => ({ok:true, status:200, json:async () => ({state})});
let resolveSubmit, statusCalls = 0;
const one = harness('configuration', (url) => {
  if (url === '/connect') return new Promise(resolve => { resolveSubmit = resolve; });
  assert.equal(url, '/status');
  if (++statusCalls === 1) throw new Error('simulated channel interruption');
  return response('failed');
});
assert.equal(one.form.elements.password.type, 'password');
assert.equal(one.slash.visibility, 'hidden');
one.controls['[data-password]'].click();
assert.equal(one.form.elements.password.type, 'text');
assert.equal(one.slash.visibility, 'visible');
assert.equal(one.form.elements.password.value, 'test-only-password');
assert.equal(one.controls['[data-password]']['aria-label'], 'Hide password');
assert.equal(one.controls['[data-password]']['aria-pressed'], 'true');
one.controls['[data-password]'].click();
assert.equal(one.form.elements.password.type, 'password');
assert.equal(one.slash.visibility, 'hidden');
assert.equal(one.form.elements.password.value, 'test-only-password');
assert.equal(one.controls['[data-password]']['aria-label'], 'Show password');
assert.equal(one.controls['[data-password]']['aria-pressed'], 'false');
one.form.elements.network.value = 'Home Network';
const submitted = one.events.submit({preventDefault(){}});
assert.match(one.main.innerHTML, /Connecting…/);
assert.match(one.main.innerHTML, /class="loading"/);
assert.doesNotMatch(one.main.innerHTML, /<form|<input|<button|test-only-password|20|timeout/);
assert.equal(one.form.elements.password.value, '');
assert.equal(one.requests[0].options.body.get('password'), 'test-only-password');
assert.deepEqual([...one.storage.entries()], [['display-setup-ssid','Home Network']]);
resolveSubmit(response('connecting'));
await submitted;
await settle();
assert.match(one.main.innerHTML, /Connecting…/);
await one.retry();
assert.match(one.main.innerHTML, /Couldn’t connect/);
assert.match(one.main.innerHTML, /Try again/);
one.clicks.click();
assert.equal(one.location.assigned, '/?configure=1');
const restored = harness('configuration', () => response('configuration'), one.storage.get('display-setup-ssid'));
assert.equal(restored.form.elements.network.value, 'Home Network');
let acknowledgements = 0;
const two = harness('connecting', url => {
  if (url === '/status') return response('success');
  assert.equal(url, '/complete');
  if (++acknowledgements === 1) throw new Error('simulated acknowledgement interruption');
  return {ok:true, status:204};
});
await settle();
assert.match(two.main.innerHTML, /Connected!/);
assert.doesNotMatch(two.main.innerHTML, /<form|<input|<button|password|Retry/);
assert.equal(acknowledgements, 0);
two.frames.shift()();
assert.equal(acknowledgements, 0);
two.frames.shift()();
await settle();
assert.equal(acknowledgements, 1);
await two.retry();
assert.equal(acknowledgements, 2);
assert.equal(two.timers.length, 0);
for (const {url, options} of two.requests) {
  assert.equal(options.body, undefined);
  assert.equal(options.headers['X-Setup-Token'], 'test-token');
  assert.ok(!url.includes('?'));
}
let posts = 0;
const three = harness('configuration', url => {
  if (url === '/connect') { ++posts; throw new Error('simulated lost submission'); }
  return response('configuration');
});
assert.equal(three.controls['#manual-network'].hidden, true);
assert.equal(three.form.elements.ssid.disabled, true);
three.controls['[data-manual]'].click();
assert.equal(three.controls['#manual-network'].hidden, false);
assert.equal(three.controls['[data-manual]']['aria-expanded'], 'true');
assert.equal(three.form.elements.network.disabled, true);
assert.equal(three.form.elements.ssid.focused, true);
assert.equal(three.form.elements.password.disabled, undefined);
three.form.elements.ssid.value = 'Hidden Network';
three.form.elements.network.value = 'Home Network';
await three.events.submit({preventDefault(){}});
await settle();
assert.match(three.main.innerHTML, /Couldn’t connect/);
assert.equal(posts, 1);
assert.equal(three.storage.get('display-setup-ssid'), 'Hidden Network');
assert.equal(three.requests[0].options.body.get('ssid'), 'Hidden Network');
assert.equal(three.requests[0].options.body.has('network'), false);
const manualRetry = harness('configuration', () => response('configuration'), 'Hidden Network');
assert.equal(manualRetry.form.elements.ssid.value, 'Hidden Network');
assert.equal(manualRetry.controls['#manual-network'].hidden, false);
manualRetry.controls['[data-scanned]'].click();
assert.equal(manualRetry.controls['#manual-network'].hidden, true);
assert.equal(manualRetry.form.elements.network.disabled, false);
assert.equal(manualRetry.form.elements.network.focused, true);
assert.equal(manualRetry.form.elements.ssid.disabled, true);
manualRetry.form.elements.network.value = 'Home Network';
await manualRetry.events.submit({preventDefault(){}});
assert.equal(manualRetry.requests[0].options.body.has('ssid'), false);
assert.equal(manualRetry.requests[0].options.body.get('network'), 'Home Network');
assert.equal(manualRetry.storage.get('display-setup-ssid'), 'Home Network');
console.log('Browser script checks passed: immediate progress, interrupted poll, failure/retry, SSID retention, success paint ordering, interrupted acknowledgement, lost submission, and credential isolation.');
