import os
import asyncio
import json
import uuid
import subprocess
from fastapi import FastAPI, Request
from fastapi.responses import StreamingResponse
from pydantic import BaseModel

app = FastAPI()
# Concurrency limit: max 5 parallel docker containers
semaphore = asyncio.Semaphore(5)

class ExecutionRequest(BaseModel):
    code: str
    language: str
    input: str = ""

async def run_sandbox(request: Request, payload: ExecutionRequest):
    async with semaphore:
        job_id = str(uuid.uuid4())
        container_name = f"codepair-job-{job_id}"
        
        lang = payload.language.lower()
        if lang in ["python", "py"]:
            image = "python:3.10-alpine"
            run_args = ["python3", "-c", "import os; exec(compile(os.environ['CODE'], 'solution.py', 'exec'))"]
            extra_args = ["--read-only"]
        elif lang in ["javascript", "js", "node"]:
            image = "node:18-alpine"
            run_args = ["node", "-e", "eval(process.env.CODE)"]
            extra_args = ["--read-only"]
        elif lang in ["cpp", "c++", "c"]:
            image = "gcc:latest"
            # C++ needs to write source and compile. We extract the code using printenv to prevent shell expansion/injection.
            run_args = ["sh", "-c", "printenv CODE > /tmp/main.cpp && g++ -O3 /tmp/main.cpp -o /tmp/main && chmod +x /tmp/main && /tmp/main"]
            extra_args = ["--tmpfs", "/tmp:rw,exec"]
        else:
            yield json.dumps({"event": "stderr", "data": f"Unsupported language: {payload.language}\n"}) + "\n"
            yield json.dumps({"event": "exit", "exit_code": 1, "timed_out": False}) + "\n"
            return

        cmd = [
            "docker", "run",
            "--name", container_name,
            "--rm",
            "-i",
            "-e", "CODE",
            "--network", "none",
            "--memory", "256m",
            "--cpus", "0.5",
            "--pids-limit", "50",
        ] + extra_args + [image] + run_args

        print(f"[Sidecar] Starting container {container_name} for language: {lang}")
        print(f"[Sidecar] Code payload:\n{payload.code}\n---")
        
        # Populate CODE environment variable for the docker run command to pass into container
        sub_env = os.environ.copy()
        sub_env["CODE"] = payload.code

        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                env=sub_env
            )
        except Exception as e:
            yield json.dumps({"event": "stderr", "data": f"Failed to spawn Docker container: {str(e)}\n"}) + "\n"
            yield json.dumps({"event": "exit", "exit_code": 1, "timed_out": False}) + "\n"
            return

        # Write custom input to container stdin and close it
        try:
            stdin_data = payload.input or ""
            process.stdin.write(stdin_data.encode('utf-8'))
            await process.stdin.drain()
            process.stdin.close()
        except Exception as e:
            print(f"[Sidecar] Error writing to container stdin: {e}")

        queue = asyncio.Queue()

        async def read_stream(stream, event_name):
            try:
                while True:
                    line = await stream.readline()
                    if not line:
                        break
                    await queue.put({"event": event_name, "data": line.decode('utf-8', errors='replace')})
            except Exception as e:
                print(f"[Sidecar] Error reading {event_name}: {e}")

        # Start concurrent stdout and stderr readers
        stdout_task = asyncio.create_task(read_stream(process.stdout, "stdout"))
        stderr_task = asyncio.create_task(read_stream(process.stderr, "stderr"))

        # Monitor execution timeout (5 seconds) and client connection state
        timed_out = False
        exit_code = 0
        start_time = asyncio.get_event_loop().time()
        timeout_limit = 5.0
        
        try:
            # We yield lines from the queue while process is running or queue has items
            while not (stdout_task.done() and stderr_task.done() and queue.empty()):
                # Check timeout limit
                if asyncio.get_event_loop().time() - start_time > timeout_limit:
                    raise asyncio.TimeoutError()

                # Check if client closed connection
                if await request.is_disconnected():
                    print(f"[Sidecar] Client disconnected mid-execution. Terminating container {container_name}...")
                    subprocess.run(["docker", "kill", container_name], capture_output=True)
                    return

                try:
                    # Non-blocking wait for items in queue
                    item = await asyncio.wait_for(queue.get(), timeout=0.1)
                    yield json.dumps(item) + "\n"
                except asyncio.TimeoutError:
                    # Check process state / timeout
                    continue
            
            # Wait for process to exit
            exit_code = await process.wait()
            
        except asyncio.TimeoutError:
            print(f"[Sidecar] Job timed out. Killing container {container_name}...")
            timed_out = True
            exit_code = -1
            # Kill the running container immediately
            subprocess.run(["docker", "kill", container_name], capture_output=True)
            yield json.dumps({"event": "stderr", "data": "Execution Timed Out (5s limit exceeded)\n"}) + "\n"
        except Exception as e:
            print(f"[Sidecar] Exception during run: {e}")
            exit_code = -1
        finally:
            # Cancel tasks
            stdout_task.cancel()
            stderr_task.cancel()
            
            # Ensure container is killed if it is somehow still running
            subprocess.run(["docker", "kill", container_name], capture_output=True)

        yield json.dumps({"event": "exit", "exit_code": exit_code, "timed_out": timed_out}) + "\n"

@app.post("/execute")
async def execute(request: Request, payload: ExecutionRequest):
    return StreamingResponse(run_sandbox(request, payload), media_type="application/x-ndjson")
