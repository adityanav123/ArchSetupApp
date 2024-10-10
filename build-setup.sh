#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status

VERSION="2.0" # Installer Version Number

PACKAGE_DIR="./archsetup_package"
INSTALLER_NAME="archsetup-installer-$VERSION.run"
INSTALLER_TITLE="ArchSetup Installer $VERSION"
EXECUTOR_SCRIPT="archsetup-executor.sh"
CPP_FILE="setup-linux.cpp"
HPP_FILE="setup-linux.hpp"
ICON_FILE="archlinux.png"
MAKEFILE="Makefile"

# Function to install prerequisites
install_prerequisites() {
    echo "Installing prerequisites..."

    # Install makeself via yay
    if ! command -v makeself &>/dev/null; then
        echo "makeself is not installed. Installing via yay..."
        if ! command -v yay &>/dev/null; then
            echo "yay AUR helper is not installed. Installing yay first..."
            sudo pacman -S --noconfirm --needed base-devel git
            git clone https://aur.archlinux.org/yay.git /tmp/yay_install
            cd /tmp/yay_install && makepkg -si --noconfirm
            cd - || exit # Return to the previous directory
            rm -rf /tmp/yay_install
        fi
        yay -S --noconfirm makeself
    else
        echo "makeself is already installed."
    fi

    sudo pacman -S --noconfirm --needed clang llvm base-devel

    echo "Prerequisites installed."
}

# Function to clean the package directory
clean_package_directory() {
    echo "Cleaning package directory..."
    rm -rf "$PACKAGE_DIR"
    mkdir -p "$PACKAGE_DIR"
    echo "Package directory cleaned."
}

# Function to copy necessary files into the package directory
copy_files_to_package() {
    echo "Copying files to package directory..."
    cp "$CPP_FILE" "$PACKAGE_DIR/"
    cp "$HPP_FILE" "$PACKAGE_DIR/"
    cp "$EXECUTOR_SCRIPT" "$PACKAGE_DIR/"
    cp "$ICON_FILE" "$PACKAGE_DIR/"
    cp "$MAKEFILE" "$PACKAGE_DIR/"
    chmod +x "$PACKAGE_DIR/$EXECUTOR_SCRIPT"
    echo "Files copied to package directory."
}

create_makefile() {
    echo "Creating Makefile..."
    cat >"$MAKEFILE" <<EOL
# Compiler
CXX := clang++

# Compiler flags
CXXFLAGS := -std=c++20 -Wall -Wextra -pedantic

# Linker flags
LDFLAGS := -lstdc++fs

# Source files
SOURCES := setup-linux.cpp

# Object files
OBJECTS := \$(SOURCES:.cpp=.o)

# Executable name
EXECUTABLE := arch-setup

# Default target
all: \$(EXECUTABLE)

# Rule to build the executable
\$(EXECUTABLE): \$(OBJECTS)
	\$(CXX) \$(OBJECTS) -o \$@ \$(LDFLAGS)

# Rule to compile source files to object files
%.o: %.cpp
	\$(CXX) \$(CXXFLAGS) -c \$< -o \$@

# Clean target
clean:
	rm -f \$(OBJECTS) \$(EXECUTABLE)

# Phony targets
.PHONY: all clean
EOL
    echo "Makefile created."
}

# Function to create the installer using makeself
create_installer() {
    echo "Creating installer..."

    if ! makeself --nox11 "$PACKAGE_DIR" "$INSTALLER_NAME" "$INSTALLER_TITLE" "./$EXECUTOR_SCRIPT"; then
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
    install_prerequisites
    clean_package_directory
    create_makefile
    copy_files_to_package
    create_installer
    cleanup
}

# MAIN()
main
