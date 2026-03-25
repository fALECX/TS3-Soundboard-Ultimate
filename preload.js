const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
  minimize: () => ipcRenderer.send('window:minimize'),
  maximize: () => ipcRenderer.send('window:maximize'),
  close: () => ipcRenderer.send('window:close'),

  openExternal: (url) => ipcRenderer.invoke('shell:openExternal', url),
  getSystemStatus: () => ipcRenderer.invoke('system:getStatus'),

  loadState: () => ipcRenderer.invoke('state:load'),
  saveState: (payload) => ipcRenderer.invoke('state:save', payload),

  importFiles: () => ipcRenderer.invoke('files:import'),
  getSoundsDir: () => ipcRenderer.invoke('sounds:getDir'),
  getSoundFileUrl: (filename) => ipcRenderer.invoke('sounds:getFileUrl', filename),
  deleteSoundFile: (filename) => ipcRenderer.invoke('sound:deleteFile', filename),

  youtubeSearch: (query) => ipcRenderer.invoke('youtube:search', query),
  youtubeDownload: (payload) => ipcRenderer.invoke('youtube:download', payload),
  youtubeGetStreamUrl: (url) => ipcRenderer.invoke('youtube:getStreamUrl', url),

  freesoundSearch: (query) => ipcRenderer.invoke('freesound:search', query),
  freesoundDownload: (payload) => ipcRenderer.invoke('freesound:download', payload),

  registerHotkeys: (payload) => ipcRenderer.invoke('hotkeys:register', payload),
  unregisterHotkeys: () => ipcRenderer.invoke('hotkeys:unregisterAll'),

  onHotkeyTrigger: (callback) =>
    ipcRenderer.on('hotkey:trigger', (_, payload) => callback(payload)),
  onDownloadProgress: (callback) =>
    ipcRenderer.on('download:progress', (_, payload) => callback(payload)),
});
