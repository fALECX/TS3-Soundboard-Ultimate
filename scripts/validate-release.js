const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..');
const pkg = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8'));
const releaseConfig = pkg.releaseConfig || {};
const youtubeMode = releaseConfig.youtubeMode || 'runtime-optional';
const bundledYtDlp = path.join(root, 'resources', 'yt-dlp.exe');
const hasBundledYtDlp = fs.existsSync(bundledYtDlp);

if (youtubeMode === 'required' && !hasBundledYtDlp) {
  console.error('Release validation failed: youtubeMode is "required" but resources/yt-dlp.exe is missing.');
  process.exit(1);
}

if (youtubeMode === 'disabled' && hasBundledYtDlp) {
  console.warn('Release validation warning: yt-dlp.exe is bundled while youtubeMode is "disabled".');
}

if (youtubeMode === 'runtime-optional' && !hasBundledYtDlp) {
  console.warn('Release validation: yt-dlp.exe is not bundled. Packaged builds will disable YouTube features.');
}

process.exit(0);
