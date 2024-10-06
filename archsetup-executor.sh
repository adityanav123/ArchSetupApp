#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BINARY_NAME="archsetup"
BINARY_PATH="/usr/local/bin/$BINARY_NAME"
LAUNCHER_PATH="$HOME/.local/share/applications/${BINARY_NAME}.desktop"
LAUNCHER_SCRIPT_PATH="$HOME/.local/bin/${BINARY_NAME}-launcher.sh"
CPP_FILE_PATH="$SCRIPT_DIR/setup-linux.cpp"
ICON_NAME="archlinux.png"
ICON_PATH="$HOME/.local/share/icons/$ICON_NAME"

request_sudo_access() {
    echo "Requesting sudo access..."
    sudo -v
}

compile_binary() {
    echo "Compiling $BINARY_NAME..."
    rm -rf "$BINARY_NAME"

    if ! g++ --std=c++20 -o "$BINARY_NAME" "$CPP_FILE_PATH"; then
        echo "Compilation failed. Aborting installation."
        exit 1
    fi
    echo "Compilation successful."
}

install_binary() {
    echo "Installing $BINARY_NAME to $BINARY_PATH..."
    sudo mv "$BINARY_NAME" "$BINARY_PATH"
    sudo chmod +x "$BINARY_PATH"
    echo "Binary installed successfully."
}

create_directories() {
    echo "Creating necessary directories..."
    mkdir -p "$(dirname "$LAUNCHER_PATH")"
    mkdir -p "$(dirname "$LAUNCHER_SCRIPT_PATH")"
    mkdir -p "$(dirname "$ICON_PATH")"
}

install_icon() {
    echo "Installing icon to $ICON_PATH..."
    cp "$SCRIPT_DIR/$ICON_NAME" "$ICON_PATH"
}

create_launcher_script() {
    echo "Creating launcher script at $LAUNCHER_SCRIPT_PATH..."
    cat <<EOL >"$LAUNCHER_SCRIPT_PATH"
#!/bin/bash
# This script launches the $BINARY_NAME binary in a new terminal

gnome-terminal -- bash -c "$BINARY_PATH; exec bash"
EOL
    chmod +x "$LAUNCHER_SCRIPT_PATH"
    echo "Launcher script created successfully."
}

create_desktop_entry() {
    echo "Creating desktop entry at $LAUNCHER_PATH..."
    cat <<EOL >"$LAUNCHER_PATH"
[Desktop Entry]
Name=Arch Setup
Comment=Run Arch Setup Script
Exec=$LAUNCHER_SCRIPT_PATH
Icon=$ICON_PATH
Terminal=false
Type=Application
Categories=Utility;
EOL
    echo "Desktop entry created successfully."

    # Set file permissions
    chmod +x "$LAUNCHER_PATH"
    echo "Set executable permissions on $LAUNCHER_PATH."

    # Validate the desktop entry

    if ! desktop-file-validate "$LAUNCHER_PATH"; then
        echo "Desktop entry validation successful."
    else
        echo "Desktop entry validation failed."
    fi
}

install_app() {
    remove_app
    compile_binary
    install_binary
    create_directories
    install_icon
    create_launcher_script
    create_desktop_entry
    echo "Installation complete. You can run '$BINARY_NAME' from the terminal or use the Arch Setup application from your desktop."
}

remove_app() {
    echo "Removing any previous $BINARY_NAME installation..."

    # Remove binary
    if [ -f "$BINARY_PATH" ]; then
        sudo rm "$BINARY_PATH"
        echo "Removed $BINARY_PATH"
    fi

    # Remove launcher script
    if [ -f "$LAUNCHER_SCRIPT_PATH" ]; then
        rm "$LAUNCHER_SCRIPT_PATH"
        echo "Removed $LAUNCHER_SCRIPT_PATH"
    fi

    # Remove .desktop entry
    if [ -f "$LAUNCHER_PATH" ]; then
        rm "$LAUNCHER_PATH"
        echo "Removed $LAUNCHER_PATH"
    fi

    # Remove icon
    if [ -f "$ICON_PATH" ]; then
        rm "$ICON_PATH"
        echo "Removed $ICON_PATH"
    fi

    echo "Removal complete."
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
