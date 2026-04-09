import { initializeApp } from 'firebase/app'
import { getAuth } from 'firebase/auth'
import { getDatabase } from 'firebase/database'

const firebaseConfig = {
  apiKey: "AIzaSyAoVKRRs_oyRWM9f56N7AoVDlAX2o0DF5w",
  authDomain: "eco-voice-ad60e.firebaseapp.com",
  databaseURL: "https://eco-voice-ad60e-default-rtdb.firebaseio.com",
  projectId: "eco-voice-ad60e",
  storageBucket: "eco-voice-ad60e.firebasestorage.app",
  messagingSenderId: "818106054098",
  appId: "1:818106054098:web:661cdbe4bb5b9ec213840b"
}

const app = initializeApp(firebaseConfig)
export const auth = getAuth(app)
export const db = getDatabase(app)
