# Working Plan: Terminal Tab for ECO Voice Web Dashboard

## What we're building

A new **Terminal** tab in the web dashboard (`Dashboard.jsx`) where the user can type text
commands directly in the browser. The terminal sends those commands to Firebase, the ESP32
picks them up and toggles the relays, and the terminal prints back a response line — just like
the serial monitor experience in the `main` (offline) branch, but running over the cloud.

---

## Reference: What the main branch serial terminal does

The offline firmware (`main` branch, `main.cpp`) runs a `serial_task` that:

1. Reads characters from stdin one at a time
2. Buffers them until `\n` is received
3. Normalizes the input: lowercase, strip whitespace and CR/LF
4. Calls `processCommand()` which matches the string and dispatches

### Commands supported (from main branch)

| Command | Action |
|---------|--------|
| `help` | Print available commands |
| `status` | Print all sensor readings + relay states |
| `light on` | Turn light relay ON (with sensor warnings) |
| `light off` | Turn light relay OFF |
| `fan on` | Turn fan relay ON (with sensor warnings) |
| `fan off` | Turn fan relay OFF |
| `all off` | Turn off both light and fan |
| `lock` | Lock the system (red LED off) |
| `<secret code>` | Enter secret code to unlock system |

### Sensor guard behavior (main branch — important to replicate)

The main branch **warns but does not block** when sensor thresholds are crossed.
This is the exact behavior to carry forward into the web terminal.

**`light on` guards:**
- No motion detected → print `Warning: no motion detected.`
- Light level > `BRIGHTNESS_THRESHOLD` (600) → print `Warning: ambient light is already above threshold.`
- Voltage low (< 4.5V) → print `Warning: low voltage detected.`
- Voltage fluctuating → print `Warning: voltage fluctuation detected.`
- Then executes regardless: `Light turned ON.`

**`fan on` guards:**
- No motion detected → print `Warning: no motion detected.`
- Temperature < 22°C OR humidity < 40% → print `Warning: temperature or humidity is below the fan recommendation threshold.`
- Voltage low → print `Warning: low voltage detected.`
- Voltage fluctuating → print `Warning: voltage fluctuation detected.`
- Then executes regardless: `Fan turned ON.`

**`light off` / `fan off` / `all off`:** no guards, execute immediately.

---

## Plan: Terminal tab in the web app

### File to modify
`webapp/src/components/Dashboard.jsx` — add a Terminal tab alongside the existing Appliances,
Voice Control, and Sensors sections.

### UI layout

Add a tab bar at the top of the dashboard (or a collapsible section):
- **Dashboard** tab — existing sensor cards + appliance toggles (current view)
- **Terminal** tab — new text terminal

The terminal section looks like:

```
┌──────────────────────────────────────────────────────────────┐
│  Terminal                                           [Clear]  │
├──────────────────────────────────────────────────────────────┤
│  > light on                                                  │
│  Warning: no motion detected.                                │
│  Light turned ON.                                            │
│  > status                                                    │
│  Temperature : 28.4°C                                        │
│  Humidity    : 55.0%                                         │
│  Motion      : not detected                                  │
│  Light level : 320                                           │
│  Current     : 0.45 A                                        │
│  Voltage     : 5.01 V                                        │
│  Power       : 2.25 W                                        │
│  Light relay : ON                                            │
│  Fan relay   : OFF                                           │
│  >                                                           │
├──────────────────────────────────────────────────────────────┤
│  [input field]                          [Send / Enter key]   │
└──────────────────────────────────────────────────────────────┘
```

### State needed (in Dashboard.jsx)

```js
const [activeTab, setActiveTab] = useState('dashboard')  // 'dashboard' | 'terminal'
const [terminalLines, setTerminalLines] = useState([])   // array of {type, text} objects
const [terminalInput, setTerminalInput] = useState('')
const terminalEndRef = useRef(null)                      // for auto-scroll
```

Line types for styling:
- `'input'` — the command the user typed (shown with `> ` prefix, bold)
- `'output'` — normal response text
- `'warning'` — sensor warnings (yellow/amber)
- `'error'` — unknown command (red)

### Command parser (pure JS, runs in the browser)

The parser mirrors `processCommand()` from the main branch exactly:

```js
function parseCommand(raw, sensors, commands, status) {
  const cmd = raw.trim().toLowerCase()
  const lines = []

  if (cmd === 'help') {
    lines.push({ type: 'output', text: 'Available commands:' })
    lines.push({ type: 'output', text: '  help       — show this help' })
    lines.push({ type: 'output', text: '  status     — print sensor and relay state' })
    lines.push({ type: 'output', text: '  light on   — turn light ON' })
    lines.push({ type: 'output', text: '  light off  — turn light OFF' })
    lines.push({ type: 'output', text: '  fan on     — turn fan ON' })
    lines.push({ type: 'output', text: '  fan off    — turn fan OFF' })
    lines.push({ type: 'output', text: '  all off    — turn off both' })
    return { lines, firebaseWrite: null }
  }

  if (cmd === 'status') {
    lines.push({ type: 'output', text: `Temperature : ${sensors.temperature?.toFixed(1) ?? '--'} °C` })
    lines.push({ type: 'output', text: `Humidity    : ${sensors.humidity?.toFixed(1) ?? '--'} %` })
    lines.push({ type: 'output', text: `Motion      : ${sensors.motion ? 'detected' : 'not detected'}` })
    lines.push({ type: 'output', text: `Light level : ${sensors.light_level ?? '--'}` })
    lines.push({ type: 'output', text: `Current     : ${sensors.current?.toFixed(2) ?? '--'} A` })
    lines.push({ type: 'output', text: `Voltage     : ${sensors.voltage?.toFixed(2) ?? '--'} V` })
    lines.push({ type: 'output', text: `Power       : ${sensors.power?.toFixed(2) ?? '--'} W` })
    lines.push({ type: 'output', text: `Light relay : ${status.light_on ? 'ON' : 'OFF'}` })
    lines.push({ type: 'output', text: `Fan relay   : ${status.fan_on ? 'ON' : 'OFF'}` })
    return { lines, firebaseWrite: null }
  }

  if (cmd === 'light on') {
    if (!sensors.motion)           lines.push({ type: 'warning', text: 'Warning: no motion detected.' })
    if (sensors.light_level > 600) lines.push({ type: 'warning', text: 'Warning: ambient light is already above threshold.' })
    if (sensors.voltage > 0 && sensors.voltage < 4.5) lines.push({ type: 'warning', text: 'Warning: low voltage detected.' })
    lines.push({ type: 'output', text: 'Light turned ON.' })
    return { lines, firebaseWrite: { path: '/eco_voice/commands/light', value: true } }
  }

  if (cmd === 'light off') {
    lines.push({ type: 'output', text: 'Light turned OFF.' })
    return { lines, firebaseWrite: { path: '/eco_voice/commands/light', value: false } }
  }

  if (cmd === 'fan on') {
    if (!sensors.motion)                             lines.push({ type: 'warning', text: 'Warning: no motion detected.' })
    if (sensors.temperature < 22 || sensors.humidity < 40)
      lines.push({ type: 'warning', text: 'Warning: temperature or humidity is below the fan recommendation threshold.' })
    if (sensors.voltage > 0 && sensors.voltage < 4.5) lines.push({ type: 'warning', text: 'Warning: low voltage detected.' })
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

  // Unknown
  lines.push({ type: 'error', text: `Unknown command: ${cmd}` })
  lines.push({ type: 'error', text: "Type 'help' to see available commands." })
  return { lines, firebaseWrite: null }
}
```

### Submitting a command (handler in Dashboard.jsx)

```js
const submitTerminalCommand = () => {
  if (!terminalInput.trim()) return

  const inputLine = { type: 'input', text: terminalInput }
  const { lines, firebaseWrite } = parseCommand(terminalInput, sensors, commands, status)

  // Write to Firebase if the command controls an appliance
  if (firebaseWrite) {
    const writes = Array.isArray(firebaseWrite) ? firebaseWrite : [firebaseWrite]
    writes.forEach(w => set(ref(db, w.path), w.value))
  }

  setTerminalLines(prev => [...prev, inputLine, ...lines])
  setTerminalInput('')
}
```

Auto-scroll to bottom whenever `terminalLines` changes:
```js
useEffect(() => {
  terminalEndRef.current?.scrollIntoView({ behavior: 'smooth' })
}, [terminalLines])
```

### Input field behavior
- Press **Enter** → submit command
- **Clear** button → `setTerminalLines([])`
- Input is disabled when device is offline
- Terminal box is fixed height with overflow-y scroll

### CSS notes (`Dashboard.css`)

```css
.terminal-box {
  background: #0d1117;
  color: #c9d1d9;
  font-family: 'Courier New', monospace;
  font-size: 13px;
  padding: 12px;
  height: 300px;
  overflow-y: auto;
  border-radius: 6px;
  border: 1px solid #30363d;
}

.terminal-line--input  { color: #58a6ff; }   /* blue — user input with > prefix */
.terminal-line--output { color: #c9d1d9; }   /* default white */
.terminal-line--warning { color: #d29922; }  /* amber — sensor warnings */
.terminal-line--error  { color: #f85149; }   /* red — unknown command */

.terminal-input-row {
  display: flex;
  gap: 8px;
  margin-top: 8px;
}

.terminal-input-row input {
  flex: 1;
  font-family: 'Courier New', monospace;
  background: #161b22;
  border: 1px solid #30363d;
  color: #c9d1d9;
  padding: 6px 10px;
  border-radius: 4px;
}
```

---

## Files to create / edit

| File | Change |
|------|--------|
| `webapp/src/components/Dashboard.jsx` | Add tab state, Terminal tab section, `parseCommand()`, `submitTerminalCommand()`, auto-scroll |
| `webapp/src/components/Dashboard.css` | Add `.terminal-box`, `.terminal-line--*`, `.terminal-input-row`, `.tab-bar` styles |

No new files needed beyond what's already in the repo. No Firebase schema changes.
No firmware changes required — the terminal uses the same `/eco_voice/commands/light` and
`/eco_voice/commands/fan` paths that the toggle buttons already write to.

---

## What the terminal does NOT do (intentional scope)

- No secret code unlock flow — the online version doesn't have a lock/unlock concept (Firebase Auth is the gate)
- No `gpio test` / `pin test` / `mic log` — those are ESP32 diagnostic commands that only make sense on the serial monitor
- No `lock` command — not applicable in online version

---

## Test checklist before shipping

- [ ] `help` prints command list
- [ ] `status` shows live sensor values from Firebase
- [ ] `light on` writes to Firebase → relay toggles → confirmed state updates in dashboard tab
- [ ] `fan on` with cold temp → warning line shown, fan still turns on
- [ ] `light on` with no motion → warning shown, light still turns on
- [ ] `all off` turns both relays off
- [ ] Unknown command shows error in red
- [ ] Terminal auto-scrolls to latest line
- [ ] Clear button empties the terminal
- [ ] Input disabled when device is offline
- [ ] Switching between Dashboard and Terminal tabs preserves terminal history
