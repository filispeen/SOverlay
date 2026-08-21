const net = require('node:net');
const { EventEmitter } = require('node:events');

const LINK_HOST = '127.0.0.1';
const LINK_PORT = 51873;
const RECONNECT_DELAY_MS = 2000;

class LinkClient extends EventEmitter {
  constructor() {
    super();
    this.socket = null;
    this.buffer = '';
    this.connected = false;
    this.stopped = false;
  }

  start() {
    this.stopped = false;
    this._connect();
  }

  stop() {
    this.stopped = true;
    if (this.socket) {
      this.socket.destroy();
      this.socket = null;
    }
  }

  _connect() {
    if (this.stopped) return;

    const socket = net.createConnection({ host: LINK_HOST, port: LINK_PORT });
    this.socket = socket;

    socket.on('connect', () => {
      this.connected = true;
      this.buffer = '';
      this.emit('connected');
      console.log('[link-client] connected to OBS plugin');
    });

    socket.on('data', (chunk) => {
      this.buffer += chunk.toString('utf8');
      let newlineIndex;
      while ((newlineIndex = this.buffer.indexOf('\n')) !== -1) {
        const line = this.buffer.slice(0, newlineIndex);
        this.buffer = this.buffer.slice(newlineIndex + 1);
        if (line.trim().length === 0) continue;
        this._handleLine(line);
      }
    });

    const scheduleReconnect = () => {
      this.connected = false;
      this.socket = null;
      if (this.stopped) return;
      setTimeout(() => this._connect(), RECONNECT_DELAY_MS);
    };

    socket.on('error', (err) => {
      console.log('[link-client] socket error:', err.message);
    });

    socket.on('close', () => {
      this.emit('disconnected');
      scheduleReconnect();
    });
  }

  _handleLine(line) {
    let event;
    try {
      event = JSON.parse(line);
    } catch (err) {
      console.error('[link-client] failed to parse line:', line, err);
      return;
    }
    this.emit('event', event);
  }
}

module.exports = { LinkClient };
