# Gomoku Backend Deployment Guide (pbrain-rapfi)

This guide explains how to deploy your Gomoku backend to Render using Docker with the pbrain-rapfi AI engine.

## Prerequisites

1. A Render account (sign up at [render.com](https://render.com))
2. Your code pushed to a Git repository (GitHub, GitLab, or Bitbucket)
3. Ensure `pbrain-rapfi` executable is committed to your repository at `src/api/ai/pbrain-rapfi`

## Deployment Steps

### 1. Connect Your Repository

1. Log in to your Render dashboard
2. Click "New +" and select "Web Service"
3. Connect your Git repository containing this code

### 2. Configure the Service

Use these settings when creating your web service:

- **Name**: `gomoku-backend` (or your preferred name)
- **Environment**: `Docker`
- **Region**: Choose your preferred region (e.g., Oregon)
- **Branch**: `main` (or your default branch)
- **Dockerfile Path**: `./Dockerfile`

### 3. Environment Variables

The following environment variables are automatically configured:

- `PYTHONUNBUFFERED`: `1` (for proper logging)
- `PORT`: Automatically set by Render

### 4. Health Check

Set the health check path to: `/api/`

## Files Included

- `Dockerfile`: Optimized Docker container for Linux pbrain-rapfi executable
- `render.yaml`: Optional configuration file for Infrastructure as Code deployment
- `.dockerignore`: Excludes unnecessary files from the Docker build
- `test_docker.sh`: Local testing script

## Technical Details

### FastAPI with Uvicorn

Your application runs using uvicorn with the following configuration:
- Host: `0.0.0.0` (accepts connections from any IP)
- Port: Uses Render's `PORT` environment variable (or defaults to 8000)
- Command: `uvicorn main:app --host 0.0.0.0 --port ${PORT:-8000}`

### pbrain-rapfi AI Engine

✅ **Native Linux Execution**: pbrain-rapfi is a Linux ELF executable that runs natively on Render
✅ **No Wine Required**: Much better performance compared to Windows executables
✅ **Optimized Dependencies**: Minimal Docker image with only required system libraries
✅ **Proper Permissions**: Executable permissions are set during Docker build

### Performance Benefits

- 🚀 **Native Performance**: No emulation overhead
- 📦 **Smaller Image**: No Wine dependencies = faster builds and deployments
- 💾 **Lower Memory Usage**: More efficient than Wine-based solutions
- ⚡ **Faster Startup**: Direct executable execution

## Local Testing

Test your Docker setup locally before deploying:

```bash
# Make the script executable (Linux/Mac)
chmod +x test_docker.sh

# Run the test
./test_docker.sh
```

Or manually:
```bash
# Build the image
docker build -t gomoku-backend .

# Run the container
docker run -p 8000:8000 -e PORT=8000 gomoku-backend
```

Then test the endpoints:
- Health check: http://localhost:8000/api/
- AI move: POST to http://localhost:8000/api/ai/move

## Deployment with render.yaml (Optional)

If you prefer Infrastructure as Code, you can use the included `render.yaml`:

1. Place `render.yaml` in your repository root
2. In Render dashboard, go to "Blueprint" and connect your repository
3. Render will automatically deploy based on the YAML configuration

## Testing Your Deployment

Once deployed, test your API endpoints:

- Health check: `https://your-app-name.onrender.com/api/`
- AI move endpoint: `https://your-app-name.onrender.com/api/ai/move`

Example AI move request:
```json
{
  "command": "START 15"
}
```

## About pbrain-rapfi

pbrain-rapfi is a high-performance Gomoku/Renju AI engine that implements the Piskvork protocol. It's commonly used in competitive AI tournaments and provides excellent gameplay.

### Supported Commands
- `START <board_size>`: Initialize a new game
- `TURN <x>,<y>`: Make a move at coordinates (x,y)
- `BEGIN`: Start as the first player
- `BOARD`: Set up board state
- `INFO`: Get engine information

## Troubleshooting

### Common Issues

1. **Executable not found**:
   - Ensure `pbrain-rapfi` is committed to your repository
   - Check that the file is in `src/api/ai/pbrain-rapfi`
   - Verify file permissions are set correctly

2. **Permission denied**:
   - The Dockerfile automatically sets executable permissions
   - If issues persist, check your Git repository settings

3. **Memory limits**: 
   - Render's free tier has memory limits
   - pbrain-rapfi is generally memory-efficient
   - Consider upgrading to a paid plan if needed

4. **Port binding issues**:
   - The Dockerfile uses Render's PORT environment variable
   - Local testing uses port 8000

### Logs

Check your service logs in the Render dashboard for:
- FastAPI/uvicorn startup logs
- pbrain-rapfi initialization messages
- AI move processing logs

### Debug Mode

To enable more verbose logging, you can:
- Add logging to your Python code
- Check the AI executable's stderr output
- Monitor the subprocess communication

## Performance Optimization

For optimal performance:

1. **Keep processes alive**: The current implementation reuses AI processes per game session
2. **Monitor memory**: Clean up inactive games after 5 minutes
3. **Use appropriate instance size**: Consider upgrading Render plan for high traffic

## Support

If you encounter issues:

1. Check the Render service logs for detailed error messages
2. Test locally using the provided `test_docker.sh` script
3. Verify all files are properly committed to your repository
4. Ensure the `pbrain-rapfi` executable is included and has correct permissions

## Why pbrain-rapfi Works Great on Render

✅ **Linux native**: No compatibility layers needed
✅ **Battle-tested**: Proven AI engine used in competitions
✅ **Resource efficient**: Optimized for server environments
✅ **Protocol compliant**: Follows standard Piskvork protocol
✅ **Fast deployment**: Simple Docker setup without complex dependencies 