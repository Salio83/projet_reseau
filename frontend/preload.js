const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  // Envoi de messages (Renderer -> Main)
  sendMove: (move) => ipcRenderer.send('game:move', move),
  sendMessage: (message) => ipcRenderer.send('chat:message', message),
  joinGame: (gameId) => ipcRenderer.send('game:join', gameId),

  // Réception de messages (Main -> Renderer)
  onGameUpdate: (callback) => ipcRenderer.on('game:update', (_event, value) => callback(value)),
  onChatMessage: (callback) => ipcRenderer.on('chat:receive-message', (_event, value) => callback(value)),
  onGameStatus: (callback) => ipcRenderer.on('game:status', (_event, value) => callback(value)),

  // Invoke/Handle (Asynchrone avec retour)
  getInitialState: () => ipcRenderer.invoke('game:get-initial-state')
});