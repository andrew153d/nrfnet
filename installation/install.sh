#!/bin/bash

# Ensure the script is run with sudo
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit
fi

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Install required packages
echo "Installing required packages..."
sudo apt-get install -y git cmake build-essential libtclap-dev pkg-config

# Clone the repository
echo "Cloning the repository..."
git clone https://github.com/andrew153d/nrfnet.git ~/nrfnet || {
  echo "Repository already exists in home directory. Skipping clone."
}
# Change to the repository directory
cd ~/nrfnet || exit

# Run the NRF24 installation script
echo "Running NRF24 installation script..."
chmod +x installation/nrfinstall.sh
./installation/nrfinstall.sh

# Build the project
echo "Building the project..."
mkdir -p build
cd build
cmake ..
make -j$(nproc)
make install

cd $SCRIPT_DIR

# Create default configuration directory and file
CONFIG_DIR="/etc/nrfnet"
CONFIG_FILE="$CONFIG_DIR/nrfnet.conf"

if [ ! -d "$CONFIG_DIR" ]; then
    echo "Creating configuration directory: $CONFIG_DIR"
    sudo mkdir -p "$CONFIG_DIR"
fi

# Copy the default configuration file to /etc/nrfnet
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Copying default configuration file to: $CONFIG_FILE"
    sudo cp "$SCRIPT_DIR/nrfnet.conf" "$CONFIG_FILE"
else
    echo "Configuration file already exists: $CONFIG_FILE"
fi

# Create a systemd service file
SERVICE_FILE="/etc/systemd/system/nrfnet.service"

if [ ! -f "$SERVICE_FILE" ]; then
    echo "Creating systemd service file: $SERVICE_FILE"
    cat <<EOL | sudo tee $SERVICE_FILE
[Unit]
Description=NRFNet Service
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/nerfnet
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOL

    # Reload systemd to recognize the new service
    sudo systemctl daemon-reload

    # Enable the service to start on boot
    sudo systemctl enable nrfnet.service

    echo "Systemd service created and enabled."
else
    echo "Systemd service file already exists: $SERVICE_FILE"
fi

rm -rf nrfnet

echo "Installation complete!"