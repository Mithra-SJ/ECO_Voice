import { useState, useEffect, useRef } from 'react'
import { ref, onValue, set } from 'firebase/database'
import { signOut } from 'firebase/auth'
import { db, auth } from '../firebase'
import './Dashboard.css'

// ── Command parser (mirrors main-branch serial terminal logic) ────────────────
function parseCommand(raw, sensors, status) {
  const cmd = raw.trim().toLowerCase()
  const lines = []

  if (!cmd) return { lines: [], firebaseWrite: null }

  if (cmd === 'help') {
    lines.push({ type: 'output', text: 'Available commands:' })
    lines.push({ type: 'output', text: '  help       — show this help' })
    lines.push({ type: 'output', text: '  status     — print sensor and relay state' })
    lines.push({ type: 'output', text: '  light on   — turn light ON' })
    lines.push({ type: 'output', text: '  light off  — turn light OFF' })
    lines.push({ type: 'output', text: '  fan on     — turn fan ON' })
    lines.push({ type: 'output', text: '  fan off    — turn fan OFF' })
    lines.push({ type: 'output', text: '  all off    — turn off both appliances' })
    return { lines, firebaseWrite: null }
  }

  if (cmd === 'status') {
    lines.push({ type: 'output', text: '─────────────────────────' })
    lines.push({ type: 'output', text: `  Temperature : ${sensors.temperature?.toFixed(1) ?? '--'} °C` })
    lines.push({ type: 'output', text: `  Humidity    : ${sensors.humidity?.toFixed(1) ?? '--'} %` })
    lines.push({ type: 'output', text: `  Motion      : ${sensors.motion ? 'detected' : 'not detected'}` })
    lines.push({ type: 'output', text: `  Light level : ${sensors.light_level ?? '--'}` })
    lines.push({ type: 'output', text: `  Current     : ${sensors.current?.toFixed(2) ?? '--'} A` })
    lines.push({ type: 'output', text: `  Voltage     : ${sensors.voltage?.toFixed(2) ?? '--'} V` })
    lines.push({ type: 'output', text: `  Power       : ${sensors.power?.toFixed(2) ?? '--'} W` })
    lines.push({ type: 'output', text: `  Light relay : ${status.light_on ? 'ON' : 'OFF'}` })
    lines.push({ type: 'output', text: `  Fan relay   : ${status.fan_on ? 'ON' : 'OFF'}` })
    lines.push({ type: 'output', text: '─────────────────────────' })
    return { lines, firebaseWrite: null }
  }

  if (cmd === 'light on') {
    if (!sensors.motion)
      lines.push({ type: 'warning', text: 'Warning: no motion detected.' })
    if ((sensors.light_level ?? 0) > 600)
      lines.push({ type: 'warning', text: 'Warning: ambient light is already above threshold.' })
    if (sensors.voltage > 0 && sensors.voltage < 4.5)
      lines.push({ type: 'warning', text: 'Warning: low voltage detected.' })
    lines.push({ type: 'output', text: 'Light turned ON.' })
    return { lines, firebaseWrite: { path: '/eco_voice/commands/light', value: true } }
  }

  if (cmd === 'light off') {
    lines.push({ type: 'output', text: 'Light turned OFF.' })
    return { lines, firebaseWrite: { path: '/eco_voice/commands/light', value: false } }
  }

  if (cmd === 'fan on') {
    if (!sensors.motion)
      lines.push({ type: 'warning', text: 'Warning: no motion detected.' })
    if ((sensors.temperature ?? 99) < 22 || (sensors.humidity ?? 99) < 40)
      lines.push({ type: 'warning', text: 'Warning: temperature or humidity is below the fan recommendation threshold.' })
    if (sensors.voltage > 0 && sensors.voltage < 4.5)
      lines.push({ type: 'warning', text: 'Warning: low voltage detected.' })
    lines.push({ type: 'output', text: 'Fan turned ON.' })
    return { lines, firebaseWrite: { path: '/eco_voice/commands/fan', value: true } }
  }

  if (cmd === 'fan off') {
    lines.push({ type: 'output', text: 'Fan turned OFF.' })
    return { lines, firebaseWrite: { path: '/eco_voice/commands/fan', value: false } }
  }

  if (cmd === 'all off') {
    lines.push({ type: 'output', text: 'All appliances turned OFF.' })
    return {
      lines,
      firebaseWrite: [
        { path: '/eco_voice/commands/light', value: false },
        { path: '/eco_voice/commands/fan',   value: false },
      ]
    }
  }

  lines.push({ type: 'error', text: `Unknown command: ${cmd}` })
  lines.push({ type: 'error', text: "Type 'help' to see available commands." })
  return { lines, firebaseWrite: null }
}

// ── Component ─────────────────────────────────────────────────────────────────
function Dashboard({ user }) {
  const [sensors, setSensors]   = useState({})
  const [commands, setCommands] = useState({ light: false, fan: false })
  const [status, setStatus]     = useState({})
  const [online, setOnline]     = useState(false)

  // Voice control
  const [listening, setListening]     = useState(false)
  const [voiceStatus, setVoiceStatus] = useState('')
  const recognitionRef = useRef(null)

  // Tab + terminal
  const [activeTab, setActiveTab]         = useState('dashboard')
  const [terminalLines, setTerminalLines] = useState([
    { type: 'output', text: 'ECO Voice Terminal — type "help" to see available commands.' }
  ])
  const [terminalInput, setTerminalInput] = useState('')
  const terminalEndRef = useRef(null)

  // ── Firebase listener ────────────────────────────────────────────────────
  useEffect(() => {
    const ecoRef = ref(db, '/eco_voice')
    const unsub = onValue(ecoRef, (snapshot) => {
      const data = snapshot.val() || {}
      setSensors(data.sensors || {})
      setCommands(data.commands || { light: false, fan: false })
      setStatus(data.status || {})

      const lastSeen = data.status?.last_seen
      if (lastSeen) setOnline(Date.now() - lastSeen < 10000)
    })
    return unsub
  }, [])

  // Refresh online status every 3 seconds
  useEffect(() => {
    const interval = setInterval(() => {
      if (status.last_seen) setOnline(Date.now() - status.last_seen < 10000)
    }, 3000)
    return () => clearInterval(interval)
  }, [status])

  // Auto-scroll terminal to bottom on new output
  useEffect(() => {
    terminalEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [terminalLines])

  // ── Appliance toggles ────────────────────────────────────────────────────
  const toggleLight = () => set(ref(db, '/eco_voice/commands/light'), !commands.light)
  const toggleFan   = () => set(ref(db, '/eco_voice/commands/fan'),   !commands.fan)

  // ── Voice control ────────────────────────────────────────────────────────
  const handleVoiceCommand = (text) => {
    if (text.includes('light on') || text.includes('turn on light') || text.includes('switch on light')) {
      set(ref(db, '/eco_voice/commands/light'), true)
      setVoiceStatus('Light ON')
    } else if (text.includes('light off') || text.includes('turn off light') || text.includes('switch off light')) {
      set(ref(db, '/eco_voice/commands/light'), false)
      setVoiceStatus('Light OFF')
    } else if (text.includes('fan on') || text.includes('turn on fan') || text.includes('switch on fan')) {
      set(ref(db, '/eco_voice/commands/fan'), true)
      setVoiceStatus('Fan ON')
    } else if (text.includes('fan off') || text.includes('turn off fan') || text.includes('switch off fan')) {
      set(ref(db, '/eco_voice/commands/fan'), false)
      setVoiceStatus('Fan OFF')
    } else {
      setVoiceStatus(`Not recognized: "${text}"`)
    }
  }

  const startVoice = () => {
    const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition
    if (!SpeechRecognition) {
      setVoiceStatus('Voice not supported — use Chrome or Edge')
      return
    }
    const recognition = new SpeechRecognition()
    recognitionRef.current = recognition
    recognition.lang = 'en-US'
    recognition.interimResults = false
    recognition.maxAlternatives = 1
    recognition.onresult = (e) => handleVoiceCommand(e.results[0][0].transcript.toLowerCase().trim())
    recognition.onend    = () => setListening(false)
    recognition.onerror  = (e) => {
      setVoiceStatus(e.error === 'no-speech' ? 'No speech detected' : `Error: ${e.error}`)
      setListening(false)
    }
    recognition.start()
    setListening(true)
    setVoiceStatus('Listening...')
  }

  const stopVoice = () => {
    recognitionRef.current?.stop()
    setListening(false)
  }

  // ── Terminal ─────────────────────────────────────────────────────────────
  const submitTerminalCommand = () => {
    if (!terminalInput.trim()) return

    const { lines, firebaseWrite } = parseCommand(terminalInput, sensors, status)

    if (firebaseWrite) {
      const writes = Array.isArray(firebaseWrite) ? firebaseWrite : [firebaseWrite]
      writes.forEach(w => set(ref(db, w.path), w.value))
    }

    setTerminalLines(prev => [
      ...prev,
      { type: 'input', text: terminalInput },
      ...lines
    ])
    setTerminalInput('')
  }

  const handleTerminalKey = (e) => {
    if (e.key === 'Enter') submitTerminalCommand()
  }

  // ── Helpers ──────────────────────────────────────────────────────────────
  const handleLogout = () => signOut(auth)

  const lastSeenText = () => {
    if (!status.last_seen) return 'Never'
    const sec = Math.floor((Date.now() - status.last_seen) / 1000)
    if (sec < 60) return `${sec}s ago`
    return `${Math.floor(sec / 60)}m ago`
  }

  // ── Render ───────────────────────────────────────────────────────────────
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

      {/* Tab bar */}
      <div className="tab-bar">
        <button
          className={`tab-btn ${activeTab === 'dashboard' ? 'tab-btn--active' : ''}`}
          onClick={() => setActiveTab('dashboard')}
        >
          Dashboard
        </button>
        <button
          className={`tab-btn ${activeTab === 'terminal' ? 'tab-btn--active' : ''}`}
          onClick={() => setActiveTab('terminal')}
        >
          Terminal
        </button>
      </div>

      {/* ── Dashboard tab ────────────────────────────────────────────────── */}
      {activeTab === 'dashboard' && (
        <>
          {/* Alerts */}
          {sensors.motion === false && (
            <div className="alert">No motion detected in the room</div>
          )}
          {sensors.voltage > 0 && sensors.voltage < 4.5 && (
            <div className="alert alert-warn">Low voltage detected: {sensors.voltage?.toFixed(2)}V</div>
          )}

          {/* Appliances */}
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

          {/* Voice Control */}
          <div className="section-title">Voice Control</div>
          <div className="voice-panel">
            <button
              className={`voice-btn ${listening ? 'voice-btn--active' : ''}`}
              onClick={listening ? stopVoice : startVoice}
              disabled={!online}
              title={online ? 'Click to speak a command' : 'Device offline'}
            >
              {listening ? '⏹ Stop' : '🎙 Speak'}
            </button>
            <div className="voice-commands">
              Say: <em>"light on"</em>, <em>"light off"</em>, <em>"fan on"</em>, <em>"fan off"</em>
            </div>
            {voiceStatus && (
              <div className={`voice-status ${voiceStatus === 'Listening...' ? 'voice-status--listening' : ''}`}>
                {voiceStatus}
              </div>
            )}
          </div>

          {/* Sensors */}
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
        </>
      )}

      {/* ── Terminal tab ─────────────────────────────────────────────────── */}
      {activeTab === 'terminal' && (
        <div className="terminal-panel">
          <div className="terminal-header">
            <span className="terminal-title">ECO Voice Terminal</span>
            <button className="terminal-clear-btn" onClick={() => setTerminalLines([])}>
              Clear
            </button>
          </div>

          <div className="terminal-box">
            {terminalLines.map((line, i) => (
              <div key={i} className={`terminal-line terminal-line--${line.type}`}>
                {line.type === 'input' ? `> ${line.text}` : line.text}
              </div>
            ))}
            <div ref={terminalEndRef} />
          </div>

          <div className="terminal-input-row">
            <span className="terminal-prompt">{'>'}</span>
            <input
              type="text"
              className="terminal-input"
              value={terminalInput}
              onChange={e => setTerminalInput(e.target.value)}
              onKeyDown={handleTerminalKey}
              placeholder={online ? 'Type a command...' : 'Device offline'}
              disabled={!online}
              autoFocus
              spellCheck={false}
              autoComplete="off"
            />
            <button
              className="terminal-send-btn"
              onClick={submitTerminalCommand}
              disabled={!online}
            >
              Send
            </button>
          </div>
        </div>
      )}

    </div>
  )
}

export default Dashboard
