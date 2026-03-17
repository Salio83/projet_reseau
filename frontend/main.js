const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');

function createWindow() {
  const win = new BrowserWindow({
    width: 1000,
    height: 800,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.js')
    }
  });

  win.loadFile(path.join(__dirname, 'src/index.html'));
  
  // win.webContents.openDevTools();

  // Gestion de l'IPC (Inter-Process Communication)
  ipcMain.on('game:move', (event, move) => {
    console.log('Coup reçu du renderer:', move);
    // Ici, on pourrait envoyer le coup à un serveur via Socket.io ou TCP
    // Simulation d'une réponse de l'adversaire (exemple)
    /*
    setTimeout(() => {
      win.webContents.send('game:update', { move: 'e7e5', turn: 'black' });
    }, 1000);
    */
  });

  ipcMain.on('chat:message', (event, message) => {
    console.log('Message chat reçu:', message);
    // Diffusion du message à tous les clients connectés via le serveur
    // Simulation d'une réception
    win.webContents.send('chat:receive-message', { user: 'Adversaire', text: message });
  });

  ipcMain.on('game:join', (event, gameId) => {
    console.log(`Tentative de rejoindre la partie: ${gameId}`);
    win.webContents.send('game:status', `Connecté à la partie ${gameId}`);
  });

  ipcMain.handle('game:get-initial-state', async () => {
    return {
      player: 'white',
      status: 'ready'
    };
  });
}

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