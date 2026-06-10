import { useEffect, useState, useRef } from 'react';

const CODE_TEMPLATES = {
  cpp: `#include <iostream>\nusing namespace std;\n\nint main() {\n    // Write your C++ code here\n    cout << "Hello, World!" << endl;\n    return 0;\n}\n`,
  python: `def solve():\n    # Write your Python 3 code here\n    print("Hello, World!")\n\nif __name__ == "__main__":\n    solve()\n`,
  javascript: `function solve() {\n    // Write your JavaScript code here\n    console.log("Hello, World!");\n}\n\nsolve();\n`
};

export default function useCollaboration(roomId, role) {
  const [code, setCode] = useState('');
  const [codes, setCodes] = useState({ cpp: '', python: '', javascript: '' });
  const [version, setVersion] = useState(0);
  const [problem, setProblem] = useState('');
  const [language, setLanguage] = useState('cpp');
  const [logs, setLogs] = useState([]);
  const [isRunning, setIsRunning] = useState(false);
  const [status, setStatus] = useState('connecting');

  const wsRef = useRef(null);
  const versionRef = useRef(0);
  const languageRef = useRef('cpp');

  // Sync refs to state to prevent stale closures
  useEffect(() => {
    versionRef.current = version;
  }, [version]);

  useEffect(() => {
    languageRef.current = language;
  }, [language]);

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
            let initialCode = msg.code || '';
            const initialLang = msg.language || 'cpp';
            
            // Try loading codes map from sessionStorage
            let loadedCodes = { cpp: '', python: '', javascript: '' };
            try {
              const saved = sessionStorage.getItem(`codepair_codes_${roomId}`);
              if (saved) {
                loadedCodes = JSON.parse(saved);
              }
            } catch (e) {
              console.error(e);
            }
            
            if (initialCode.trim()) {
              loadedCodes[initialLang] = initialCode;
            } else {
              initialCode = loadedCodes[initialLang] || CODE_TEMPLATES[initialLang] || '';
              loadedCodes[initialLang] = initialCode;
              
              // Sync the auto-generated template back to the server
              const nextVersion = (msg.version || 0) + 1;
              if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                  type: 'edit',
                  code: initialCode,
                  version: nextVersion,
                  language: initialLang
                }));
              }
              setVersion(nextVersion);
            }
            
            setCodes(loadedCodes);
            setCode(initialCode);
            setLanguage(initialLang);
            setProblem(msg.problem || '');
            sessionStorage.setItem(`codepair_codes_${roomId}`, JSON.stringify(loadedCodes));
            break;
            
          case 'edit':
            // Only apply edit if the incoming version is strictly newer than ours
            if (msg.version > versionRef.current) {
              const newCode = msg.code || '';
              const editLang = msg.language || languageRef.current;
              
              setCode(newCode);
              setVersion(msg.version);
              
              setCodes((prev) => {
                const next = { ...prev, [editLang]: newCode };
                sessionStorage.setItem(`codepair_codes_${roomId}`, JSON.stringify(next));
                return next;
              });
            }
            break;
            
          case 'language':
            const nextLang = msg.language;
            setLanguage(nextLang);
            
            // Peer switched active language. Load code for that language
            setCodes((prev) => {
              let nextCode = prev[nextLang];
              if (!nextCode || !nextCode.trim()) {
                nextCode = CODE_TEMPLATES[nextLang] || '';
              }
              setCode(nextCode);
              const updated = { ...prev, [nextLang]: nextCode };
              sessionStorage.setItem(`codepair_codes_${roomId}`, JSON.stringify(updated));
              return updated;
            });
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

    // Save to codes map locally
    setCodes((prev) => {
      const next = { ...prev, [languageRef.current]: newCode };
      sessionStorage.setItem(`codepair_codes_${roomId}`, JSON.stringify(next));
      return next;
    });

    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'edit',
        code: newCode,
        version: nextVersion,
        language: languageRef.current
      }));
    }
  };

  const updateLanguage = (newLanguage) => {
    setLanguage(newLanguage);
    
    let nextCode = '';
    setCodes((prev) => {
      nextCode = prev[newLanguage];
      if (!nextCode || !nextCode.trim()) {
        nextCode = CODE_TEMPLATES[newLanguage] || '';
      }
      setCode(nextCode);
      
      const nextVersion = versionRef.current + 1;
      setVersion(nextVersion);
      
      if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
        // Send language switch first
        wsRef.current.send(JSON.stringify({
          type: 'language',
          language: newLanguage
        }));
        
        // Then send the code edit with the new language property
        wsRef.current.send(JSON.stringify({
          type: 'edit',
          code: nextCode,
          version: nextVersion,
          language: newLanguage
        }));
      }
      
      const updated = { ...prev, [newLanguage]: nextCode };
      sessionStorage.setItem(`codepair_codes_${roomId}`, JSON.stringify(updated));
      return updated;
    });
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
