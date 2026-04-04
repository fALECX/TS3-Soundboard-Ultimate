const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..');
const packageJsonPath = path.join(root, 'package.json');
const cmakeListsPath = path.join(root, 'CMakeLists.txt');

const pkg = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
const cmake = fs.readFileSync(cmakeListsPath, 'utf8');

const versionMatch = cmake.match(
  /project\(\s*rp_soundboard_ultimate_ts3_plugin\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)\s+LANGUAGES\s+CXX\s*\)/i
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

console.log(`Release validation passed for RP Soundboard Ultimate ${cmakeVersion}.`);
