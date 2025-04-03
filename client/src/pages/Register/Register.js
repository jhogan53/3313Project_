// File: src/pages/Register/Register.js
// Register page with username and password form.
// On success, shows an embedded success message and then reloads to update header.

import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import './Register.css';

function Register() {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [msg, setMsg] = useState({ text: '', type: '' });
  const navigate = useNavigate();

  const handleSubmit = async (e) => {
    e.preventDefault();
    setMsg({ text: '', type: '' });
    try {
      const response = await fetch('/register', {
        method: 'POST',
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ username, password })
      });
      const data = await response.json();
      if (!response.ok) {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      } else {
        setMsg({ text: "Registration successful!", type: 'success' });
        setTimeout(() => {
          // After registration, force full reload to update state
          window.location.href = "/";
        }, 2000);
      }
    } catch (err) {
      setMsg({ text: "Network error", type: 'error' });
    }
  };

  return (
    <div className="register">
      <h2>Register</h2>
      {msg.text && (
        <div className={`msg ${msg.type}`}>
          {msg.text}
        </div>
      )}
      <form onSubmit={handleSubmit}>
        <div className="form-group">
          <label>Username:</label>
          <input 
            type="text"
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            required 
          />
        </div>
        <div className="form-group">
          <label>Password:</label>
          <input 
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            required 
          />
        </div>
        <button type="submit">Register</button>
      </form>
    </div>
  );
}

export default Register;
