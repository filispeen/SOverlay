const { app, BrowserWindow, screen, globalShortcut, ipcMain } = require('electron');
const remote = require('@electron/remote/main');
const path = require('node:path');
const fs = require('fs');
require('v8-compile-cache');

// Handle creating/removing shortcuts on Windows when installing/uninstalling.
if (require('electron-squirrel-startup')) {
  app.quit();
}


const createWindow = () => {
  const { width, height } = screen.getPrimaryDisplay().workAreaSize;

  const mainWindow = new BrowserWindow({
    width: 363,
    height: 631,
    maxWidth: 363, minWidth: 363,
    maxHeight: 631, minHeight: 631,
    x: 1548, y: 200,
    webPreferences: {
      //preload: path.join(__dirname, 'preload.js'),
      contextIsolation: false,  // Required for iframe/webview
      nodeIntegration: true
    },
    frame: false,
    autoHideMenuBar: true,
    transparent: true,
    alwaysOnTop: true,
    skipTaskbar: true
  });
  const editMessageWindow = new BrowserWindow({ width: width, height: height, maxWidth: width, minWidth: width, maxHeight: height, minHeight: height, frame: false, resizable: false, autoHideMenuBar: true, transparent: true, alwaysOnTop: true, skipTaskbar: true, webPreferences: { preload: __dirname + "\\edit_preload.js"}})

  editMessageWindow.setOpacity(0);
  //mainWindow.loadFile(path.join(__dirname, 'index.html')); //Youtube chat and almost everything doesn`t work.
  mainWindow.loadURL("https://youtube.com/live_chat?v=jnBzgExtFB8"); //Using this method everything fucking works.
  mainWindow.setIgnoreMouseEvents(true, { forward: true })
  editMessageWindow.loadFile(path.join(__dirname, "edit_message.html"))
  editMessageWindow.setIgnoreMouseEvents(true, { forward: true })

  mainWindow.webContents.on('did-finish-load', () => {
    const cssPath = path.join(__dirname, 'chat.css');
    fs.readFile(cssPath, 'utf8', (err, data) => {
        if (err) {
            console.error('Ошибка чтения файла CSS:', err);
            return;
        }
        mainWindow.webContents.insertCSS(data).then(() => {
            console.log('Кастомный CSS успешно добавлен.');
        }).catch((error) => {
            console.error('Ошибка при добавлении CSS:', error);
        });
    });
  });

  const win = BrowserWindow.getFocusedWindow();
  win.setIgnoreMouseEvents(true, { forward: true });

  var edit = false;
  globalShortcut.register('Alt+CommandOrControl+O', () => {
    console.log('Toggling edit mode.');
    if (!edit) {
      mainWindow.setSkipTaskbar(false);
      editMessageWindow.setOpacity(1);
      edit = true;
    } else {
      mainWindow.setSkipTaskbar(true);
      win.setIgnoreMouseEvents(true, { forward: true });
      editMessageWindow.setOpacity(0);
      edit = false;
    }
  })
  //win.setIgnoreMouseEvents(true);

  ipcMain.on("close-app", () => app.quit());
  ipcMain.on("forward_false", () => editMessageWindow.setIgnoreMouseEvents(false, { forward: false }));
  ipcMain.on("forward_true", () => editMessageWindow.setIgnoreMouseEvents(true, { forward: true }));
};

app.whenReady().then(() => {
  createWindow();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});