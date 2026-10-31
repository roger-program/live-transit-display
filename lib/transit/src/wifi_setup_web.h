#pragma once
namespace provisioning {
inline constexpr char WifiPageHead[] =
    "<!doctype html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Display Wi-Fi</title><style>"
    "body{font:18px system-ui;max-width:32rem;margin:2rem auto;padding:1rem}"
    "main{padding:2rem 0}h1{margin:0 0 2rem}p{margin:1.5rem 0;line-height:1.6}"
    "input,select,button{font:inherit;box-sizing:border-box;width:100%;padding:.8rem;"
    "margin:.4rem 0 1rem}label{display:block}"
    ".link{width:auto;padding:.3rem 0;color:#555;background:none;border:0;"
    "text-decoration:underline;font-size:.9em;text-align:left;cursor:pointer}"
    ".loading{width:1.6rem;height:1.6rem;border:2px solid #ddd;border-top-color:#666;"
    "border-radius:50%;margin:2rem 0;animation:turn 1s linear infinite}"
    "@keyframes turn{to{transform:rotate(360deg)}}"
    "@media(prefers-reduced-motion:reduce){.loading{animation:none}}"
    "</style></head><body><main aria-live=polite>";
inline constexpr char WifiConnectingContent[] =
    "<h1>Connecting…</h1><p>Setting up your display.</p>"
    "<div class=loading aria-hidden=true></div>";
inline constexpr char WifiSuccessContent[] =
    "<h1>Connected!</h1><p>Your display is ready.</p><p>You can close this page.</p>";
inline constexpr char WifiFailureContent[] =
    "<h1>Couldn’t connect</h1><p>Check your Wi-Fi password and try again.</p>"
    "<button type=button data-retry>Try again</button>";
// token and initialState are supplied by firmware; neither contains user credentials.
inline constexpr char WifiSetupScript[] = R"JS(
const main = document.querySelector('main');
const retry = () => location.assign('/?configure=1');
let finished = false;
async function request(url, options = {}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 4000);
  try {
    return await fetch(url, {...options, cache:'no-store', credentials:'omit',
      headers:{...options.headers, 'X-Setup-Token':token}, signal:controller.signal});
  } finally { clearTimeout(timer); }
}
function failure() {
  finished = true;
  main.innerHTML = '<h1>Couldn’t connect</h1><p>Check your Wi-Fi password and try again.</p>' +
    '<button type="button" data-retry>Try again</button>';
  main.querySelector('[data-retry]').addEventListener('click', retry);
}
async function acknowledge() {
  try {
    const response = await request('/complete', {method:'POST'});
    if (response.ok) return;
  } catch (_) {}
  setTimeout(acknowledge, 1000);
}
function success() {
  finished = true;
  main.innerHTML = '<h1>Connected!</h1><p>Your display is ready.</p><p>You can close this page.</p>';
  // Allow a paint before acknowledging; firmware then leaves the AP up for five seconds.
  requestAnimationFrame(() => requestAnimationFrame(acknowledge));
}
function connecting() {
  main.innerHTML = '<h1>Connecting…</h1><p>Setting up your display.</p>' +
    '<div class="loading" aria-hidden="true"></div>';
}
async function poll() {
  if (finished) return;
  try {
    const response = await request('/status');
    if (response.status === 403) { location.replace('/'); return; }
    if (response.ok) {
      const result = await response.json();
      if (result.state === 'success') { success(); return; }
      if (result.state === 'failed' || result.state === 'configuration') { failure(); return; }
    }
  } catch (_) {} // A channel switch or a lost request leaves progress visible; retry quietly.
  setTimeout(poll, 1000);
}
const form = main.querySelector('form');
if (form) {
  const network = form.elements.network;
  const ssid = form.elements.ssid;
  const manualButton = main.querySelector('[data-manual]');
  const scannedButton = main.querySelector('[data-scanned]');
  const manualFields = main.querySelector('#manual-network');
  let manual = false;
  function selectMode(value) {
    manual = value;
    manualFields.hidden = !manual;
    manualButton.hidden = manual;
    manualButton.setAttribute('aria-expanded', String(manual));
    network.disabled = manual;
    ssid.disabled = !manual;
    ssid.required = manual;
    network.required = !manual;
  }
  manualButton.addEventListener('click', () => { selectMode(true); ssid.focus(); });
  scannedButton.addEventListener('click', () => { selectMode(false); network.focus(); });
  try {
    const remembered = sessionStorage.getItem('display-setup-ssid');
    if (remembered && !ssid.value && !network.value) {
      if ([...network.options].some(option => option.value === remembered)) network.value = remembered;
      else ssid.value = remembered;
    }
  } catch (_) {}
  selectMode(Boolean(ssid.value));
  form.addEventListener('submit', async event => {
    event.preventDefault();
    const body = new URLSearchParams(new FormData(form));
    try { sessionStorage.setItem('display-setup-ssid', manual ? ssid.value : network.value); } catch (_) {}
    form.reset(); // Do not keep the submitted password in the rendered DOM.
    connecting();
    try {
      const response = await request('/connect', {method:'POST', body});
      if (!response.ok) { failure(); return; }
    } catch (_) {} // Submission may have reached the device even if its response was interrupted.
    poll();
  });
}
if (initialState === 'connecting') poll();
else if (initialState === 'success') success();
else if (initialState === 'failed') main.querySelector('[data-retry]').addEventListener('click', retry);
)JS";
} // namespace provisioning
