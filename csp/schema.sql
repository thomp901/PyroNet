CREATE TABLE devices (
  id TEXT PRIMARY KEY,
  lat DOUBLE PRECISION,
  lng DOUBLE PRECISION,
  last_seen TIMESTAMP DEFAULT NOW()
);

CREATE TABLE sensor_data (
  id SERIAL PRIMARY KEY,
  device_id TEXT REFERENCES devices(id),
  temp REAL,
  humidity REAL,
  gas_level REAL,
  particulates REAL,
  risk_level INT,
  battery INT,
  timestamp TIMESTAMP DEFAULT NOW()
);

CREATE TABLE alerts (
  id SERIAL PRIMARY KEY,
  type TEXT,
  title TEXT,
  device_id TEXT,
  timestamp TIMESTAMP DEFAULT NOW()
);

