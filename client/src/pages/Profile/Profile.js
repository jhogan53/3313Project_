// File: src/pages/Profile/Profile.js
// Profile page: Displays current username, balance, and provides deposit/withdraw functionality.
import React, { useState, useEffect } from 'react';
import './Profile.css';

function Profile() {
  const [profile, setProfile] = useState({ username: '', balance: '' });
  const [depositAmount, setDepositAmount] = useState('');
  const [withdrawAmount, setWithdrawAmount] = useState('');

  const token = localStorage.getItem("token");

  useEffect(() => {
    // Fetch profile info from backend.
    const fetchProfile = async () => {
      try {
        const response = await fetch('/profile', {
          method: 'GET',
          headers: {
            "Authorization": `Bearer ${token}`
          }
        });
        const data = await response.json();
        if (response.ok) {
          setProfile({ username: data.username, balance: data.balance });
          localStorage.setItem("balance", data.balance);
        } else {
          alert(`Error: ${data.error}`);
        }
      } catch (err) {
        alert("Network error");
      }
    };
    fetchProfile();
  }, [token]);

  const handleDeposit = async (e) => {
    e.preventDefault();
    try {
      const response = await fetch('/deposit', {
        method: 'POST',
        headers: {
          "Content-Type": "application/json",
          "Authorization": `Bearer ${token}`
        },
        body: JSON.stringify({ amount: parseFloat(depositAmount) })
      });
      const data = await response.json();
      if (response.ok) {
        alert(data.message);
        // Refresh profile info.
        const res = await fetch('/profile', { method: 'GET', headers: {"Authorization": `Bearer ${token}`} });
        const profileData = await res.json();
        setProfile({ username: profileData.username, balance: profileData.balance });
        localStorage.setItem("balance", profileData.balance);
        setDepositAmount('');
      } else {
        alert(`Error: ${data.error}`);
      }
    } catch (err) {
      alert("Network error");
    }
  };

  const handleWithdraw = async (e) => {
    e.preventDefault();
    try {
      const response = await fetch('/withdraw', {
        method: 'POST',
        headers: {
          "Content-Type": "application/json",
          "Authorization": `Bearer ${token}`
        },
        body: JSON.stringify({ amount: parseFloat(withdrawAmount) })
      });
      const data = await response.json();
      if (response.ok) {
        alert(data.message);
        // Refresh profile info.
        const res = await fetch('/profile', { method: 'GET', headers: {"Authorization": `Bearer ${token}`} });
        const profileData = await res.json();
        setProfile({ username: profileData.username, balance: profileData.balance });
        localStorage.setItem("balance", profileData.balance);
        setWithdrawAmount('');
      } else {
        alert(`Error: ${data.error}`);
      }
    } catch (err) {
      alert("Network error");
    }
  };

  return (
    <div className="profile">
      <h2>Profile</h2>
      <p><strong>Username:</strong> {profile.username}</p>
      <p><strong>Balance:</strong> ${profile.balance}</p>
      <div className="transactions">
        <form onSubmit={handleDeposit}>
          <h3>Deposit Funds</h3>
          <input 
            type="number" 
            placeholder="Amount" 
            value={depositAmount} 
            onChange={(e) => setDepositAmount(e.target.value)}
            required 
          />
          <button type="submit">Deposit</button>
        </form>
        <form onSubmit={handleWithdraw}>
          <h3>Withdraw Funds</h3>
          <input 
            type="number" 
            placeholder="Amount" 
            value={withdrawAmount} 
            onChange={(e) => setWithdrawAmount(e.target.value)}
            required 
          />
          <button type="submit">Withdraw</button>
        </form>
      </div>
    </div>
  );
}

export default Profile;
