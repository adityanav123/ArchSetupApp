# ArchSetupApp
Application for setting up Arch &amp; Arch Based distros: For personal use

## Features

- **Shell Setup**: Configure Zsh as the default shell with syntax highlighting.
- **Developer Tools**: Install essential tools for development like `git`, `base-devel`, and more.
- **Gaming Setup**: Set up a complete gaming environment with `Lutris`, `Steam`, `wine-staging`, and other dependencies for NVIDIA and Intel GPU systems.
- **LunarVim & Doom Emacs Setup**: Easy installation of LunarVim and Doom Emacs editors.
- **AUR Helper Installation**: Set up `yay` for easy installation of AUR packages.
- **Terminal Setup**: Install and configure multiple terminal emulators.
- **Installer Utility**: Generate an installer `.run` file for easy distribution and installation.

## Dependencies

Make sure the following tools are installed before running the scripts:

- **Arch-based Linux Distribution**: The setup is optimized for Arch Linux and its derivatives.
- **Git**: Required for cloning repositories.
- **Base-devel**: For compiling the app (clang/g++ -std=c++20)
- **Makeself**: Required to create the installer `.run` file.

To install `makeself`: [Makeself Website](https://makeself.io/)

## How to Run

1. **Clone the repository**:

    ```bash
    git clone https://github.com/ArchSetupApp.git
    cd ArchSetupApp
    ```

2. **Make the scripts executable**:

    ```bash
    chmod +x build-setup.sh archsetup-executor.sh
    ```

3. **Run the setup script to create the installer**:

    ```bash
    ./build-setup.sh
    ```

   This will generate an installer file called `archsetup-installer-1.2.0.run`.

4. **Run the generated installer**:

    ```bash
    ./archsetup-installer-1.2.0.run
    ```

   The installer will set up the necessary command line utilities and add the program to your system's applications database.

5. **Manual Linux environment setup (optional)**:

    If you want to run the setup manually, use:

    ```bash
    ./archsetup-executor.sh
    ```

## Customization

- **Zsh Customization**: Automatically installs Zsh with syntax highlighting and configures your `.zshrc` for an enhanced terminal experience.
- **Gaming Tools**: Installs essential gaming packages like Proton, Lutris, and NVIDIA/Intel-specific drivers.
- **Developer Tools**: Set up Git, base-devel, and other important packages for your development environment.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
