// File: src/pages/Home/Home.js
// Home page displaying welcome information.
import React from 'react';
import './Home.css';

function Home() {
  console.log("Home component rendered");
  return (
    <div className="home">
      <h2>Welcome to the Online Auction System</h2>
      <p>Bid on items and win auctions in real time!</p>
    </div>
  );
}

export default Home;
