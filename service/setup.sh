#!/bin/bash

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

if [ "$EUID" -ne 0 ]; then 
    log_error "This script must be run as root (use sudo)"
    exit 1
fi

if ! command -v docker &> /dev/null; then
    log_error "Docker is not installed or not in PATH"
    exit 1
fi

if ! docker ps -a --format '{{.Names}}' | grep -q "^fpvjp-app$"; then
    log_error "Docker container 'fpvjp-app' not found"
    exit 1
fi

log_info "Checking required files..."
if [ ! -f ../server-ca-cert.pem ]; then
    log_error "Required file 'server-ca-cert.pem' not found in parent directory"
    exit 1
fi

if [ ! -f ../vtx ]; then
    log_error "Required file 'vtx' not found in parent directory"
    exit 1
fi

log_info "All required files found"

log_info "Starting VTX installation..."
if [ -f /opt/vtx/vtx ]; then
    log_warn "VTX is already installed. This will overwrite existing files."
    read -p "Continue? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "Installation cancelled."
        exit 0
    fi
fi

log_info "Creating /opt/vtx directory..."
mkdir -p /opt/vtx

log_info "Copying server certificate..."
if ! cp ../server-ca-cert.pem /opt/vtx/; then
    log_error "Failed to copy 'server-ca-cert.pem'"
    exit 1
fi

log_info "Copying VTX binary..."
if ! cp ../vtx /opt/vtx/; then
    log_error "Failed to copy vtx binary"
    exit 1
fi

log_info "Setting executable permission..."
chmod +x /opt/vtx/vtx

log_info "Copying environment file..."
if ! cp ./vtx.env /etc/; then
    log_error "Failed to copy 'vtx.env'"
    exit 1
fi

log_info "Updating /etc/hosts with fpv hostname..."
if grep -q "^127\.0\.0\.1.*\bfpv\b" /etc/hosts; then
    log_info "fpv hostname already exists in /etc/hosts"
else
    if grep -q "^127\.0\.0\.1" /etc/hosts; then
        sed -i '/^127\.0\.0\.1/ s/$/ fpv/' /etc/hosts
        log_info "Added fpv to existing 127.0.0.1 entry"
    else
        echo "127.0.0.1 localhost fpv" >> /etc/hosts
        log_info "Added new 127.0.0.1 entry with fpv"
    fi
fi

log_info "Copying systemd service file..."
if ! cp ./vtx.service /etc/systemd/system/; then
    log_error "Failed to copy 'vtx.service'"
    exit 1
fi

log_info "Reloading systemd daemon..."
systemctl daemon-reload

log_info "Enabling VTX service..."
systemctl enable vtx.service

log_info "Starting VTX service..."
if systemctl start vtx.service; then
    log_info "VTX service started successfully"
else
    log_error "Failed to start VTX service"
    systemctl status vtx.service
    exit 1
fi

### Service Info ##################

echo ""

log_info "Service status:"
systemctl status vtx.service --no-pager

echo ""

log_info "Installation completed successfully!"

echo ""

log_info "Useful commands:"
echo "  sudo systemctl status vtx.service   # Check status"
echo "  sudo systemctl stop vtx.service     # Stop service"
echo "  sudo systemctl disable vtx.service  # Disable service"
echo "  sudo systemctl restart vtx.service  # Restart service"
echo "  sudo journalctl -u vtx.service -f   # View logs"
