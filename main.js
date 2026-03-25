const { app, BrowserWindow, dialog, globalShortcut, ipcMain, shell } = require('electron');
const fs = require('fs');
const https = require('https');
const path = require('path');
const { spawn, spawnSync } = require('child_process');
const { pathToFileURL } = require('url');

const {
  AUDIO_EXTENSIONS,
  STARTER_SAMPLES,
  createBoardRecord,
  createSoundRecord,
  ensureUniqueFilename,
  migrateLegacyData,
  normalizeBoardsConfig,
  normalizeLibrary,
  sanitizeFilenameBase,
} = require('./lib/data-model');
const { validateHotkeyEntries } = require('./lib/hotkeys');

const LEGACY_ROOT = __dirname;

let DATA_ROOT = '';
let SOUNDS_DIR = '';
let APP_CONFIG_FILE = '';
let LIBRARY_FILE = '';
let BOARDS_FILE = '';

let mainWindow = null;

const DEFAULT_APP_CONFIG = {
  firstRunComplete: false,
  freesoundApiKey: '',
  outputDevice: 'default',
  masterVolume: 0.8,
  globalHotkeysEnabled: true,
};

function ensureDir(dirPath) {
  if (!fs.existsSync(dirPath)) fs.mkdirSync(dirPath, { recursive: true });
}

function readJson(filePath, fallback) {
  try {
    if (fs.existsSync(filePath)) {
      return JSON.parse(fs.readFileSync(filePath, 'utf8'));
    }
  } catch (error) {
    console.error(`Failed to read ${filePath}:`, error);
  }
  return fallback;
}

function writeJson(filePath, value) {
  fs.writeFileSync(filePath, JSON.stringify(value, null, 2));
}

function initializeDataPaths() {
  DATA_ROOT = app.getPath('userData');
  SOUNDS_DIR = path.join(DATA_ROOT, 'sounds');
  APP_CONFIG_FILE = path.join(DATA_ROOT, 'app-config.json');
  LIBRARY_FILE = path.join(DATA_ROOT, 'library.json');
  BOARDS_FILE = path.join(DATA_ROOT, 'boards-config.json');

  ensureDir(DATA_ROOT);
  ensureDir(SOUNDS_DIR);
}

function getBundledSoundsDir() {
  return path.join(__dirname, 'sounds');
}

function listSoundFiles(dirPath = SOUNDS_DIR) {
  if (!fs.existsSync(dirPath)) return [];
  return fs
    .readdirSync(dirPath, { withFileTypes: true })
    .filter((entry) => entry.isFile() && AUDIO_EXTENSIONS.has(path.extname(entry.name).toLowerCase()))
    .map((entry) => entry.name)
    .sort((a, b) => a.localeCompare(b));
}

function copyFileIfMissing(sourcePath, targetPath) {
  if (fs.existsSync(sourcePath) && !fs.existsSync(targetPath)) {
    fs.copyFileSync(sourcePath, targetPath);
  }
}

function copyBundledStarterSounds() {
  const bundledSoundsDir = getBundledSoundsDir();
  if (!fs.existsSync(bundledSoundsDir)) return;

  for (const sample of STARTER_SAMPLES) {
    const sourcePath = path.join(bundledSoundsDir, sample.filename);
    const targetPath = path.join(SOUNDS_DIR, sample.filename);
    copyFileIfMissing(sourcePath, targetPath);
  }
}

function migrateLegacySoundFiles() {
  const legacySoundsDir = path.join(LEGACY_ROOT, 'sounds');
  if (!fs.existsSync(legacySoundsDir)) return;

  for (const entry of fs.readdirSync(legacySoundsDir, { withFileTypes: true })) {
    if (!entry.isFile()) continue;
    if (!AUDIO_EXTENSIONS.has(path.extname(entry.name).toLowerCase())) continue;
    copyFileIfMissing(path.join(legacySoundsDir, entry.name), path.join(SOUNDS_DIR, entry.name));
  }
}

function getLegacyGridConfig() {
  const candidates = [
    path.join(DATA_ROOT, 'grid-config.json'),
    path.join(LEGACY_ROOT, 'grid-config.json'),
  ];

  for (const candidate of candidates) {
    const data = readJson(candidate, null);
    if (data) return data;
  }
  return null;
}

function getLegacyAppConfig() {
  const candidates = [
    path.join(DATA_ROOT, 'app-config.json'),
    path.join(LEGACY_ROOT, 'app-config.json'),
  ];

  for (const candidate of candidates) {
    const data = readJson(candidate, null);
    if (data) return data;
  }
  return null;
}

function ensureAppConfig() {
  const legacyGrid = getLegacyGridConfig();
  const legacyApp = getLegacyAppConfig() || {};

  const nextConfig = {
    ...DEFAULT_APP_CONFIG,
    ...legacyApp,
    outputDevice: legacyApp.outputDevice || legacyGrid?.outputDevice || DEFAULT_APP_CONFIG.outputDevice,
    masterVolume:
      typeof legacyApp.masterVolume === 'number'
        ? legacyApp.masterVolume
        : legacyGrid?.volume ?? DEFAULT_APP_CONFIG.masterVolume,
  };

  writeJson(APP_CONFIG_FILE, nextConfig);
  return nextConfig;
}

function ensureDataFiles() {
  copyBundledStarterSounds();
  migrateLegacySoundFiles();

  const soundFiles = listSoundFiles();
  let appConfig = readJson(APP_CONFIG_FILE, null);
  if (!appConfig) appConfig = ensureAppConfig();
  else {
    appConfig = { ...DEFAULT_APP_CONFIG, ...appConfig };
    writeJson(APP_CONFIG_FILE, appConfig);
  }

  const libraryExists = fs.existsSync(LIBRARY_FILE);
  const boardsExist = fs.existsSync(BOARDS_FILE);

  if (!libraryExists || !boardsExist) {
    const migrated = migrateLegacyData({
      legacyGridConfig: getLegacyGridConfig(),
      soundFiles,
    });

    writeJson(LIBRARY_FILE, migrated.library);
    writeJson(BOARDS_FILE, migrated.boards);
  }

  let library = normalizeLibrary(readJson(LIBRARY_FILE, {}));
  const knownFiles = new Set(Object.values(library).map((sound) => sound.filename.toLowerCase()));

  soundFiles.forEach((filename) => {
    if (knownFiles.has(filename.toLowerCase())) return;
    const sound = createSoundRecord({ filename, sourceType: 'local' });
    library[sound.soundId] = sound;
  });

  let boards = normalizeBoardsConfig(readJson(BOARDS_FILE, null), library);

  if (!boards.boards.length) {
    const board = createBoardRecord();
    boards = {
      version: 2,
      activeBoardId: board.id,
      boards: [board],
    };
  }

  writeJson(LIBRARY_FILE, library);
  writeJson(BOARDS_FILE, boards);
  writeJson(APP_CONFIG_FILE, appConfig);
}

function loadAppConfig() {
  return { ...DEFAULT_APP_CONFIG, ...readJson(APP_CONFIG_FILE, {}) };
}

function saveAppConfig(config) {
  const nextConfig = { ...DEFAULT_APP_CONFIG, ...(config || {}) };
  writeJson(APP_CONFIG_FILE, nextConfig);
  return nextConfig;
}

function loadLibrary() {
  return normalizeLibrary(readJson(LIBRARY_FILE, {}));
}

function saveLibrary(library) {
  const normalized = normalizeLibrary(library);
  writeJson(LIBRARY_FILE, normalized);
  return normalized;
}

function loadBoards(library) {
  return normalizeBoardsConfig(readJson(BOARDS_FILE, null), library);
}

function saveBoards(config, library) {
  const normalized = normalizeBoardsConfig(config, library);
  writeJson(BOARDS_FILE, normalized);
  return normalized;
}

function loadState() {
  const appConfig = loadAppConfig();
  const library = loadLibrary();
  const boards = loadBoards(library);
  return {
    appConfig,
    library,
    boards,
    status: getSystemStatus(),
  };
}

function saveState(payload = {}) {
  const appConfig = saveAppConfig(payload.appConfig || {});
  const library = saveLibrary(payload.library || {});
  const boards = saveBoards(payload.boards || {}, library);
  return {
    appConfig,
    library,
    boards,
    status: getSystemStatus(),
  };
}

function findBundledYtDlpPath() {
  return app.isPackaged
    ? path.join(process.resourcesPath, 'yt-dlp.exe')
    : path.join(__dirname, 'resources', 'yt-dlp.exe');
}

function resolveYtDlpStatus() {
  const bundledPath = findBundledYtDlpPath();
  if (fs.existsSync(bundledPath)) {
    return {
      available: true,
      mode: 'bundled',
      bundled: true,
      path: bundledPath,
      message: 'Bundled yt-dlp detected.',
    };
  }

  if (app.isPackaged) {
    return {
      available: false,
      mode: 'disabled',
      bundled: false,
      path: null,
      message: 'Packaged build has no bundled yt-dlp, so YouTube is disabled.',
    };
  }

  const probe = spawnSync('where', ['yt-dlp'], { encoding: 'utf8', windowsHide: true });
  if (probe.status === 0 && probe.stdout.trim()) {
    return {
      available: true,
      mode: 'path',
      bundled: false,
      path: 'yt-dlp',
      message: 'Using yt-dlp from PATH.',
    };
  }

  return {
    available: false,
    mode: 'missing',
    bundled: false,
    path: null,
    message: 'yt-dlp not found.',
  };
}

function getSystemStatus() {
  return {
    youtube: resolveYtDlpStatus(),
  };
}

function getUniqueFilenameForImport(originalName) {
  return ensureUniqueFilename(new Set(listSoundFiles()), originalName);
}

function getUniqueBaseName(baseName) {
  const currentNames = new Set(
    listSoundFiles().map((filename) => path.basename(filename, path.extname(filename)).toLowerCase())
  );
  let candidate = sanitizeFilenameBase(baseName);
  let counter = 2;
  while (currentNames.has(candidate.toLowerCase())) {
    candidate = `${sanitizeFilenameBase(baseName)} (${counter})`;
    counter += 1;
  }
  return candidate;
}

function sendDownloadProgress(payload) {
  mainWindow?.webContents.send('download:progress', payload);
}

function createWindow() {
  const iconPath = path.join(__dirname, 'assets', 'icon.ico');

  mainWindow = new BrowserWindow({
    width: 1360,
    height: 860,
    minWidth: 980,
    minHeight: 640,
    frame: false,
    backgroundColor: '#08111b',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
    icon: fs.existsSync(iconPath) ? iconPath : undefined,
  });

  mainWindow.loadFile('index.html');
  mainWindow.setMenuBarVisibility(false);
  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

function registerWindowIpc() {
  ipcMain.on('window:minimize', () => mainWindow?.minimize());
  ipcMain.on('window:maximize', () => {
    if (!mainWindow) return;
    if (mainWindow.isMaximized()) mainWindow.unmaximize();
    else mainWindow.maximize();
  });
  ipcMain.on('window:close', () => mainWindow?.close());
}

function registerStateIpc() {
  ipcMain.handle('shell:openExternal', async (_, url) => shell.openExternal(url));
  ipcMain.handle('system:getStatus', async () => getSystemStatus());
  ipcMain.handle('state:load', async () => loadState());
  ipcMain.handle('state:save', async (_, payload) => saveState(payload));
  ipcMain.handle('sounds:getDir', async () => SOUNDS_DIR);
  ipcMain.handle('sounds:getFileUrl', async (_, filename) => {
    const filePath = path.join(SOUNDS_DIR, filename);
    return pathToFileURL(filePath).href;
  });
  ipcMain.handle('sound:deleteFile', async (_, filename) => {
    const filePath = path.join(SOUNDS_DIR, filename);
    try {
      if (fs.existsSync(filePath)) fs.unlinkSync(filePath);
      return { success: true };
    } catch (error) {
      return { success: false, error: error.message };
    }
  });
}

function registerImportIpc() {
  ipcMain.handle('files:import', async () => {
    const result = await dialog.showOpenDialog(mainWindow, {
      properties: ['openFile', 'multiSelections'],
      filters: [{ name: 'Audio', extensions: ['mp3', 'wav', 'ogg', 'flac', 'm4a', 'webm', 'opus', 'mp4'] }],
    });

    if (result.canceled) return [];

    const imported = [];
    for (const sourcePath of result.filePaths) {
      const ext = path.extname(sourcePath);
      const baseName = path.basename(sourcePath, ext);
      const destinationFilename = getUniqueFilenameForImport(`${sanitizeFilenameBase(baseName)}${ext}`);
      const destinationPath = path.join(SOUNDS_DIR, destinationFilename);
      fs.copyFileSync(sourcePath, destinationPath);
      imported.push({
        filename: destinationFilename,
        displayName: baseName,
        sourceType: 'local',
      });
    }

    return imported;
  });
}

function registerYoutubeIpc() {
  ipcMain.handle('youtube:search', async (_, query) => {
    const status = resolveYtDlpStatus();
    if (!status.available) {
      return {
        results: [],
        error: 'ytdlp_unavailable',
        warning: status.message,
      };
    }

    return new Promise((resolve) => {
      const proc = spawn(
        status.path,
        [`ytsearch10:${query}`, '--dump-json', '--flat-playlist', '--no-warnings'],
        { windowsHide: true }
      );

      let stdout = '';
      let stderr = '';
      let settled = false;

      const finish = (payload) => {
        if (settled) return;
        settled = true;
        resolve(payload);
      };

      proc.stdout.on('data', (data) => {
        stdout += data.toString();
      });

      proc.stderr.on('data', (data) => {
        stderr += data.toString();
      });

      proc.on('error', () => {
        finish({
          results: [],
          error: 'ytdlp_unavailable',
          warning: 'yt-dlp could not be started.',
        });
      });

      proc.on('close', (code) => {
        if (code !== 0) {
          console.error('yt-dlp search error:', stderr);
          finish({
            results: [],
            error: 'search_failed',
            warning: stderr.substring(0, 240) || 'YouTube search failed.',
          });
          return;
        }

        try {
          const results = stdout
            .trim()
            .split('\n')
            .filter(Boolean)
            .map((line) => JSON.parse(line))
            .map((entry) => ({
              id: entry.id,
              title: entry.title,
              duration: entry.duration,
              url: entry.url || `https://www.youtube.com/watch?v=${entry.id}`,
              thumbnail: entry.thumbnail || entry.thumbnails?.[0]?.url,
              channel: entry.channel || entry.uploader,
            }));

          finish({ results, error: null, warning: null });
        } catch (error) {
          console.error('yt-dlp parse error:', error);
          finish({
            results: [],
            error: 'parse_failed',
            warning: 'Failed to parse YouTube results.',
          });
        }
      });

      setTimeout(() => {
        proc.kill();
        finish({
          results: [],
          error: 'timeout',
          warning: 'YouTube search timed out.',
        });
      }, 15000);
    });
  });

  ipcMain.handle('youtube:getStreamUrl', async (_, url) => {
    const status = resolveYtDlpStatus();
    if (!status.available) {
      return { success: false, error: status.message };
    }

    return new Promise((resolve) => {
      const proc = spawn(
        status.path,
        [url, '--get-url', '--format', 'bestaudio[ext=m4a]/bestaudio[ext=webm]/bestaudio/best', '--no-playlist', '--no-warnings'],
        { windowsHide: true }
      );

      let stdout = '';
      let stderr = '';
      let settled = false;

      const finish = (payload) => {
        if (settled) return;
        settled = true;
        resolve(payload);
      };

      proc.stdout.on('data', (data) => {
        stdout += data.toString();
      });

      proc.stderr.on('data', (data) => {
        stderr += data.toString();
      });

      proc.on('error', () => finish({ success: false, error: 'yt-dlp could not be started.' }));
      proc.on('close', (code) => {
        const streamUrl = stdout.trim().split('\n').filter(Boolean)[0];
        if (code === 0 && streamUrl) {
          finish({ success: true, url: streamUrl });
          return;
        }
        finish({ success: false, error: stderr.substring(0, 200) || 'Unable to fetch stream URL.' });
      });

      setTimeout(() => {
        proc.kill();
        finish({ success: false, error: 'Stream URL request timed out.' });
      }, 15000);
    });
  });

  ipcMain.handle('youtube:download', async (_, { url, filenameBase, operationId }) => {
    const status = resolveYtDlpStatus();
    if (!status.available) {
      return { success: false, error: status.message };
    }

    const uniqueBase = getUniqueBaseName(filenameBase);
    const outputTemplate = path.join(SOUNDS_DIR, `${uniqueBase}.%(ext)s`);

    return new Promise((resolve) => {
      const proc = spawn(
        status.path,
        [
          url,
          '--format',
          'bestaudio[ext=m4a]/bestaudio[ext=webm]/bestaudio/best',
          '--no-playlist',
          '--no-warnings',
          '--newline',
          '--print',
          'after_move:__FINAL__:%(filepath)s',
          '-o',
          outputTemplate,
        ],
        { windowsHide: true }
      );

      let stdout = '';
      let stderr = '';
      let settled = false;

      const finish = (payload) => {
        if (settled) return;
        settled = true;
        resolve(payload);
      };

      const onProgressChunk = (chunk) => {
        const text = chunk.toString();
        stderr += text;
        const match = text.match(/(\d+(?:\.\d+)?)%/);
        if (!match) return;
        sendDownloadProgress({
          operationId,
          source: 'youtube',
          percent: Number(match[1]),
          status: 'downloading',
          label: `${match[1]}%`,
        });
      };

      proc.stdout.on('data', (data) => {
        const text = data.toString();
        stdout += text;
      });
      proc.stderr.on('data', onProgressChunk);

      proc.on('error', () => {
        finish({ success: false, error: 'yt-dlp could not be started.' });
      });

      proc.on('close', (code) => {
        const finalLine = stdout
          .trim()
          .split('\n')
          .find((line) => line.startsWith('__FINAL__:'));
        const finalPath = finalLine ? finalLine.replace('__FINAL__:', '').trim() : null;

        if (code === 0 && finalPath && fs.existsSync(finalPath)) {
          sendDownloadProgress({
            operationId,
            source: 'youtube',
            percent: 100,
            status: 'done',
            label: 'Complete',
          });
          finish({
            success: true,
            filename: path.basename(finalPath),
            displayName: filenameBase,
            sourceType: 'youtube',
            sourceUrl: url,
          });
          return;
        }

        console.error('yt-dlp download error:', stderr);
        sendDownloadProgress({
          operationId,
          source: 'youtube',
          percent: 0,
          status: 'failed',
          label: 'Failed',
        });
        finish({
          success: false,
          error: stderr.substring(0, 240) || 'YouTube download failed.',
        });
      });

      setTimeout(() => {
        proc.kill();
        sendDownloadProgress({
          operationId,
          source: 'youtube',
          percent: 0,
          status: 'failed',
          label: 'Timed out',
        });
        finish({ success: false, error: 'YouTube download timed out.' });
      }, 120000);
    });
  });
}

function registerFreesoundIpc() {
  ipcMain.handle('freesound:search', async (_, query) => {
    const { freesoundApiKey } = loadAppConfig();
    if (!freesoundApiKey) {
      return {
        results: [],
        noApiKey: true,
        invalidKey: false,
        error: null,
      };
    }

    return new Promise((resolve) => {
      const url = `https://freesound.org/apiv2/search/text/?query=${encodeURIComponent(
        query
      )}&fields=id,name,duration,previews,tags&page_size=15&token=${freesoundApiKey}`;

      https
        .get(url, (response) => {
          let raw = '';

          response.on('data', (chunk) => {
            raw += chunk.toString();
          });

          response.on('end', () => {
            if (response.statusCode === 401 || response.statusCode === 403) {
              resolve({
                results: [],
                noApiKey: false,
                invalidKey: true,
                error: 'Authentication failed.',
              });
              return;
            }

            try {
              const payload = JSON.parse(raw);
              const results = (payload.results || []).map((item) => ({
                id: item.id,
                name: item.name,
                duration: item.duration,
                preview: item.previews?.['preview-hq-mp3'] || item.previews?.['preview-lq-mp3'],
                tags: item.tags?.slice(0, 5) || [],
              }));

              resolve({
                results,
                noApiKey: false,
                invalidKey: false,
                error: null,
              });
            } catch (error) {
              resolve({
                results: [],
                noApiKey: false,
                invalidKey: false,
                error: 'Failed to parse Freesound response.',
              });
            }
          });
        })
        .on('error', (error) => {
          resolve({
            results: [],
            noApiKey: false,
            invalidKey: false,
            error: error.message,
          });
        });
    });
  });

  ipcMain.handle('freesound:download', async (_, { previewUrl, filenameBase, operationId }) => {
    const uniqueFilename = getUniqueFilenameForImport(`${sanitizeFilenameBase(filenameBase)}.mp3`);
    const outputPath = path.join(SOUNDS_DIR, uniqueFilename);

    return new Promise((resolve) => {
      let settled = false;

      const finish = (payload) => {
        if (settled) return;
        settled = true;
        resolve(payload);
      };

      const cleanupPartial = () => {
        if (fs.existsSync(outputPath)) {
          try {
            fs.unlinkSync(outputPath);
          } catch (error) {
            console.error('Failed to clean partial Freesound download:', error);
          }
        }
      };

      const request = https.get(previewUrl, (response) => {
        if (response.statusCode !== 200) {
          cleanupPartial();
          finish({
            success: false,
            error: `Freesound returned HTTP ${response.statusCode}.`,
          });
          return;
        }

        const contentLength = Number(response.headers['content-length'] || 0);
        let received = 0;
        const file = fs.createWriteStream(outputPath);

        response.on('data', (chunk) => {
          received += chunk.length;
          if (contentLength > 0) {
            sendDownloadProgress({
              operationId,
              source: 'freesound',
              percent: Number(((received / contentLength) * 100).toFixed(1)),
              status: 'downloading',
              label: `${Math.round((received / contentLength) * 100)}%`,
            });
          }
        });

        response.pipe(file);

        file.on('finish', () => {
          file.close(() => {
            const stats = fs.existsSync(outputPath) ? fs.statSync(outputPath) : null;
            if (!stats || stats.size === 0) {
              cleanupPartial();
              finish({ success: false, error: 'Downloaded Freesound preview was empty.' });
              return;
            }

            sendDownloadProgress({
              operationId,
              source: 'freesound',
              percent: 100,
              status: 'done',
              label: 'Complete',
            });
            finish({
              success: true,
              filename: uniqueFilename,
              displayName: filenameBase,
              sourceType: 'freesound',
              sourceUrl: previewUrl,
            });
          });
        });

        file.on('error', (error) => {
          cleanupPartial();
          finish({ success: false, error: error.message });
        });
      });

      request.on('error', (error) => {
        cleanupPartial();
        sendDownloadProgress({
          operationId,
          source: 'freesound',
          percent: 0,
          status: 'failed',
          label: 'Failed',
        });
        finish({ success: false, error: error.message });
      });

      request.setTimeout(45000, () => {
        request.destroy(new Error('Freesound download timed out.'));
      });
    });
  });
}

function registerHotkeyIpc() {
  ipcMain.handle('hotkeys:register', async (_, payload) => {
    globalShortcut.unregisterAll();

    const enabled = payload?.enabled !== false;
    const entries = Array.isArray(payload?.entries) ? payload.entries : [];

    if (!enabled || entries.length === 0) {
      return { success: true, results: [] };
    }

    const validation = validateHotkeyEntries(entries);
    if (!validation.valid) {
      return {
        success: false,
        conflicts: validation.conflicts,
        results: [],
      };
    }

    const results = entries.map((entry) => {
      const registered = globalShortcut.register(entry.accelerator, () => {
        mainWindow?.webContents.send('hotkey:trigger', entry);
      });

      return {
        accelerator: entry.accelerator,
        type: entry.type,
        boardId: entry.boardId,
        cellIndex: entry.cellIndex ?? null,
        success: registered,
      };
    });

    return {
      success: results.every((result) => result.success),
      results,
    };
  });

  ipcMain.handle('hotkeys:unregisterAll', async () => {
    globalShortcut.unregisterAll();
    return { success: true };
  });
}

app.whenReady().then(() => {
  initializeDataPaths();
  ensureDataFiles();

  registerWindowIpc();
  registerStateIpc();
  registerImportIpc();
  registerYoutubeIpc();
  registerFreesoundIpc();
  registerHotkeyIpc();

  createWindow();
});

app.on('window-all-closed', () => {
  globalShortcut.unregisterAll();
  app.quit();
});
