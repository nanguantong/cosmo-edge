// logger.js — Minimal console logger with timestamps and a verbose toggle.
//
// Levels: info / warn / error always shown; debug only when verbose.

const LEVELS = { error: 0, warn: 1, info: 2, debug: 3 };

export class Logger {
  constructor({ verbose = false } = {}) {
    this.level = verbose ? LEVELS.debug : LEVELS.info;
  }

  _fmt(level, msg) {
    return `[${new Date().toISOString()}] [${level.toUpperCase()}] ${msg}`;
  }

  debug(msg) { if (this.level >= LEVELS.debug) console.log(this._fmt('debug', msg)); }
  info(msg) { if (this.level >= LEVELS.info) console.log(this._fmt('info', msg)); }
  warn(msg) { if (this.level >= LEVELS.warn) console.warn(this._fmt('warn', msg)); }
  error(msg) { if (this.level >= LEVELS.error) console.error(this._fmt('error', msg)); }
}
