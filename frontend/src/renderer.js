const boardElement = document.getElementById('board');
const statusElement = document.getElementById('status');
const navItems = document.querySelectorAll('.nav-item');
const sections = document.querySelectorAll('.view-section');
const chatMessages = document.getElementById('chat-messages');
const chatInput = document.querySelector('.chat-input input');
const chatBtn = document.querySelector('.chat-input button');

// Configuration du plateau initial
const INITIAL_BOARD = [
  ['black_rook', 'black_knight', 'black_bishop', 'black_queen', 'black_king', 'black_bishop', 'black_knight', 'black_rook'],
  ['black_pawn', 'black_pawn', 'black_pawn', 'black_pawn', 'black_pawn', 'black_pawn', 'black_pawn', 'black_pawn'],
  [null, null, null, null, null, null, null, null],
  [null, null, null, null, null, null, null, null],
  [null, null, null, null, null, null, null, null],
  [null, null, null, null, null, null, null, null],
  ['white_pawn', 'white_pawn', 'white_pawn', 'white_pawn', 'white_pawn', 'white_pawn', 'white_pawn', 'white_pawn'],
  ['white_rook', 'white_knight', 'white_bishop', 'white_queen', 'white_king', 'white_bishop', 'white_knight', 'white_rook']
];

const ASSETS_PATH = '../../assets/img/';

// --- Gestion de l'IPC (Réception) ---
if (window.electronAPI) {
  window.electronAPI.onGameUpdate((update) => {
    console.log('Mise à jour du jeu reçue:', update);
    // On pourrait ici mettre à jour graphiquement le plateau
    statusElement.innerText = `L'adversaire a joué : ${update.move}`;
  });

  window.electronAPI.onChatMessage((msg) => {
    const div = document.createElement('div');
    div.className = 'msg';
    div.innerText = `${msg.user}: ${msg.text}`;
    chatMessages.appendChild(div);
    chatMessages.scrollTop = chatMessages.scrollHeight;
  });

  window.electronAPI.onGameStatus((status) => {
    statusElement.innerText = status;
  });
}

// Gestion de la navigation
function switchView(targetId) {
  sections.forEach(section => {
    section.classList.toggle('active', section.id === targetId);
  });
  
  navItems.forEach(item => {
    item.classList.toggle('active', item.dataset.target === targetId);
  });

  if (targetId === 'game-section') {
    createBoard();
    if (window.electronAPI) {
      window.electronAPI.joinGame('standard-match');
    }
  }
}

window.switchView = switchView;

navItems.forEach(item => {
  item.addEventListener('click', () => {
    switchView(item.dataset.target);
  });
});

// Chat Logic
function sendChatMessage() {
  const text = chatInput.value.trim();
  if (text && window.electronAPI) {
    window.electronAPI.sendMessage(text);
    chatInput.value = '';
  }
}

chatBtn.addEventListener('click', sendChatMessage);
chatInput.addEventListener('keypress', (e) => {
  if (e.key === 'Enter') sendChatMessage();
});

function createBoard() {
  if (!boardElement) return;
  boardElement.innerHTML = '';
  
  for (let r = 0; r < 8; r++) {
    for (let c = 0; c < 8; c++) {
      const square = document.createElement('div');
      const isLight = (r + c) % 2 === 0;
      square.className = `square ${isLight ? 'light' : 'dark'}`;
      square.dataset.rank = 8 - r;
      square.dataset.file = String.fromCharCode(97 + c);

      const pieceName = INITIAL_BOARD[r][c];
      if (pieceName) {
        const img = document.createElement('img');
        img.src = `${ASSETS_PATH}${pieceName}.png`;
        img.className = 'piece';
        img.draggable = true;
        
        img.addEventListener('dragstart', (e) => {
          e.dataTransfer.setData('text/plain', JSON.stringify({
            piece: pieceName,
            fromRank: square.dataset.rank,
            fromFile: square.dataset.file
          }));
          setTimeout(() => { img.style.opacity = '0.5'; }, 0);
        });

        img.addEventListener('dragend', () => {
          img.style.opacity = '1';
        });

        square.appendChild(img);
      }

      square.addEventListener('dragover', (e) => {
        e.preventDefault();
      });

      square.addEventListener('drop', (e) => {
        e.preventDefault();
        const data = JSON.parse(e.dataTransfer.getData('text/plain'));
        const piece = document.querySelector(`img[src*="${data.piece}"][style*="opacity: 0.5"]`);
        if (piece) {
          if (square.firstChild && square.firstChild.classList.contains('piece')) {
            square.removeChild(square.firstChild);
          }
          square.appendChild(piece);
          
          const moveData = {
            from: `${data.fromFile}${data.fromRank}`,
            to: `${square.dataset.file}${square.dataset.rank}`,
            piece: data.piece
          };

          statusElement.innerText = `Déplacement : ${moveData.from} -> ${moveData.to}`;
          
          // Envoi au Main via IPC
          if (window.electronAPI) {
            window.electronAPI.sendMove(moveData);
          }
        }
      });

      if (c === 0) {
        const rankLabel = document.createElement('span');
        rankLabel.className = 'coordinates coord-rank';
        rankLabel.innerText = 8 - r;
        square.appendChild(rankLabel);
      }
      if (r === 7) {
        const fileLabel = document.createElement('span');
        fileLabel.className = 'coordinates coord-file';
        fileLabel.innerText = String.fromCharCode(97 + c);
        square.appendChild(fileLabel);
      }

      boardElement.appendChild(square);
    }
  }
}

document.addEventListener('DOMContentLoaded', async () => {
  switchView('home-section');
  
  if (window.electronAPI) {
    const initialState = await window.electronAPI.getInitialState();
    console.log('État initial récupéré:', initialState);
  }
});