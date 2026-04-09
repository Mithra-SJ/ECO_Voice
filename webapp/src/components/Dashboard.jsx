import { useState, useEffect } from 'react'
import { ref, onValue, set } from 'firebase/database'
import { signOut } from 'firebase/auth'
import { db, auth } from '../firebase'
import './Dashboard.css'

function Dashboard({ user }) {
  const [sensors, setSensors] = useState({})
  const [commands, setCommands] = useState({ light: false, fan: false })
  const [status, setStatus] = useState({})
  const [online, setOnline] = useState(false)

  useEffect(() => {
    const ecoRef = ref(db, '/eco_voice')
    const unsub = onValue(ecoRef, (snapshot) => {
      const data = snapshot.val() || {}
      setSensors(data.sensors || {})
      setCommands(data.commands || { light: false, fan: false })
      setStatus(data.status || {})

      // Device is online if last_seen within 10 seconds
      const lastSeen = data.status?.last_seen
      if (lastSeen) {
        const ageMs = Date.now() - lastSeen
        setOnline(ageMs < 10000)
      }
    })
    return unsub
  }, [])

  // Refresh online status every 3 seconds
  useEffect(() => {
    const interval = setInterval(() => {
      if (status.last_seen) {
        setOnline(Date.now() - status.last_seen < 10000)
      }
    }, 3000)
    return () => clearInterval(interval)
  }, [status])

  const toggleLight = () => {
    const next = !commands.light
    set(ref(db, '/eco_voice/commands/light'), next)
  }

  const toggleFan = () => {
    const next = !commands.fan
    set(ref(db, '/eco_voice/commands/fan'), next)
  }

  const handleLogout = () => signOut(auth)

  const lastSeenText = () => {
    if (!status.last_seen) return 'Never'
    const sec = Math.floor((Date.now() - status.last_seen) / 1000)
    if (sec < 60) return `${sec}s ago`
    return `${Math.floor(sec / 60)}m ago`
  }

  return (
    <div className="dashboard">
      {/* Header */}
      <div className="header">
        <div>
          <h1>ECO Voice</h1>
          <span className={`device-status ${online ? 'online' : 'offline'}`}>
            {online ? 'Device Online' : 'Device Offline'}
          </span>
          <span className="last-seen">Last seen: {lastSeenText()}</span>
        </div>
        <button className="logout-btn" onClick={handleLogout}>Logout</button>
      </div>

      {/* Alerts */}
      {sensors.motion === false && (
        <div className="alert">No motion detected in the room</div>
      )}
      {sensors.voltage > 0 && sensors.voltage < 4.5 && (
        <div className="alert alert-warn">Low voltage detected: {sensors.voltage?.toFixed(2)}V</div>
      )}

      {/* Appliance Controls */}
      <div className="section-title">Appliances</div>
      <div className="appliance-grid">
        <div className={`appliance-card ${commands.light ? 'active' : ''}`}>
          <div className="appliance-icon">💡</div>
          <div className="appliance-name">Light</div>
          <div className="appliance-state">{status.light_on ? 'ON' : 'OFF'}</div>
          <button
            className={`toggle-btn ${commands.light ? 'on' : 'off'}`}
            onClick={toggleLight}
            disabled={!online}
          >
            Turn {commands.light ? 'Off' : 'On'}
          </button>
        </div>

        <div className={`appliance-card ${commands.fan ? 'active' : ''}`}>
          <div className="appliance-icon">🌀</div>
          <div className="appliance-name">Fan</div>
          <div className="appliance-state">{status.fan_on ? 'ON' : 'OFF'}</div>
          <button
            className={`toggle-btn ${commands.fan ? 'on' : 'off'}`}
            onClick={toggleFan}
            disabled={!online}
          >
            Turn {commands.fan ? 'Off' : 'On'}
          </button>
        </div>
      </div>

      {/* Sensor Readings */}
      <div className="section-title">Sensors</div>
      <div className="sensor-grid">
        <div className="sensor-card">
          <div className="sensor-label">Temperature</div>
          <div className="sensor-value">{sensors.temperature?.toFixed(1) ?? '--'}°C</div>
        </div>
        <div className="sensor-card">
          <div className="sensor-label">Humidity</div>
          <div className="sensor-value">{sensors.humidity?.toFixed(1) ?? '--'}%</div>
        </div>
        <div className="sensor-card">
          <div className="sensor-label">Motion</div>
          <div className={`sensor-value ${sensors.motion ? 'green' : 'red'}`}>
            {sensors.motion ? 'Detected' : 'None'}
          </div>
        </div>
        <div className="sensor-card">
          <div className="sensor-label">Light Level</div>
          <div className="sensor-value">{sensors.light_level ?? '--'}</div>
        </div>
        <div className="sensor-card">
          <div className="sensor-label">Current</div>
          <div className="sensor-value">{sensors.current?.toFixed(2) ?? '--'} A</div>
        </div>
        <div className="sensor-card">
          <div className="sensor-label">Voltage</div>
          <div className="sensor-value">{sensors.voltage?.toFixed(2) ?? '--'} V</div>
        </div>
        <div className="sensor-card">
          <div className="sensor-label">Power</div>
          <div className="sensor-value">{sensors.power?.toFixed(2) ?? '--'} W</div>
        </div>
      </div>
    </div>
  )
}

export default Dashboard
