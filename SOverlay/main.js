const { app, BrowserWindow, screen } = require('electron');

let overlayWindow;

function createOverlay() {
  const primaryDisplay = screen.getPrimaryDisplay();
  const { width, height } = primaryDisplay.workAreaSize;

  overlayWindow = new BrowserWindow({
    width: width,
    height: height,
    transparent: true,
    frame: false,
    alwaysOnTop: true,
    skipTaskbar: true,
    hasShadow: false,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false 
    }
  });

  overlayWindow.setIgnoreMouseEvents(true);
  overlayWindow.maximize();
  overlayWindow.loadFile('SOverlay/index.html');
  overlayWindow.webContents.openDevTools({ mode: 'detach' });
  console.log('Overlay window created and loaded.');
}

app.whenReady().then(createOverlay);