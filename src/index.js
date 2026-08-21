const { app, BrowserWindow, screen, globalShortcut, ipcMain } = require('electron');
const path = require('node:path');
require('v8-compile-cache');

const { LinkClient } = require('./link-client');
const { WindowManager } = require('./window-manager');

if (require('electron-squirrel-startup')) {
  app.quit();
}

const windowManager = new WindowManager();
const linkClient = new LinkClient();

let editMessageWindow = null;
let editModeActive = false;
let editTargetName = null;

const createEditWindow = () => {
  const { width, height } = screen.getPrimaryDisplay().workAreaSize;

  editMessageWindow = new BrowserWindow({
    width, height,
    maxWidth: width, minWidth: width,
    maxHeight: height, minHeight: height,
    frame: false,
    resizable: false,
    autoHideMenuBar: true,
    transparent: true,
    alwaysOnTop: true,
    skipTaskbar: true,
    webPreferences: { preload: path.join(__dirname, 'edit_preload.js') },
  });

  editMessageWindow.setOpacity(0);
  editMessageWindow.loadFile(path.join(__dirname, 'edit_message.html'));
  editMessageWindow.setIgnoreMouseEvents(true, { forward: true });
};

const toggleEditMode = () => {
  if (!editMessageWindow) return;

  if (!editModeActive) {
    const names = windowManager.getAllNames();
    if (names.length === 0) {
      console.log('[edit-mode] no windows available yet, plugin may not be connected');
      return;
    }
    editTargetName = names[0];
    console.log(`[edit-mode] editing "${editTargetName}"`);
    editMessageWindow.setOpacity(1);
    editModeActive = true;
  } else {
    editMessageWindow.setIgnoreMouseEvents(true, { forward: true });
    editMessageWindow.setOpacity(0);
    editModeActive = false;
    editTargetName = null;
  }
};

const setupIpc = () => {
  ipcMain.on('close-app', () => app.quit());
  ipcMain.on('forward_false', () => editMessageWindow.setIgnoreMouseEvents(false, { forward: false }));
  ipcMain.on('forward_true', () => editMessageWindow.setIgnoreMouseEvents(true, { forward: true }));

  ipcMain.handle('get_window_props', () => {
    if (!editTargetName) return { width: 0, height: 0, x: 0, y: 0 };
    const win = windowManager.getWindow(editTargetName);
    if (!win || win.isDestroyed()) return { width: 0, height: 0, x: 0, y: 0 };
    const { width, height, x, y } = win.getBounds();
    return { width, height, x, y };
  });

  ipcMain.on('chat_props', (event, chatProps) => {
    if (!editTargetName) return;
    const win = windowManager.getWindow(editTargetName);
    if (!win || win.isDestroyed()) return;
    win.setPosition(chatProps.x, chatProps.y);
    win.setSize(chatProps.width, chatProps.height);
  });
};

const setupLinkClient = () => {
  linkClient.on('connected', () => {
    console.log('[main] link to OBS plugin established');
  });

  linkClient.on('disconnected', () => {
    console.log('[main] link to OBS plugin lost, retrying...');
  });

  linkClient.on('event', (event) => {
    if (!event || !event.type || !event.source) return;

    switch (event.type) {
      case 'transform':
        windowManager.handleTransform(event.source, event);
        break;
      case 'show_onscreen':
        windowManager.handleShowOnscreen(event.source, event.value);
        break;
      case 'enabled':
        windowManager.handleEnabled(event.source, event.value);
        break;
      case 'removed':
        windowManager.handleRemoved(event.source);
        break;
      default:
        console.log('[main] unknown event type:', event.type);
    }
  });

  linkClient.start();
};

app.whenReady().then(() => {
  createEditWindow();
  setupIpc();
  setupLinkClient();

  globalShortcut.register('Alt+CommandOrControl+O', toggleEditMode);

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createEditWindow();
    }
  });
});

app.on('window-all-closed', () => {
  linkClient.stop();
  windowManager.closeAll();
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
