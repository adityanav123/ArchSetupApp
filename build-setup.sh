#!/bin/bash

VERSION="1.2.0" # Installer Version Number

PACKAGE_DIR="./archsetup_package"
INSTALLER_NAME="archsetup-installer-$VERSION.run"
INSTALLER_TITLE="ArchSetup Installer $VERSION"
EXECUTOR_SCRIPT="archsetup-executor.sh"
CPP_FILE="setup-linux.cpp"
ICON_FILE="archlinux.png"

# Function to clean the package directory
clean_package_directory() {
    echo "Cleaning package directory..."
    rm -rf "$PACKAGE_DIR"
    mkdir "$PACKAGE_DIR"
    echo "Package directory cleaned."
}

# Function to copy necessary files into the package directory
copy_files_to_package() {
    echo "Copying files to package directory..."
    cp "$CPP_FILE" "$PACKAGE_DIR/"
    cp "$EXECUTOR_SCRIPT" "$PACKAGE_DIR/"
    cp "$ICON_FILE" "$PACKAGE_DIR/"
    chmod +x "$PACKAGE_DIR/$EXECUTOR_SCRIPT"
    echo "Files copied to package directory."
}

# Function to create the installer using makeself
create_installer() {
    echo "Creating installer..."
    makeself --nox11 "$PACKAGE_DIR" "$INSTALLER_NAME" "$INSTALLER_TITLE" "./$EXECUTOR_SCRIPT"
    if [ $? -ne 0 ]; then
        echo "Failed to create the installer."
        exit 1
    fi
    echo "Installer $INSTALLER_NAME created successfully."
}

cleanup() {
    echo "Cleaning up package directory..."
    rm -rf "$PACKAGE_DIR"
    echo "Cleanup complete."
}

main() {
    clean_package_directory
    copy_files_to_package
    create_installer
    cleanup
}

# MAIN()
main
