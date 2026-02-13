import Database from "better-sqlite3";
import path from "path";
import { randomUUID } from "crypto";

const DB_PATH = process.env.DB_PATH || path.join(process.cwd(), "data", "plumbr.db");

let db: Database.Database | null = null;

export function getDb(): Database.Database {
    if (!db) {
        // Ensure directory exists
        const dir = path.dirname(DB_PATH);
        const fs = require("fs");
        if (!fs.existsSync(dir)) {
            fs.mkdirSync(dir, { recursive: true });
        }

        db = new Database(DB_PATH);
        db.pragma("journal_mode = WAL");
        db.pragma("foreign_keys = ON");
        initDb(db);
    }
    return db;
}

function initDb(db: Database.Database) {
    db.exec(`
    CREATE TABLE IF NOT EXISTS users (
      id         TEXT PRIMARY KEY,
      email      TEXT UNIQUE NOT NULL,
      password   TEXT NOT NULL,
      name       TEXT,
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS apps (
      id          TEXT PRIMARY KEY,
      user_id     TEXT NOT NULL REFERENCES users(id),
      name        TEXT NOT NULL,
      description TEXT DEFAULT '',
      api_key     TEXT UNIQUE NOT NULL,
      api_secret  TEXT UNIQUE NOT NULL,
      status      TEXT DEFAULT 'active',
      rate_limit  INTEGER DEFAULT 1000,
      created_at  TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS api_usage (
      id        TEXT PRIMARY KEY,
      app_id    TEXT NOT NULL REFERENCES apps(id),
      endpoint  TEXT NOT NULL,
      lines     INTEGER DEFAULT 0,
      timestamp TEXT DEFAULT (datetime('now'))
    );

    CREATE INDEX IF NOT EXISTS idx_apps_user_id ON apps(user_id);
    CREATE INDEX IF NOT EXISTS idx_apps_api_key ON apps(api_key);
    CREATE INDEX IF NOT EXISTS idx_usage_app_id ON api_usage(app_id);
  `);
}

export function generateApiKey(): string {
    const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    let key = "plumbr_live_";
    for (let i = 0; i < 32; i++) {
        key += chars.charAt(Math.floor(Math.random() * chars.length));
    }
    return key;
}

export function generateApiSecret(): string {
    const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    let secret = "plumbr_secret_";
    for (let i = 0; i < 40; i++) {
        secret += chars.charAt(Math.floor(Math.random() * chars.length));
    }
    return secret;
}
