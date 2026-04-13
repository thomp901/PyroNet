/**
 * PyroNet Sensor Node Simulator
 * 
 * This script simulates what your Wi-SUN gateway would send to the CSP.
 * Run it while the server is running to test real-time updates.
 * 
 * Usage: node simulate.js
 */

const http = require('http');

const API_BASE = 'http://localhost:3000/api';

// Simulated devices around Purdue campus
const devices = [
    { id: 'Node-Alpha', lat: 40.4237, lng: -86.9212 },
    { id: 'Node-Beta', lat: 40.4280, lng: -86.9150 },
    { id: 'Node-Gamma', lat: 40.4200, lng: -86.9280 },
    { id: 'Node-Delta', lat: 40.4260, lng: -86.9240 }
];

// Helper to make HTTP requests
function post(endpoint, data) {
    return new Promise((resolve, reject) => {
        const url = new URL(API_BASE + endpoint);
        const postData = JSON.stringify(data);
        
        const options = {
            hostname: url.hostname,
            port: url.port,
            path: url.pathname,
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Content-Length': Buffer.byteLength(postData)
            }
        };

        const req = http.request(options, (res) => {
            let body = '';
            res.on('data', chunk => body += chunk);
            res.on('end', () => {
                try {
                    resolve(JSON.parse(body));
                } catch {
                    resolve(body);
                }
            });
        });

        req.on('error', reject);
        req.write(postData);
        req.end();
    });
}

// Generate random sensor values
function generateSensorData(baseRisk = 0) {
    // Add some randomness to simulate real sensor fluctuations
    const riskVariance = Math.random() > 0.9 ? 1 : 0; // 10% chance of risk increase
    
    return {
        temp: 20 + Math.random() * 15, // 20-35°C
        humidity: 30 + Math.random() * 40, // 30-70%
        gas_level: 400 + Math.random() * (baseRisk > 1 ? 800 : 200), // VOC ppm
        particulates: 10 + Math.random() * (baseRisk > 1 ? 150 : 40), // µg/m³
        risk_level: Math.min(3, baseRisk + riskVariance),
        battery: Math.floor(50 + Math.random() * 50) // 50-100%
    };
}

// Register all devices
async function registerDevices() {
    console.log('Registering devices...\n');
    
    for (const device of devices) {
        try {
            const result = await post('/register', device);
            console.log(`  ✓ ${device.id} registered at (${device.lat}, ${device.lng})`);
        } catch (err) {
            console.log(`  ✗ Failed to register ${device.id}: ${err.message}`);
        }
    }
    
    console.log('');
}

// Send periodic sensor data
async function sendSensorData() {
    console.log('Sending sensor data...\n');
    
    for (const device of devices) {
        // Vary base risk level by device for demo purposes
        let baseRisk = 0;
        if (device.id === 'Node-Gamma') baseRisk = 2; // This one shows elevated risk
        if (device.id === 'Node-Delta') baseRisk = 1;
        
        const data = {
            device_id: device.id,
            ...generateSensorData(baseRisk)
        };
        
        try {
            await post('/sensor-data', data);
            console.log(`  📤 ${device.id}: risk=${data.risk_level}, temp=${data.temp.toFixed(1)}°C, VOC=${data.gas_level.toFixed(0)}ppm`);
        } catch (err) {
            console.log(`  ✗ Failed to send data for ${device.id}: ${err.message}`);
        }
    }
    
    console.log('');
}

// Simulate a critical alert scenario
async function simulateCriticalEvent() {
    console.log('🚨 Simulating critical fire detection event...\n');
    
    const criticalData = {
        device_id: 'Node-Gamma',
        temp: 45.5,
        humidity: 15,
        gas_level: 1500, // High VOC
        particulates: 180, // High PM
        risk_level: 3,
        battery: 72
    };
    
    try {
        await post('/sensor-data', criticalData);
        console.log('   Critical event sent from Node-Gamma');
        console.log('     Check the dashboard - you should see a critical alert!\n');
    } catch (err) {
        console.log(`  ✗ Failed: ${err.message}`);
    }
}

// Main simulation loop
async function runSimulation() {
    console.log('═══════════════════════════════════════════');
    console.log('  PyroNet Sensor Node Simulator');
    console.log('═══════════════════════════════════════════\n');
    console.log('Make sure the server is running: npm start\n');
    
    // Step 1: Register devices
    await registerDevices();
    
    // Wait a moment
    await new Promise(r => setTimeout(r, 1000));
    
    // Step 2: Send initial sensor data
    await sendSensorData();
    
    // Step 3: Start periodic updates
    console.log('⏱  Starting periodic updates (every 5 seconds)...');
    console.log('   Press Ctrl+C to stop\n');
    
    let iteration = 0;
    const interval = setInterval(async () => {
        iteration++;
        console.log(`--- Update #${iteration} ---`);
        await sendSensorData();
        
        // Every 4th iteration, simulate a critical event
        if (iteration % 4 === 0) {
            await simulateCriticalEvent();
        }
    }, 5000);
    
    // Handle graceful shutdown
    process.on('SIGINT', () => {
        console.log('\n\n Simulation stopped');
        clearInterval(interval);
        process.exit(0);
    });
}

// Run it
runSimulation().catch(console.error);
