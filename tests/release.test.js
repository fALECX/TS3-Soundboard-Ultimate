const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..');

function read(relativePath) {
  return fs.readFileSync(path.join(root, relativePath), 'utf8');
}

test('package version matches CMake project version', () => {
  const pkg = JSON.parse(read('package.json'));
  const cmake = read('CMakeLists.txt');
  const match = cmake.match(
    /project\(\s*rp_soundboard_ultimate_ts3_plugin\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)\s+LANGUAGES\s+[A-Z0-9_ ]+\)/i
  );

  assert.ok(match, 'CMake project version was not found');
  assert.equal(pkg.version, match[1]);
});

test('README honors the maintainer and documents the native plugin flow', () => {
  const readme = read('README.md');

  assert.match(readme, /twitch\.tv\/fALECX/i);
  assert.match(readme, /x\.com\/fALECX/i);
  assert.match(readme, /Discord:\s*`falecx`/i);
  assert.match(readme, /TeamSpeak 3 soundboard plugin/i);
  assert.doesNotMatch(readme, /Legacy Electron source files remain in the tree/i);
});

test('security and contribution docs no longer contain placeholder or Electron-era guidance', () => {
  const contributing = read('CONTRIBUTING.md');
  const security = read('SECURITY.md');

  assert.doesNotMatch(contributing, /npm run dev|npm run dist|IPC/i);
  assert.doesNotMatch(security, /your-domain\.example/i);
});

test('obsolete web entry files are absent from the repo root', () => {
  for (const file of [
    'index.html',
    'main.js',
    'preload.js',
    'renderer.js',
    'renderer-ui.js',
    'renderer-state.js',
    'renderer-playback.js',
    'styles.css',
  ]) {
    assert.equal(fs.existsSync(path.join(root, file)), false, `${file} should not exist`);
  }
});
