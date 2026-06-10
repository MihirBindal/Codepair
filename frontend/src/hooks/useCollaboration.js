import { useEffect, useState, useRef } from 'react';

export default function useCollaboration(roomId, role) {
  const [code, setCode] = useState('');
  const [version, setVersion] = useState(0);
  const [problem, setProblem] = useState('');
  const [language, setLanguage] = useState('cpp');
  const [logs, setLogs] = useState([]);
  const [isRunning, setIsRunning] = useState(false);
  const [status, setStatus] = useState('connecting');

  const wsRef = useRef(null);
  const versionRef = useRef(0);

  // Sync ref to version state to read it in callback
  useEffect(() => {
    versionRef.current = version;
  }, [version]);

  useEffect(() => {
    if (!roomId || !role) return;

    setStatus('connecting');
    const ws = new WebSocket(`ws://localhost:4001/ws/${roomId}/${role}`);
    wsRef.current = ws;

    ws.onopen = () => {
      console.log(`WebSocket connected to room ${roomId} as ${role}`);
      setStatus('connected');
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        
        switch (msg.type) {
          case 'init':
            setCode(msg.code || '');
            setVersion(msg.version || 0);
            setLanguage(msg.language || 'cpp');
            setProblem(msg.problem || '');
            break;
            
          case 'edit':
            // Only apply edit if the incoming version is strictly newer than ours
            if (msg.version > versionRef.current) {
              setCode(msg.code || '');
              setVersion(msg.version);
            }
            break;
            
          case 'language':
            setLanguage(msg.language);
            break;
            
          case 'problem':
            setProblem(msg.problem || '');
            break;
            
          case 'output':
            const innerEvent = msg.data;
            if (innerEvent.event === 'stdout') {
              setLogs((prev) => [...prev, { type: 'stdout', text: innerEvent.data }]);
            } else if (innerEvent.event === 'stderr') {
              setLogs((prev) => [...prev, { type: 'stderr', text: innerEvent.data }]);
            } else if (innerEvent.event === 'exit') {
              setIsRunning(false);
              const exitCode = innerEvent.exit_code;
              const timedOut = innerEvent.timed_out;
              
              if (timedOut) {
                setLogs((prev) => [...prev, { type: 'system danger', text: '\n[System] Process terminated: 5s Execution Limit Exceeded.' }]);
              } else {
                setLogs((prev) => [
                  ...prev,
                  { 
                    type: exitCode === 0 ? 'system success' : 'system danger', 
                    text: `\n[System] Process exited with code ${exitCode}.` 
                  }
                ]);
              }
            }
            break;
            
          default:
            console.warn('Unknown WebSocket message type:', msg);
        }
      } catch (err) {
        console.error('Failed to parse incoming WebSocket message:', err, event.data);
      }
    };

    ws.onclose = () => {
      console.log('WebSocket connection closed.');
      setStatus('disconnected');
      setIsRunning(false);
    };

    ws.onerror = (err) => {
      console.error('WebSocket error:', err);
      setStatus('disconnected');
    };

    return () => {
      ws.close();
    };
  }, [roomId, role]);

  // Actions
  const updateCode = (newCode) => {
    setCode(newCode);
    const nextVersion = versionRef.current + 1;
    setVersion(nextVersion);

    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'edit',
        code: newCode,
        version: nextVersion
      }));
    }
  };

  const updateLanguage = (newLanguage) => {
    setLanguage(newLanguage);
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'language',
        language: newLanguage
      }));
    }
  };

  const updateProblem = (newProblem) => {
    setProblem(newProblem);
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'problem',
        problem: newProblem
      }));
    }
  };

  const runCode = () => {
    setLogs([{ type: 'system', text: '[System] Compiling and executing code sandbox...\n' }]);
    setIsRunning(true);
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'run'
      }));
    }
  };

  const clearLogs = () => {
    setLogs([]);
  };

  return {
    code,
    problem,
    language,
    logs,
    isRunning,
    status,
    updateCode,
    updateLanguage,
    updateProblem,
    runCode,
    clearLogs
  };
}
