import React, { useState, useEffect } from 'react';
import Lobby from './components/Lobby';
import Workspace from './components/Workspace';

export default function App() {
  const [roomId, setRoomId] = useState(null);
  const [role, setRole] = useState(null);

  // Check URL parameters on load to support shared links
  useEffect(() => {
    const params = new URLSearchParams(window.location.search);
    const roomParam = params.get('room');
    const roleParam = params.get('role');

    if (roomParam && roleParam) {
      setRoomId(roomParam);
      setRole(roleParam);
    }
  }, []);

  const handleJoinRoom = (room, userRole) => {
    setRoomId(room);
    setRole(userRole);
    // Update the URL to include parameters for easy sharing and page refreshes
    const newUrl = `${window.location.origin}/?room=${room}&role=${userRole}`;
    window.history.pushState({ room, role: userRole }, '', newUrl);
  };

  const handleLeaveRoom = () => {
    setRoomId(null);
    setRole(null);
    // Clear URL parameters
    window.history.pushState({}, '', window.location.origin);
  };

  // Handle browser back/forward buttons
  useEffect(() => {
    const handlePopState = (e) => {
      const params = new URLSearchParams(window.location.search);
      const roomParam = params.get('room');
      const roleParam = params.get('role');
      
      if (roomParam && roleParam) {
        setRoomId(roomParam);
        setRole(roleParam);
      } else {
        setRoomId(null);
        setRole(null);
      }
    };
    
    window.addEventListener('popstate', handlePopState);
    return () => window.removeEventListener('popstate', handlePopState);
  }, []);

  if (roomId && role) {
    return (
      <Workspace 
        roomId={roomId} 
        role={role} 
        onLeave={handleLeaveRoom} 
      />
    );
  }

  return (
    <Lobby 
      onJoinRoom={handleJoinRoom} 
    />
  );
}
