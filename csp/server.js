// const db = require('./db');
// const express = require('express');
// const http = require('http');
// const WebSocket = require('ws');
// const fs = require('fs');
// const path = require('path');

// const app = express();
// const port = 3000;

// // Create HTTP server and WebSocket server
// const server = http.createServer(app);
// const wss = new WebSocket.Server({ server });

// // ============================================
// // Simple JSON File Database
// // ============================================
// const DATA_FILE = './pyronet_data.json';

// // ============================================
// // Middleware
// // ============================================
// app.use(express.json());
// app.use(express.static(path.join(__dirname, 'public')));

// // ============================================
// // WebSocket - Broadcast to all connected clients
// // ============================================
// function broadcast(type, payload) {
//     const message = JSON.stringify({ type, data: payload });
//     wss.clients.forEach(client => {
//         if (client.readyState === WebSocket.OPEN) {
//             client.send(message);
//         }
//     });
// }

// wss.on('connection', (ws) => {
//     console.log(' Dashboard client connected');
    
//     ws.on('close', () => {
//         console.log(' Dashboard client disconnected');
//     });
// });

// // ============================================
// // Helper: Add alert and broadcast
// // ============================================
// function addAlert(type, title, details, deviceId) {
//     const alert = {
//         id: Date.now(),
//         type,
//         title,
//         details,
//         device_id: deviceId,
//         timestamp: new Date().toISOString()
//     };
    
//     data.alerts.unshift(alert);
    
//     // Keep only last 500 alerts
//     if (data.alerts.length > 500) {
//         data.alerts = data.alerts.slice(0, 500);
//     }
    
//     broadcast('new_alert', alert);
    
//     return alert;
// }

// // ============================================
// // API Routes
// // ============================================

// // GET /api/devices - Get all registered devices
// app.get('/api/devices', (req, res) => {
//     const deviceList = Object.values(data.devices);
//     res.json(deviceList);
// });

// // GET /api/device/:id - Get single device
// app.get('/api/device/:id', (req, res) => {
//     const device = data.devices[req.params.id];
//     if (!device) {
//         return res.status(404).json({ error: 'Device not found' });
//     }
//     res.json(device);
// });

// // POST /api/register - Register or update a device
//     app.post('/api/register', async (req, res) => {
//         const { id, lat, lng } = req.body;
    
//         if (!id || lat === undefined || lng === undefined) {
//         return res.status(400).json({
//             error: 'Missing required fields: id, lat, lng'
//         });
//         }
    
//         try {
//         // Insert or update device (current state)
//         await db.query(
//             `
//             INSERT INTO devices (id, lat, lng)
//             VALUES ($1, $2, $3)
//             ON CONFLICT (id)
//             DO UPDATE SET
//             lat = EXCLUDED.lat,
//             lng = EXCLUDED.lng,
//             last_seen = NOW()
//             `,
//             [id, lat, lng]
//         );
    
//         // Fetch updated device
//         const { rows } = await db.query(
//             'SELECT * FROM devices WHERE id = $1',
//             [id]
//         );
    
//         const device = rows[0];
    
//         // Broadcast to dashboard
//         broadcast('device_update', device);
    
//         res.json({
//             success: true,
//             device
//         });
//         } catch (err) {
//         console.error('Register device failed:', err);
//         res.status(500).json({ error: 'Database error' });
//         }
//     });
        


// // POST /api/sensor-data - Receive sensor readings from a node
// app.post('/api/sensor-data', (req, res) => {
//     const { device_id, temp, humidity, gas_level, particulates, risk_level, battery } = req.body;
    
//     if (!device_id) {
//         return res.status(400).json({ error: 'Missing device_id' });
//     }

//     // Update device state
//     if (temp !== undefined) device.temp = temp;
//     if (humidity !== undefined) device.humidity = humidity;
//     if (gas_level !== undefined) device.gas_level = gas_level;
//     if (particulates !== undefined) device.particulates = particulates;
//     if (risk_level !== undefined) device.risk_level = risk_level;
//     if (battery !== undefined) device.battery = battery;
//     device.last_seen = new Date().toISOString();

//     // Log sensor reading
//     data.sensor_logs.push({
//         device_id,
//         timestamp: new Date().toISOString(),
//         temp, humidity, gas_level, particulates, risk_level, battery
//     });
    
//     // Keep only last 24 hours of logs (approx 1000 entries per device)
//     const cutoff = Date.now() - (24 * 60 * 60 * 1000);
//     data.sensor_logs = data.sensor_logs.filter(log => 
//         new Date(log.timestamp).getTime() > cutoff
//     );


//     console.log(`📊 Data from ${device_id}: risk=${risk_level}, temp=${temp?.toFixed(1)}, humidity=${humidity?.toFixed(1)}`);

//     // Check for critical alert conditions
//     if (risk_level >= 3 || (gas_level > 1000 && particulates > 100)) {
//         addAlert('critical', 'Critical Fire Risk Detected', 
//             `High VOC (${gas_level?.toFixed(0)}) and particulates (${particulates?.toFixed(0)}) detected`, device_id);
//     } else if (risk_level === 2 && device.risk_level < 2) {
//         addAlert('warning', 'Elevated Risk Level', 
//             `Risk level increased to ${risk_level}`, device_id);
//     }

//     // Broadcast device update
//     broadcast('device_update', device);

//     res.json({ success: true, message: 'Data logged' });
// });

// // POST /api/alert - Manually create an alert
// app.post('/api/alert', (req, res) => {
//     const { type, title, details, device_id } = req.body;
    
//     if (!type || !title) {
//         return res.status(400).json({ error: 'Missing required fields: type, title' });
//     }

//     const alert = addAlert(type, title, details || '', device_id || 'System');
//     res.json({ success: true, alert });
// });

// // GET /api/alerts - Get alert history
// app.get('/api/alerts', (req, res) => {
//     const limit = parseInt(req.query.limit) || 100;
//     res.json(data.alerts.slice(0, limit));
// });

// // DELETE /api/alerts - Clear all alerts
// app.delete('/api/alerts', (req, res) => {
//     data.alerts = [];
//     broadcast('alerts_cleared', {});
//     res.json({ success: true, message: 'All alerts cleared' });
// });

// // GET /api/history/:device_id - Get sensor history for a device
// app.get('/api/history/:device_id', (req, res) => {
//     const deviceLogs = data.sensor_logs.filter(log => log.device_id === req.params.device_id);
//     res.json(deviceLogs);
// });

// // DELETE /api/device/:id - Remove a device
// app.delete('/api/device/:id', (req, res) => {
//     const deviceId = req.params.id;
    
//     if (data.devices[deviceId]) {
//         delete data.devices[deviceId];
//         addAlert('warning', 'Device Removed', `${deviceId} was removed from the network`, deviceId);
//         broadcast('device_removed', { id: deviceId });
//     }
    
//     res.json({ success: true, message: `Device ${deviceId} removed` });
// });

// // Root route
// app.get('/', (req, res) => {
//     res.sendFile(path.join(__dirname, 'public', 'index.html'));
// });

// // ============================================
// // Start Server
// // ============================================
// server.listen(3000, '0.0.0.0', () => {
//     console.log('PyroNet CSP is Live!');
//     console.log('Dashboard: http://0.0.0.0:3000');
//     console.log('API Base:  http://0.0.0.0:3000/api');
//     console.log('WebSocket: ws://0.0.0.0:3000');
// });

const db = require('./db');
const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const app = express();
const port = 3000;

// HTTP + WebSocket
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// --------------------
// Middleware
// --------------------
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// --------------------
// WebSocket broadcast
// --------------------
function broadcast(type, payload) {
  const message = JSON.stringify({ type, data: payload });
  wss.clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(message);
    }
  });
}

wss.on('connection', () => {
  console.log('Dashboard client connected');
});

// --------------------
// API ROUTES
// --------------------

// ✅ REGISTER DEVICE (Postgres)
app.post('/api/register', async (req, res) => {
  const { id, lat, lng } = req.body;

  if (!id || lat === undefined || lng === undefined) {
    return res.status(400).json({ error: 'Missing id, lat, or lng' });
  }

  try {
    await db.query(
      `
      INSERT INTO devices (id, lat, lng)
      VALUES ($1, $2, $3)
      ON CONFLICT (id)
      DO UPDATE SET
        lat = EXCLUDED.lat,
        lng = EXCLUDED.lng,
        last_seen = NOW()
      `,
      [id, lat, lng]
    );

    const { rows } = await db.query(
      'SELECT * FROM devices WHERE id = $1',
      [id]
    );

    const device = rows[0];

    broadcast('device_update', device);

    res.json({ success: true, device });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'DB error' });
  }
});

// ✅ GET ALL DEVICES (Postgres)
app.get('/api/devices', async (req, res) => {
  try {
    const { rows } = await db.query('SELECT * FROM devices');
    res.json(rows);
  } catch (err) {
    res.status(500).json({ error: 'DB error' });
  }
});

// SENSOR DATA — NOT MIGRATED YET
app.post('/api/sensor-data', (req, res) => {
  return res.status(501).json({
    error: 'sensor-data not migrated to Postgres yet'
  });
});

// ALERTS — NOT MIGRATED YET
app.get('/api/alerts', (req, res) => {
  return res.status(501).json({
    error: 'alerts not migrated to Postgres yet'
  });
});

// --------------------
// Root
// --------------------
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// --------------------
// Start server
// --------------------
server.listen(port, '0.0.0.0', () => {
  console.log(`PyroNet CSP running`);
  console.log(`Dashboard: http://0.0.0.0:${port}`);
});
