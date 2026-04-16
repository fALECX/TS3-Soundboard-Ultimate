const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..');
const packageJsonPath = path.join(root, 'package.json');
const cmakeListsPath = path.join(root, 'CMakeLists.txt');

const pkg = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
const cmake = fs.readFileSync(cmakeListsPath, 'utf8');
const packageIniTemplate = fs.readFileSync(path.join(root, 'src', 'package.ini.in'), 'utf8');
const readme = fs.readFileSync(path.join(root, 'README.md'), 'utf8');
const security = fs.readFileSync(path.join(root, 'SECURITY.md'), 'utf8');

const versionMatch = cmake.match(
  /project\(\s*rp_soundboard_ultimate_ts3_plugin\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)\s+LANGUAGES\s+[A-Z0-9_ ]+\)/i
);

if (!versionMatch) {
  console.error('Release validation failed: could not read the plugin version from CMakeLists.txt.');
  process.exit(1);
}

const cmakeVersion = versionMatch[1];
if (pkg.version !== cmakeVersion) {
  console.error(`Release validation failed: package.json version ${pkg.version} does not match CMake version ${cmakeVersion}.`);
  process.exit(1);
}

const scripts = pkg.scripts || {};
const forbiddenScripts = ['start', 'dev', 'dist', 'pack'];
const presentForbiddenScripts = forbiddenScripts.filter((name) => Object.prototype.hasOwnProperty.call(scripts, name));
if (presentForbiddenScripts.length > 0) {
  console.error(`Release validation failed: obsolete Electron scripts are still present: ${presentForbiddenScripts.join(', ')}.`);
  process.exit(1);
}

if (pkg.main || pkg.build || pkg.releaseConfig) {
  console.error('Release validation failed: Electron build metadata is still present in package.json.');
  process.exit(1);
}

const electronDeps = [];
const dependencyGroups = [pkg.dependencies || {}, pkg.devDependencies || {}];
for (const deps of dependencyGroups) {
  for (const name of ['electron', 'electron-builder']) {
    if (Object.prototype.hasOwnProperty.call(deps, name)) {
      electronDeps.push(name);
    }
  }
}

if (electronDeps.length > 0) {
  console.error(`Release validation failed: obsolete Electron dependencies are still present: ${electronDeps.join(', ')}.`);
  process.exit(1);
}

const packageIniLines = packageIniTemplate.split(/\r?\n/).filter(Boolean);
const descriptionLines = packageIniLines.filter((line) => line.startsWith('Description = '));
if (descriptionLines.length !== 1) {
  console.error('Release validation failed: package.ini template must contain exactly one Description line.');
  process.exit(1);
}

if (packageIniTemplate.includes('\nThis package installs through TeamSpeak') || packageIniTemplate.includes('\r\nThis package installs through TeamSpeak')) {
  console.error('Release validation failed: package.ini template contains a multiline Description, which TeamSpeak installer rejects.');
  process.exit(1);
}

for (const marker of ['twitch.tv/fALECX', 'x.com/fALECX', 'Discord: `falecx`']) {
  if (!readme.includes(marker)) {
    console.error(`Release validation failed: README is missing maintainer attribution marker: ${marker}.`);
    process.exit(1);
  }
}

if (security.includes('your-domain.example')) {
  console.error('Release validation failed: SECURITY.md still contains a placeholder contact address.');
  process.exit(1);
}

for (const obsoletePath of [
  'index.html',
  'main.js',
  'preload.js',
  'renderer.js',
  'renderer-ui.js',
  'renderer-state.js',
  'renderer-playback.js',
  'styles.css',
  path.join('tests', 'smoke.test.js'),
]) {
  if (fs.existsSync(path.join(root, obsoletePath))) {
    console.error(`Release validation failed: obsolete legacy file is still present: ${obsoletePath}.`);
    process.exit(1);
  }
}

console.log(`Release validation passed for RP Soundboard Ultimate ${cmakeVersion}.`);
