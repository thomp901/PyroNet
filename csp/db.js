// const sqlite3 = require('sqlite3').verbose();
// const db = new sqlite3.Database('./pyronet.db');

// // Step 2: Define your table schema based on PyroNet sensor metrics
// db.serialize(() => {
//   db.run(`CREATE TABLE IF NOT EXISTS sensor_logs (
//     id INTEGER PRIMARY KEY AUTOINCREMENT,
//     device_id TEXT NOT NULL,       -- From requirement 6-1-1
//     timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, 
//     temp REAL,                     -- From BME 688
//     humidity REAL,                 -- From BME 688
//     gas_level REAL,                -- From BME 688 (VOC)
//     particulates REAL,             -- From SPS30
//     risk_level INTEGER,             -- Calculated local risk (0-3)
//     battery_health INTEGER         -- From requirement 6-1-5
//   )`);
//   console.log("PyroNet database 'pyronet.db' initialized.");
// });

// module.exports = db;

const { Pool } = require('pg');

const pool = new Pool({
  host: 'localhost',
  user: 'pyronet_user',
  password: 'strongpassword',
  database: 'pyronet',
  port: 5432,
});

module.exports = pool;