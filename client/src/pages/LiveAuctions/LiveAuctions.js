// File: src/pages/LiveAuctions/LiveAuctions.js
import React, { useState, useEffect, useContext } from 'react';
import UserContext from '../../UserContext';
import './LiveAuctions.css';

function LiveAuctions() {
  const { user } = useContext(UserContext);
  const [liveAuctions, setLiveAuctions] = useState([]);
  const [msg, setMsg] = useState({ text: '', type: '' });
  const [tick, setTick] = useState(0);
  const [bidInputs, setBidInputs] = useState({}); // keyed by auction_id

  // Update tick every second.
  useEffect(() => {
    const interval = setInterval(() => setTick(t => t + 1), 1000);
    return () => clearInterval(interval);
  }, []);

  // Helper: parse posted_time as UTC.
  const parsePostedTime = (timeStr) => {
    return timeStr.endsWith("Z") ? new Date(timeStr) : new Date(timeStr + "Z");
  };

  // Fetch live auctions from the backend.
  const fetchLiveAuctions = async () => {
    try {
      const response = await fetch('/live', {
        method: 'GET',
        headers: { 'Authorization': `Bearer ${user.token}` }
      });
      const data = await response.json();
      if (response.ok) {
        setLiveAuctions(data.live_auctions);
      } else {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      }
    } catch (err) {
      setMsg({ text: 'Network error', type: 'error' });
    }
  };

  useEffect(() => {
    fetchLiveAuctions();
  }, [user.token, tick]);

  // Timer calculation: if the auction has been made live early, we assume start_delay is 0.
  const calculateRemainingTime = (auction) => {
    const postedTime = parsePostedTime(auction.posted_time);
    const effectiveStart = new Date(postedTime.getTime() + auction.start_delay * 60000);
    const effectiveEnd = new Date(effectiveStart.getTime() + auction.live_duration * 60000);
    return effectiveEnd - new Date();
  };

  const formatTime = (ms) => {
    if (ms <= 0) return '00:00:00';
    const totalSeconds = Math.floor(ms / 1000);
    const hours = String(Math.floor(totalSeconds / 3600)).padStart(2, '0');
    const minutes = String(Math.floor((totalSeconds % 3600) / 60)).padStart(2, '0');
    const seconds = String(totalSeconds % 60).padStart(2, '0');
    return `${hours}:${minutes}:${seconds}`;
  };

  const handlePlaceBid = async (auctionId) => {
    const bidAmount = parseFloat(bidInputs[auctionId]);
    if (isNaN(bidAmount)) {
      setMsg({ text: 'Please enter a valid bid amount', type: 'error' });
      return;
    }
    try {
      const response = await fetch('/place_bid', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${user.token}`
        },
        body: JSON.stringify({ auction_id: auctionId, bid_amount: bidAmount })
      });
      const data = await response.json();
      if (!response.ok) {
        setMsg({ text: `Error: ${data.error}`, type: 'error' });
      } else {
        setMsg({ text: data.message, type: 'success' });
        // Re-fetch live auctions to update the highest bid info.
        fetchLiveAuctions();
      }
    } catch (err) {
      setMsg({ text: 'Network error', type: 'error' });
    }
  };

  const handleBidInputChange = (auctionId, value) => {
    setBidInputs(prev => ({ ...prev, [auctionId]: value }));
  };

  return (
    <div className="live-auctions">
      <h2>Live Auctions</h2>
      {msg.text && <div className={`msg ${msg.type}`}>{msg.text}</div>}
      {liveAuctions.length === 0 ? (
        <p>No live auctions found.</p>
      ) : (
        liveAuctions.map(auction => {
          const postedTime = parsePostedTime(auction.posted_time);
          const remainingTime = calculateRemainingTime(auction);
          return (
            <div key={auction.auction_id} className="auction-card">
              <h3>{auction.item_name}</h3>
              {auction.image_url && (
                <div className="auction-image">
                  <img src={auction.image_url} alt={auction.item_name} />
                </div>
              )}
              <p><strong>Description:</strong> {auction.description}</p>
              <p><strong>Base Price:</strong> ${auction.base_price}</p>
              <p><strong>Posted:</strong> {postedTime.toLocaleString()}</p>
              <p><strong>Live - Ends in:</strong> {formatTime(remainingTime)}</p>
              <div className="auction-result">
                {auction.highest_bid > 0 ? (
                  <p>
                    Current Highest Bid: ${auction.highest_bid} by {auction.highest_bidder}
                  </p>
                ) : (
                  <p>No bids yet</p>
                )}
              </div>
              {/* Allow bidding only if the logged in user is not the seller */}
              {auction.seller_id !== Number(user.user_id) && (
                <div className="bid-form">
                  <input
                    type="number"
                    placeholder="Enter your bid"
                    value={bidInputs[auction.auction_id] || ""}
                    onChange={(e) => handleBidInputChange(auction.auction_id, e.target.value)}
                  />
                  <button onClick={() => handlePlaceBid(auction.auction_id)}>Place Bid</button>
                </div>
              )}
            </div>
          );
        })
      )}
    </div>
  );
}

export default LiveAuctions;
