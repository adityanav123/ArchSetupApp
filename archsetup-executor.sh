#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BINARY_NAME="arch-setup"
BINARY_PATH="/usr/local/bin/$BINARY_NAME"
LAUNCHER_PATH="$HOME/.local/share/applications/${BINARY_NAME}.desktop"
LAUNCHER_SCRIPT_PATH="$HOME/.local/bin/${BINARY_NAME}-launcher.sh"
CPP_FILE_PATH="$SCRIPT_DIR/setup-linux.cpp"
ICON_NAME="archlinux.png"
ICON_PATH="$HOME/.local/share/icons/$ICON_NAME"

log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1"
}

request_sudo_access() {
    log "Requesting sudo access..."
    sudo -v
}

compile_binary() {
    log "Compiling $BINARY_NAME using Makefile..."

    # Ensure we're in the correct directory
    cd "$SCRIPT_DIR" || {
        log "Failed to change to script directory."
        exit 1
    }

    # Check if Makefile exists
    if [[ ! -f "Makefile" ]]; then
        log "Error: Makefile not found in $SCRIPT_DIR"
        exit 1
    fi

    # Run make
    if ! make; then
        log "Compilation failed. Error output:"
        make 2>&1 | tee compile_error.log
        log "Compilation error log saved to compile_error.log"
        log "Aborting installation."
        exit 1
    fi

    # Check if the binary was created and is executable
    if [[ ! -x "arch-setup" ]]; then
        log "Binary 'arch-setup' was not created or is not executable. Aborting installation."
        exit 1
    fi

    log "Compilation successful. Binary details:"
    file "arch-setup"

    log "Binary dependencies:"
    ldd "arch-setup" || log "ldd command failed or not available."

    log "Compilation complete."
}

install_binary() {
    log "Installing $BINARY_NAME to $BINARY_PATH..."
    sudo mv "$BINARY_NAME" "$BINARY_PATH"
    sudo chmod 755 "$BINARY_PATH"
    log "Binary installed successfully."
}

create_directories() {
    log "Creating necessary directories..."
    mkdir -p "$(dirname "$LAUNCHER_PATH")"
    mkdir -p "$(dirname "$LAUNCHER_SCRIPT_PATH")"
    mkdir -p "$(dirname "$ICON_PATH")"
}

install_icon() {
    log "Installing icon to $ICON_PATH..."
    cp "$SCRIPT_DIR/$ICON_NAME" "$ICON_PATH"
    chmod 644 "$ICON_PATH"
}

create_launcher_script() {
    log "Creating launcher script at $LAUNCHER_SCRIPT_PATH..."
    cat <<EOL >"$LAUNCHER_SCRIPT_PATH"
#!/bin/bash
# This script launches the $BINARY_NAME binary in a new terminal

gnome-terminal -- bash -c "sudo $BINARY_PATH; exec bash"
EOL
    chmod 755 "$LAUNCHER_SCRIPT_PATH"
    log "Launcher script created successfully."
}

create_desktop_entry() {
    log "Creating desktop entry at $LAUNCHER_PATH..."
    cat <<EOL >"$LAUNCHER_PATH"
[Desktop Entry]
Name=Arch Setup
Comment=Run Arch Setup Script
Exec=pkexec $LAUNCHER_SCRIPT_PATH
Icon=$ICON_PATH
Terminal=false
Type=Application
Categories=System;
EOL
    chmod 644 "$LAUNCHER_PATH"
    log "Desktop entry created successfully."

    if desktop-file-validate "$LAUNCHER_PATH"; then
        log "Desktop entry validation successful."
    else
        log "Desktop entry validation failed."
    fi

    update-desktop-database "$HOME/.local/share/applications"
}

install_app() {
    remove_app
    compile_binary
    install_binary
    create_directories
    install_icon
    create_launcher_script
    create_desktop_entry
    log "Installation complete. You can run '$BINARY_NAME' from the terminal or use the Arch Setup application from your desktop."
}

remove_app() {
    log "Removing any previous $BINARY_NAME installation..."

    # Remove binary
    if [ -f "$BINARY_PATH" ]; then
        sudo rm "$BINARY_PATH"
        log "Removed $BINARY_PATH"
    fi

    # Remove launcher script
    if [ -f "$LAUNCHER_SCRIPT_PATH" ]; then
        rm "$LAUNCHER_SCRIPT_PATH"
        log "Removed $LAUNCHER_SCRIPT_PATH"
    fi

    # Remove .desktop entry
    if [ -f "$LAUNCHER_PATH" ]; then
        rm "$LAUNCHER_PATH"
        log "Removed $LAUNCHER_PATH"
    fi

    # Remove icon
    if [ -f "$ICON_PATH" ]; then
        rm "$ICON_PATH"
        log "Removed $ICON_PATH"
    fi

    log "Removal complete."
}

main() {
    request_sudo_access
    if [[ "$1" == "--remove" ]]; then
        remove_app
    else
        install_app
    fi
}

# MAIN()
main "$@"
