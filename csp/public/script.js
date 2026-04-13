// ============================================
// PyroNet CSP - Dashboard Frontend
// ============================================

// Global state
let pyroMap = null;
let devices = new Map(); // Store device markers by ID
let alerts = []; // Alert history cache
let ws = null; // WebSocket connection

// ============================================
// WebSocket Connection
// ============================================
function initWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}`;
    
    ws = new WebSocket(wsUrl);
    
    ws.onopen = () => {
        console.log(' Connected to PyroNet server');
        updateStatus('Connected');
    };
    
    ws.onclose = () => {
        console.log('🔌 Disconnected from server, reconnecting...');
        updateStatus('Reconnecting...');
        // Attempt to reconnect after 3 seconds
        setTimeout(initWebSocket, 3000);
    };
    
    ws.onerror = (error) => {
        console.error('WebSocket error:', error);
        updateStatus('Connection Error');
    };
    
    ws.onmessage = (event) => {
        try {
            const message = JSON.parse(event.data);
            handleWebSocketMessage(message);
        } catch (err) {
            console.error('Failed to parse WebSocket message:', err);
        }
    };
}

function handleWebSocketMessage(message) {
    const { type, data } = message;
    
    switch (type) {
        case 'device_update':
            console.log(' Device update:', data.id);
            updateDeviceOnMap(data);
            break;
            
        case 'device_removed':
            console.log(' Device removed:', data.id);
            removeDeviceFromMap(data.id);
            break;
            
        case 'new_alert':
            console.log(' New alert:', data.title);
            addAlertToList(data);
            // Flash the Alert History button if not on that view
            flashAlertButton();
            break;
            
        case 'alerts_cleared':
            console.log(' Alerts cleared');
            alerts = [];
            renderAlerts();
            break;
            
        default:
            console.log('Unknown message type:', type);
    }
}

function updateStatus(status) {
    const statusEl = document.getElementById('network-status');
    if (statusEl) {
        statusEl.textContent = status;
    }
}

function flashAlertButton() {
    const alertBtn = document.querySelector('[data-view="alerts"]');
    if (alertBtn && !alertBtn.classList.contains('active')) {
        alertBtn.style.animation = 'flash 0.5s ease-in-out 3';
        setTimeout(() => {
            alertBtn.style.animation = '';
        }, 1500);
    }
}

// ============================================
// View Switching Logic
// ============================================
function initViewSwitching() {
    const navButtons = document.querySelectorAll('.nav-btn');
    const viewPanels = document.querySelectorAll('.view-panel');

    navButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const targetView = btn.dataset.view;

            // Update button states
            navButtons.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');

            // Update panel visibility
            viewPanels.forEach(panel => panel.classList.remove('active'));

            if (targetView === 'map') {
                document.getElementById('map-view').classList.add('active');
                if (pyroMap) {
                    setTimeout(() => pyroMap.invalidateSize(), 100);
                }
            } else if (targetView === 'alerts') {
                document.getElementById('alert-view').classList.add('active');
                renderAlerts();
            }
        });
    });

    // Filter dropdown handler
    document.getElementById('filter-type').addEventListener('change', renderAlerts);

    // Clear alerts button
    document.getElementById('clear-alerts').addEventListener('click', async () => {
        if (confirm('Clear all alerts?')) {
            try {
                await fetch('/api/alerts', { method: 'DELETE' });
                alerts = [];
                renderAlerts();
            } catch (err) {
                console.error('Failed to clear alerts:', err);
            }
        }
    });
}

// ============================================
// Alert Management
// ============================================
function addAlertToList(alert) {
    // Convert timestamp string to Date if needed
    if (typeof alert.timestamp === 'string') {
        alert.timestamp = new Date(alert.timestamp);
    }
    alerts.unshift(alert);
    
    // Keep only last 100
    if (alerts.length > 100) {
        alerts = alerts.slice(0, 100);
    }
    
    // Re-render if alerts view is active
    if (document.getElementById('alert-view').classList.contains('active')) {
        renderAlerts();
    }
}

function renderAlerts() {
    const alertList = document.getElementById('alert-list');
    const filterType = document.getElementById('filter-type').value;

    let filteredAlerts = alerts;
    if (filterType !== 'all') {
        filteredAlerts = alerts.filter(a => a.type === filterType);
    }

    if (filteredAlerts.length === 0) {
        alertList.innerHTML = '<div class="no-alerts">No alerts recorded yet.</div>';
        return;
    }

    alertList.innerHTML = filteredAlerts.map(alert => `
        <div class="alert-item ${alert.type}">
            <div class="alert-content">
                <div class="alert-title">${escapeHtml(alert.title)}</div>
                <div class="alert-details">${escapeHtml(alert.details || '')}</div>
                <div class="alert-timestamp">${formatTimestamp(alert.timestamp)}</div>
            </div>
            <div class="alert-meta">
                <div class="alert-device">${escapeHtml(alert.device_id || 'System')}</div>
            </div>
        </div>
    `).join('');
}

function formatTimestamp(date) {
    if (!(date instanceof Date)) {
        date = new Date(date);
    }
    return date.toLocaleString('en-US', {
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
}

function escapeHtml(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// ============================================
// Map Initialization
// ============================================
function initMap() {
    pyroMap = L.map('map').setView([40.4237, -86.9212], 15);

    L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
        attribution: '© OpenStreetMap contributors'
    }).addTo(pyroMap);

    window.addEventListener('resize', () => {
        if (pyroMap) pyroMap.invalidateSize();
    });

    return pyroMap;
}

// ============================================
// Device Management
// ============================================
function updateDeviceOnMap(deviceData) {
    const { id, lat, lng, risk_level, temp, humidity, gas_level, particulates, battery } = deviceData;

    // Risk level colors
    const colors = {
        0: '#22c55e', // Green - Low
        1: '#eab308', // Yellow - Moderate  
        2: '#f97316', // Orange - High
        3: '#ef4444'  // Red - Critical
    };
    const color = colors[risk_level] || '#22c55e';

    // Remove existing marker if present
    if (devices.has(id)) {
        pyroMap.removeLayer(devices.get(id));
    }

    // Create new marker
    const marker = L.circleMarker([lat, lng], {
        radius: 12,
        fillColor: color,
        color: "#000",
        weight: 2,
        opacity: 1,
        fillOpacity: 0.8
    }).addTo(pyroMap);

    // Build popup content
    let popupHtml = `
        <div style="color: #000; min-width: 160px;">
            <b style="font-size: 1.1em;">Node: ${escapeHtml(id)}</b>
            <hr style="margin: 5px 0; border-color: #ccc;">
            <table style="width: 100%;">
                <tr><td>Risk Level:</td><td><b>${risk_level}</b></td></tr>
    `;
    
    if (temp !== null && temp !== undefined) {
        popupHtml += `<tr><td>Temperature:</td><td>${temp.toFixed(1)}°C</td></tr>`;
    }
    if (humidity !== null && humidity !== undefined) {
        popupHtml += `<tr><td>Humidity:</td><td>${humidity.toFixed(1)}%</td></tr>`;
    }
    if (gas_level !== null && gas_level !== undefined) {
        popupHtml += `<tr><td>VOC:</td><td>${gas_level.toFixed(0)} ppm</td></tr>`;
    }
    if (particulates !== null && particulates !== undefined) {
        popupHtml += `<tr><td>PM:</td><td>${particulates.toFixed(1)} µg/m³</td></tr>`;
    }
    if (battery !== null && battery !== undefined) {
        popupHtml += `<tr><td>Battery:</td><td>${battery}%</td></tr>`;
    }
    
    popupHtml += `</table></div>`;
    
    marker.bindPopup(popupHtml);

    // Store reference
    devices.set(id, marker);
    updateDeviceCount();
}

function removeDeviceFromMap(deviceId) {
    if (devices.has(deviceId)) {
        pyroMap.removeLayer(devices.get(deviceId));
        devices.delete(deviceId);
        updateDeviceCount();
    }
}

function updateDeviceCount() {
    const countEl = document.getElementById('online-count');
    if (countEl) {
        countEl.textContent = devices.size;
    }
}

// ============================================
// API Fetching
// ============================================
async function fetchDevices() {
    try {
        const response = await fetch('/api/devices');
        if (response.ok) {
            const deviceList = await response.json();
            deviceList.forEach(device => updateDeviceOnMap(device));
            updateStatus('Monitoring Mesh...');
        }
    } catch (error) {
        console.log('Could not fetch devices:', error);
        updateStatus('Offline Mode');
    }
}

async function fetchAlerts() {
    try {
        const response = await fetch('/api/alerts');
        if (response.ok) {
            const alertList = await response.json();
            alerts = alertList.map(a => ({
                ...a,
                timestamp: new Date(a.timestamp)
            }));
        }
    } catch (error) {
        console.log('Could not fetch alerts:', error);
    }
}

// ============================================
// Initialization
// ============================================
document.addEventListener('DOMContentLoaded', async () => {
    console.log(' PyroNet CSP Initializing...');
    
    // Initialize UI
    initViewSwitching();
    initMap();
    
    // Fetch existing data from server
    await fetchDevices();
    await fetchAlerts();
    
    // Connect WebSocket for real-time updates
    initWebSocket();
    
    console.log(' PyroNet CSP Ready');
});

// Fix map size after window fully loads
window.addEventListener('load', () => {
    if (pyroMap) {
        pyroMap.invalidateSize();
    }
});
