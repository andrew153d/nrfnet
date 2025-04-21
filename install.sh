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
git clone https://github.com/andrew153d/nrfmesh.git || {
  echo "Repository already exists. Skipping clone."
}
# Change to the repository directory
cd nrfmesh || exit

# Run the NRF24 installation script
echo "Running NRF24 installation script..."
chmod +x nrfinstall.sh
./nrfinstall.sh

# Build the project
echo "Building the project..."
mkdir -p build
cd build
cmake ..
make -j$(nproc)
make install

cd $SCRIPT_DIR

# Create default configuration directory and file
CONFIG_DIR="/etc/nrfmesh"
CONFIG_FILE="$CONFIG_DIR/nrfmesh.conf"

if [ ! -d "$CONFIG_DIR" ]; then
    echo "Creating configuration directory: $CONFIG_DIR"
    sudo mkdir -p "$CONFIG_DIR"
fi

# Copy the default configuration file to /etc/nrfmesh
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Copying default configuration file to: $CONFIG_FILE"
    sudo cp "$SCRIPT_DIR/nrfmesh.conf" "$CONFIG_FILE"
else
    echo "Configuration file already exists: $CONFIG_FILE"
fi

rm -rf nrfmesh

echo "Installation complete!"