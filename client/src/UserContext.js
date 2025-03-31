// File: src/UserContext.js
// This file creates a React context to share user state (token, username, balance)
// between components such as Header and Profile.
import React from 'react';

const UserContext = React.createContext();

export default UserContext;
