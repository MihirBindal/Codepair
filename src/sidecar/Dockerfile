FROM python:3.12-alpine

# Install Docker CLI
RUN apk add --no-cache docker-cli

# Install FastAPI and Uvicorn
RUN pip install --no-cache-dir fastapi uvicorn pydantic

WORKDIR /app
COPY main.py /app/main.py

EXPOSE 8000
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
