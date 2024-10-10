#include "setup-linux.hpp"

// Parse String using delimiter
std::vector<std::string> parse_string(const std::string &input,
                                      char delimiter) {
  std::vector<std::string> result;
  std::stringstream ss(input);
  std::string item;

  while (std::getline(ss, item, delimiter)) {
    result.push_back(item);
  }

  return result;
}

void parsePacmanYayResults(const std::string &result,
                           std::vector<PackageStruct> &matchingPackages,
                           const std::string &source) {
  std::regex pacmanYayPattern(
      R"((\S+)\/(\S+)\s+([\d\.]+-\d+)(\s*\[installed\])?\s*\n\s*(.*))");
  std::smatch match;
  std::string::const_iterator searchStart(result.cbegin());
  while (
      std::regex_search(searchStart, result.cend(), match, pacmanYayPattern)) {
    std::string packageName = match[2].str();
    std::string version = match[3].str();
    std::string description = match[5].str();
    bool installed = match[4].matched;

    matchingPackages.emplace_back(packageName, version, description, source);
    searchStart = match.suffix().first;
  }
}

void parseYayResults(const std::string &result,
                     std::vector<PackageStruct> &matchingPackages) {
  std::istringstream iss(result);
  std::string line;
  std::string currentPackage, currentVersion, currentDescription;

  while (std::getline(iss, line)) {
    if (line.substr(0, 8) == "Package:") {
      if (!currentPackage.empty()) {
        matchingPackages.emplace_back(currentPackage, currentVersion,
                                      currentDescription, "AUR");
        currentPackage = currentVersion = currentDescription = "";
      }
      currentPackage = line.substr(9);
    } else if (line.substr(0, 8) == "Version:") {
      currentVersion = line.substr(9);
    } else if (line.substr(0, 12) == "Description:") {
      currentDescription = line.substr(13);
    }
  }

  // Add the last package if exists
  if (!currentPackage.empty()) {
    matchingPackages.emplace_back(currentPackage, currentVersion,
                                  currentDescription, "AUR");
  }
}

std::string runFlatpakCommand(const std::string &packageName,
                              const std::string &columns) {
  std::string flatpakCommand =
      "flatpak search " + packageName + " --columns=" + columns;
  std::array<char, 128> buffer;
  std::string result;

  FILE *pipe = popen(flatpakCommand.c_str(), "r");
  if (!pipe) {
    std::cerr << "Failed to run flatpak command." << std::endl;
    return "";
  }

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  pclose(pipe);

  return result;
}

void fetchFlatpakDetails(const std::string &packageName,
                         std::vector<PackageStruct> &matchingPackages) {
  std::string nameResult = runFlatpakCommand(packageName, "name");
  std::string descriptionResult = runFlatpakCommand(packageName, "description");
  std::string versionResult = runFlatpakCommand(packageName, "version");

  if (nameResult.find("No matches found") != std::string::npos) {
    return; // Exit if no matches are found
  }

  std::istringstream nameStream(nameResult);
  std::istringstream descriptionStream(descriptionResult);
  std::istringstream versionStream(versionResult);

  std::string nameLine, descriptionLine, versionLine;

  // Collect each field from the respective results
  while (std::getline(nameStream, nameLine) &&
         std::getline(descriptionStream, descriptionLine) &&
         std::getline(versionStream, versionLine)) {
    matchingPackages.emplace_back(nameLine, versionLine, descriptionLine,
                                  "Flatpak");
  }
}

// Parsing flags
bool verboseMode = true; // Default to simplified mode

void parseFlags(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--verbose=0") {
      verboseMode = false;
    }
  }
}

// Progress Bar
void showProgressBar(int totalSteps) {
  int progress = 0;
  while (progress <= totalSteps) {
    std::cout << "[";
    int pos = totalSteps / 10;
    for (int i = 0; i < 15; ++i) {
      if (i < progress / (totalSteps / 10)) {
        std::cout << "-";
      } else {
        std::cout << " ";
      }
    }
    std::cout << "] " << (progress * 100) / totalSteps << "%\r";
    std::cout.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ++progress;
  }
  std::cout << std::endl;
}

void runCommand(const std::string &command) {
  int result = std::system(command.c_str());
  if (result != 0) {
    std::cerr << ERROR_COLOR << "Command failed: " << command << RESET_COLOR
              << std::endl;
  }
}

bool isCommandSuccessful(const std::string &command) {
  return std::system(command.c_str()) == 0;
}

// Check if a package is installed using pacman or yay
bool isPackageInstalled(const std::string &packageName) {
  return isCommandSuccessful("pacman -Q " + packageName +
                             " > /dev/null 2>&1") ||
         isCommandSuccessful("yay -Q " + packageName + " > /dev/null 2>&1");
}

// Install a package with progress bar in non-verbose mode, returns true on
// success
bool installPackageWithProgress(const std::string &packageName,
                                const std::string &extraFlags) {
  std::string pacmanQuietFlag = verboseMode ? "" : "--quiet";
  std::string command = "sudo pacman -S --noconfirm --needed " +
                        pacmanQuietFlag + " " + extraFlags + " " + packageName;

  if (verboseMode) {
    std::cout << INPUT_COLOR << "Installing " << packageName << "..."
              << RESET_COLOR << "\n";
    return isCommandSuccessful(command);
  } else {
    std::system("sudo -v");

    // Show progress bar for non-verbose mode
    std::cout << INPUT_COLOR << "Installing " << packageName << "..."
              << RESET_COLOR << "\n";
    std::thread progressThread(showProgressBar, 150); // Progress bar thread

    // Redirect output to /dev/null
    command += " > /dev/null 2>&1";
    bool success = isCommandSuccessful(command);

    // Wait for the progress bar to finish
    progressThread.join();

    return success;
  }
}

// Install a package using pacman, fallback to yay with optional extra flags,
// returns true on success
bool installPackage(const std::string &packageName,
                    const std::string &extraFlags) {
  std::string pacmanQuietFlag = verboseMode ? "" : "--quiet";
  std::string yayQuietFlag = verboseMode ? "" : "--quiet --sudoloop";

  std::string pacmanCommand = "sudo pacman -S --noconfirm --needed " +
                              pacmanQuietFlag + " " + extraFlags + " " +
                              packageName;
  std::string yayCommand = "yay -S --noconfirm --needed " + yayQuietFlag + " " +
                           extraFlags + " " + packageName;

  if (!verboseMode) {
    // Redirect output to /dev/null in non-verbose mode
    pacmanCommand += " > /dev/null 2>&1";
    yayCommand += " > /dev/null 2>&1";
  }

  if (!isPackageInstalled(packageName)) {
    if (installPackageWithProgress(packageName)) {
      std::cout << SUCCESS_COLOR << packageName
                << " installed successfully via pacman.\n"
                << RESET_COLOR;
      return true;
    }

    if (isCommandSuccessful(yayCommand)) {
      if (!isPackageInstalled(packageName)) {
        std::cerr << ERROR_COLOR << "Failed to install " << packageName
                  << " via yay. Package not found.\n"
                  << RESET_COLOR;
        return false;
      }

      std::cout << SUCCESS_COLOR << packageName
                << " installed successfully via yay.\n"
                << RESET_COLOR;
      return true;
    } else {
      std::cerr << ERROR_COLOR << "Failed to install " << packageName
                << " via both pacman and yay.\n"
                << RESET_COLOR;
      return false;
    }
  } else {
    std::cout << SUCCESS_COLOR << packageName << " is already installed.\n"
              << RESET_COLOR;
    return true;
  }
}

bool downloadFile(const std::string &url, const std::string &outputFilePath) {
  std::string command = "curl -L " + url + " -o " + outputFilePath;
  return isCommandSuccessful(command);
}

// Check if file exists and is not empty
bool isFileValid(const std::string &filePath) {
  std::ifstream file(filePath);
  return file.good() && file.peek() != std::ifstream::traits_type::eof();
}

void applyConfig(const std::string &gistUrl, const std::string &configPath) {
  std::string backupPath = configPath + "_old.bak";
  std::string tempConfigPath = "/tmp/config_gist";

  // Ensure the target directory exists
  fs::path targetDir = fs::path(configPath).parent_path();
  if (!fs::exists(targetDir)) {
    std::cout << INPUT_COLOR
              << "Target directory does not exist. Creating: " << targetDir
              << RESET_COLOR << "\n";
    fs::create_directories(targetDir);
  }

  if (fs::exists(configPath) && isFileValid(configPath)) {
    std::cout << INPUT_COLOR << "Backing up current config..." << RESET_COLOR
              << "\n";
    std::ifstream src(configPath, std::ios::binary);
    std::ofstream dst(backupPath, std::ios::binary);
    dst << src.rdbuf();
    std::cout << SUCCESS_COLOR << "Backup created at: " << backupPath
              << RESET_COLOR << "\n";
  } else {
    std::cout << ERROR_COLOR << "No valid config found to back up.\n"
              << RESET_COLOR;
  }

  std::cout << INPUT_COLOR << "Downloading new config..." << RESET_COLOR
            << "\n";
  if (!downloadFile(gistUrl, tempConfigPath)) {
    std::cerr << ERROR_COLOR << "Failed to download the config from " << gistUrl
              << "\n"
              << RESET_COLOR;
    return;
  }

  if (!isFileValid(tempConfigPath)) {
    std::cerr << ERROR_COLOR << "Downloaded config file is invalid or empty.\n"
              << RESET_COLOR;
    return;
  }

  try {
    std::cout << INPUT_COLOR << "Applying new config..." << RESET_COLOR << "\n";
    std::ifstream src(tempConfigPath, std::ios::binary);
    std::ofstream dst(configPath, std::ios::binary);
    dst << src.rdbuf();
    std::cout << SUCCESS_COLOR << "Configuration applied successfully.\n"
              << RESET_COLOR;
  } catch (const std::exception &e) {
    std::cerr << ERROR_COLOR << "Error applying new config: " << e.what()
              << "\n"
              << RESET_COLOR;
  }

  try {
    fs::remove(tempConfigPath);
  } catch (const fs::filesystem_error &e) {
    std::cerr << ERROR_COLOR << "Failed to remove temporary file: " << e.what()
              << "\n"
              << RESET_COLOR;
  }
}

// Function to install Flatpak and add Flathub repository
void setupFlatpak() {
  // Install flatpak
  installPackage("flatpak", "--needed");

  // Add Flathub repository if not already added
  if (!isCommandSuccessful("flatpak remote-list | grep flathub")) {
    std::cout << INPUT_COLOR << "Adding Flathub repository to Flatpak...\n"
              << RESET_COLOR;
    runCommand("sudo flatpak remote-add --if-not-exists flathub "
               "https://flathub.org/repo/flathub.flatpakrepo");
  }
}

// ZSH & Starship Setup
void setZshAsDefaultShell() {
  if (!isCommandSuccessful("which zsh > /dev/null 2>&1")) {
    std::cout << INPUT_COLOR << "Zsh is not installed. Installing Zsh..."
              << RESET_COLOR << "\n";
    installPackage("zsh", "--needed");
  }

  const char *user = std::getenv("USER");
  if (user == nullptr) {
    std::cerr << ERROR_COLOR
              << "Failed to get the current user. Cannot set Zsh as the "
                 "default shell.\n"
              << RESET_COLOR;
    return;
  }

  std::string command = "chsh -s $(which zsh) " + std::string(user);
  if (isCommandSuccessful(command)) {
    std::cout << SUCCESS_COLOR << "Zsh has been set as the default shell.\n"
              << RESET_COLOR;
  } else {
    std::cerr << ERROR_COLOR << "Failed to set Zsh as the default shell.\n"
              << RESET_COLOR;
  }
}

// Install terminal emulator and apply configuration
void installTerminal(const std::string &terminalName,
                     const std::string &configUrl,
                     const std::string &configPath) {
  if (!installPackage(terminalName, "--needed")) {
    std::cerr << ERROR_COLOR << "Failed to install " << terminalName
              << ". Aborting setup.\n"
              << RESET_COLOR;
    return;
  }

  if (!configUrl.empty() && !configPath.empty()) {
    applyConfig(configUrl, configPath);
    std::cout << SUCCESS_COLOR << terminalName << " configuration applied from "
              << configUrl << ".\n"
              << RESET_COLOR;
  } else {
    std::cout << SUCCESS_COLOR << terminalName
              << " installed with no specific configuration applied.\n"
              << RESET_COLOR;
  }

  std::cout << SUCCESS_COLOR << terminalName << " setup completed.\n"
            << RESET_COLOR;
}

// WezTerm
void setupWezTerm() {
  std::string weztermConfigPath =
      std::string(getenv("HOME")) + "/.config/wezterm/wezterm.lua";
  installTerminal("wezterm",
                  "https://gist.githubusercontent.com/adityanav123/"
                  "dd3031a3dd82b53d36dafdecc58f4257/raw/"
                  "921bbc3b4346f21123cd8a4e6f8657f3b6fbfb64/wezterm.lua",
                  weztermConfigPath);
}

// Kitty
void setupKitty() {
  std::string kittyConfigPath =
      std::string(getenv("HOME")) + "/.config/kitty/kitty.conf";
  installTerminal("kitty",
                  "https://gist.githubusercontent.com/adityanav123/"
                  "8afec13d17c5191bbbfc2f92e632d739/raw/"
                  "c271c161ec0d74506a36900b6f2501c578cd6e18/kitty.conf",
                  kittyConfigPath);
}

void setupTerminal() {
  std::cout << INPUT_COLOR << "Which terminal would you like to install? \n"
            << RESET_COLOR;
  std::cout << OPTION_COLOR << "(1) WezTerm \n(2) Kitty\n" << RESET_COLOR;

  int terminalChoice;
  std::cin >> terminalChoice;

  switch (terminalChoice) {
  case 1:
    std::cout << INPUT_COLOR << "Setting up WezTerm...\n" << RESET_COLOR;
    setupWezTerm();
    break;
  case 2:
    std::cout << INPUT_COLOR << "Setting up Kitty...\n" << RESET_COLOR;
    setupKitty();
    break;
  default:
    std::cout << ERROR_COLOR << "Invalid choice. Please try again.\n"
              << RESET_COLOR;
    break;
  }
}

// Setup ZSH shell and apply .zshrc config
void setupStarshipTheme() {
  std::cout << INPUT_COLOR << "Choose a theme for Starship: \n" << RESET_COLOR;
  std::cout << OPTION_COLOR << "(1) Gruvbox\n(2) Catppuccin Mocha\n"
            << RESET_COLOR;

  int themeChoice;
  std::cin >> themeChoice;

  std::string starshipConfigPath =
      std::string(getenv("HOME")) + "/.config/starship.toml";

  switch (themeChoice) {
  case 1: {
    std::string gruvboxCommand =
        "starship preset gruvbox-rainbow -o " + starshipConfigPath;
    if (isCommandSuccessful(gruvboxCommand)) {
      std::cout << SUCCESS_COLOR << "Gruvbox theme applied to Starship.\n"
                << RESET_COLOR;
    } else {
      std::cerr << ERROR_COLOR << "Failed to apply Gruvbox theme to Starship.\n"
                << RESET_COLOR;
    }
    break;
  }
  case 2: {
    std::string starshipThemePath = "/tmp/catppuccin_starship";
    std::string cloneThemeCommand =
        "git clone https://github.com/catppuccin/starship " + starshipThemePath;

    if (isCommandSuccessful(cloneThemeCommand)) {
      std::string themeFilePath = starshipThemePath + "/themes/mocha.toml";

      std::ifstream themeFile(themeFilePath);
      std::ofstream starshipConfig(starshipConfigPath, std::ios_base::app);

      if (themeFile.is_open() && starshipConfig.is_open()) {
        // Write the palette and theme configuration to starship.toml
        starshipConfig << "\npalette = \"catppuccin_mocha\"\n";
        starshipConfig << themeFile.rdbuf();
        themeFile.close();
        starshipConfig.close();

        std::cout << SUCCESS_COLOR
                  << "Catppuccin Mocha theme applied to Starship.\n"
                  << RESET_COLOR;
      } else {
        std::cerr
            << ERROR_COLOR
            << "Failed to configure Catppuccin Mocha theme for Starship.\n"
            << RESET_COLOR;
      }

      fs::remove_all(starshipThemePath);
    } else {
      std::cerr << ERROR_COLOR
                << "Failed to clone Catppuccin Starship theme repository.\n"
                << RESET_COLOR;
    }
    break;
  }
  default:
    std::cout << ERROR_COLOR << "Invalid choice. No theme applied.\n"
              << RESET_COLOR;
    break;
  }
}

void setupShell() {
  // check yay
  ensureYayInstalled();

  std::vector<std::string> zsh_dependencies{"zsh",
                                            "ttf-recursive",
                                            "ttf-recursive-nerd",
                                            "ttf-firacode-nerd",
                                            "pfetch",
                                            "starship",
                                            "eza"};
  for (const auto &pkg : zsh_dependencies) {
    installPackage(pkg, "--needed");
  }

  // Homebrew Setup
  std::string homebrewInstallCommand =
      "/bin/bash -c \"$(curl -fsSL "
      "https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\"";
  std::cout << INPUT_COLOR << "Installing Homebrew...\n" << RESET_COLOR;
  runCommand(homebrewInstallCommand);

  // Source Homebrew
  std::string sourceHomebrewCommand = "eval $(/opt/homebrew/bin/brew shellenv)";
  if (isCommandSuccessful(sourceHomebrewCommand)) {
    std::cout << SUCCESS_COLOR
              << "Homebrew sourced successfully in this session.\n"
              << RESET_COLOR;
  } else {
    std::cerr
        << ERROR_COLOR
        << "Failed to source Homebrew. You may need to restart the terminal.\n"
        << RESET_COLOR;
  }

  // Install Zsh Syntax Highlighting via Homebrew
  runCommand("brew install zsh-syntax-highlighting");

  // Clone zsh-autosuggestions (already handled in .zshrc)
  std::cout << INPUT_COLOR << "Installing zsh-autosuggestions...\n"
            << RESET_COLOR;
  runCommand("git clone https://github.com/zsh-users/zsh-autosuggestions "
             "~/.zsh/zsh-autosuggestions");

  setZshAsDefaultShell();

  applyConfig("https://gist.githubusercontent.com/adityanav123/"
              "00f0dd587acd1a664e0de5ccf295513e/raw",
              std::string(getenv("HOME")) + "/.zshrc");

  setupStarshipTheme();

  std::cout << SUCCESS_COLOR << ".zshrc updated.\n" << RESET_COLOR;
}

// Gaming environment setup
void gamingSetup() {
  std::cout << INPUT_COLOR << "Installing gaming tools and libraries...\n"
            << RESET_COLOR;

  installPackage("mesa", "--needed");
  installPackage("lib32-mesa", "--needed");

  std::vector<std::string> nvidia_gpu_packages{
      "nvidia",   "nvidia-utils",   "lib32-nvidia-utils",
      "libvdpau", "lib32-libvdpau", "nvidia-settings"};
  for (const auto &pkg : nvidia_gpu_packages)
    installPackage(pkg, "--needed");

  std::vector<std::string> intel_gpu_packages{
      "vulkan-intel", "intel-media-driver", "libva-intel-driver"};
  for (const auto &pkg : intel_gpu_packages)
    installPackage(pkg, "--needed");

  std::vector<std::string> wine_dependencies{
      "giflib",  "lib32-giflib",  "libpng", "lib32-libpng",
      "libldap", "lib32-libldap", "gnutls", "lib32-gnutls"};
  for (const auto &pkg : wine_dependencies)
    installPackage(pkg, "--needed");

  installPackage("protonup-qt", "--needed");

  std::vector<std::string> gaming_tools{
      "lutris", "steam", "gamemode",    "lib32-gamemode", "wine-staging",
      "wine",   "vkd3d", "lib32-vkd3d", "faudio",         "lib32-faudio"};
  for (const auto &pkg : gaming_tools)
    installPackage(pkg, "--needed");

  // Add user to gamemode group
  std::cout << INPUT_COLOR << "Setting up gamemode.\n" << RESET_COLOR;
  runCommand("sudo usermod -aG gamemode $USER");

  // running gamemode test
  std::cout << INPUT_COLOR << "Running gamemode tests.\n" << RESET_COLOR;
  runCommand("gamemoded -t");

  std::cout << SUCCESS_COLOR << "Gaming environment setup complete.\n"
            << RESET_COLOR;
}

// Developer tools setup
void developerSetup() {
  std::cout << INPUT_COLOR << "Installing developer tools...\n" << RESET_COLOR;
  std::vector<std::string> devTools{"git", "neovim", "clang", "llvm",
                                    "gdb", "lldb",   "emacs"};
  for (const auto &pkg : devTools)
    installPackage(pkg);
}

// Setup LunarVim
void setupLVim() {
  std::cout << INPUT_COLOR << "Setting up LunarVim...\n" << RESET_COLOR;
  std::vector<std::string> lvim_dependencies{
      "git",     "make",    "python-pip",    "npm", "nodejs",
      "ripgrep", "lazygit", "python-pynvim", "curl"};
  for (const auto &pkg : lvim_dependencies)
    installPackage(pkg, "--needed");

  // Node.js global setup
  std::cout << INPUT_COLOR << "Setting up npm global directory...\n"
            << RESET_COLOR;
  runCommand("mkdir -p ~/.npm-global/lib");
  runCommand("npm config set prefix '~/.npm-global'");

  // Update system path for npm global directory
  std::string profilePath = std::string(getenv("HOME")) + "/.profile";
  std::ofstream profileFile(profilePath, std::ios_base::app);
  if (profileFile.is_open()) {
    profileFile << "\nexport PATH=~/.npm-global/bin:$PATH\n";
    profileFile.close();
  }
  runCommand("source ~/.profile");

  // Test npm global setup by installing a package
  /* std::cout << INPUT_COLOR << "Testing npm global installation with
   jshint...\n"
             << RESET_COLOR;
   runCommand("npm install -g jshint");
 */
  // Cargo Setup
  std::string cargoInstallCommand =
      "curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y";
  std::cout << INPUT_COLOR << "Installing Rust.\n" << RESET_COLOR;

  runCommand(cargoInstallCommand);

  // Source cargo environment to avoid restart
  std::string cargoEnvPath = std::string(getenv("HOME")) + "/.cargo/env";
  runCommand("source " + cargoEnvPath);

  if (isCommandSuccessful(
          "LV_BRANCH='release-1.4/neovim-0.9' bash <(curl -s "
          "https://raw.githubusercontent.com/LunarVim/LunarVim/release-1.4/"
          "neovim-0.9/utils/installer/install.sh)")) {
    std::cout << SUCCESS_COLOR << "LunarVim installed successfully.\n"
              << RESET_COLOR;
    std::string lVimConfigPath =
        std::string(getenv("HOME")) + "/.config/lvim/config.lua";
    applyConfig("https://gist.githubusercontent.com/adityanav123/"
                "2e708e777628d3914cf59e5d1f332f20/raw",
                lVimConfigPath);
  } else {
    std::cerr << ERROR_COLOR << "Failed to install LunarVim.\n" << RESET_COLOR;
  }
}

// Setup Doom Emacs
void setupDoomEmacs() {
  std::cout << INPUT_COLOR << "Setting up Doom Emacs...\n" << RESET_COLOR;

  installPackage("emacs", "--needed");
  installPackage("git", "--needed");

  std::string emacsConfigPath = std::string(getenv("HOME")) + "/.config/emacs";

  std::string cloneCommand =
      "git clone --depth 1 https://github.com/doomemacs/doomemacs " +
      emacsConfigPath;

  if (isCommandSuccessful(cloneCommand)) {
    std::cout << SUCCESS_COLOR << "Doom Emacs cloned successfully.\n"
              << RESET_COLOR;
  } else {
    std::cerr << ERROR_COLOR << "Failed to clone Doom Emacs.\n" << RESET_COLOR;
    return;
  }

  std::cout << SUCCESS_COLOR << "Starting Doom Install!" << RESET_COLOR
            << std::endl;
  // DOOM INSTALL
  std::string doomInstallCommand = emacsConfigPath + "/bin/doom install";

  if (isCommandSuccessful(doomInstallCommand)) {
    std::cout << SUCCESS_COLOR << "Doom Emacs installed successfully.\n"
              << RESET_COLOR;
  } else {
    std::cerr << ERROR_COLOR << "Failed to install Doom Emacs.\n"
              << RESET_COLOR;
    return;
  }

  std::string doomConfigPath = std::string(getenv("HOME")) + "/.config/doom/";
  std::vector<std::string> filesToRemove = {doomConfigPath + "package.el",
                                            doomConfigPath + "config.el",
                                            doomConfigPath + "init.el"};

  for (const auto &filePath : filesToRemove) {
    if (fs::exists(filePath)) {
      std::cout << INPUT_COLOR << "Removing existing file: " << filePath
                << RESET_COLOR << "\n";
      fs::remove(filePath); // Remove the file
    }
  }

  std::string cloneConfigCommand =
      "git clone https://github.com/adityanav123/MyDoomEmacsSetup " +
      doomConfigPath;

  if (isCommandSuccessful(cloneConfigCommand)) {
    std::cout << SUCCESS_COLOR
              << "Your Doom Emacs configuration cloned successfully.\n"
              << RESET_COLOR;
  } else {
    std::cerr << ERROR_COLOR
              << "Failed to clone your Doom Emacs configuration.\n"
              << RESET_COLOR;
    return;
  }

  // Install 'emms' package
  std::cout << INPUT_COLOR << "Installing emms (Emacs Multimedia package)...\n"
            << RESET_COLOR;
  if (!installPackage("emms")) {
    std::cerr << ERROR_COLOR << "Failed to install emms package.\n"
              << RESET_COLOR;
  }

  // DOOM SYNC
  std::string doomSyncCommand = emacsConfigPath + "/bin/doom sync";
  if (isCommandSuccessful(doomSyncCommand)) {
    std::cout << SUCCESS_COLOR << "Doom Emacs synchronized successfully.\n"
              << RESET_COLOR;
  } else {
    std::cerr << ERROR_COLOR << "Failed to synchronize Doom Emacs.\n"
              << RESET_COLOR;
  }

  std::string shellConfigPath;
  const char *shell = std::getenv("SHELL");
  if (shell && std::string(shell).find("zsh") != std::string::npos) {
    shellConfigPath = std::string(getenv("HOME")) + "/.zshrc";
  } else {
    shellConfigPath = std::string(getenv("HOME")) + "/.bashrc";
  }

  std::ofstream shellConfigFile;
  shellConfigFile.open(shellConfigPath, std::ios_base::app);
  if (shellConfigFile.is_open()) {
    shellConfigFile << "\n# Added by Arch Linux setup script\n";
    shellConfigFile << "export PATH=\"$PATH:" << emacsConfigPath << "/bin\"\n";
    shellConfigFile.close();
    std::cout << SUCCESS_COLOR << "Added Doom Emacs bin directory to PATH in "
              << shellConfigPath << "\n"
              << RESET_COLOR;
  } else {
    std::cerr << ERROR_COLOR
              << "Failed to add Doom Emacs to PATH. Could not open "
              << shellConfigPath << "\n"
              << RESET_COLOR;
  }

  std::cout << INPUT_COLOR << "Doom Emacs setup complete. Useful commands:\n"
            << RESET_COLOR;
  std::cout << OPTION_COLOR
            << "doom sync    - Synchronize your config with Doom Emacs.\n"
            << "doom upgrade - Update Doom Emacs and all packages.\n"
            << "doom doctor  - Diagnose common issues.\n"
            << "doom env     - Regenerate the environment file.\n"
            << RESET_COLOR;
}

// Package Downloader
void ensureYayInstalled() {
  if (!isCommandSuccessful("which yay > /dev/null 2>&1")) {
    std::cout << INPUT_COLOR
              << "The 'yay' AUR helper is not installed. Do you want to "
                 "install it? (y/n): "
              << RESET_COLOR;
    char choice;
    std::cin >> choice;
    if (choice == 'y' || choice == 'Y') {
      std::cout << INPUT_COLOR << "Installing 'yay'...\n" << RESET_COLOR;
      // Install dependencies
      installPackage("base-devel", "--needed");
      installPackage("git", "--needed");

      std::string tmpDir = "/tmp/yay_install";
      runCommand("git clone https://aur.archlinux.org/yay.git " + tmpDir);
      runCommand("cd " + tmpDir + " && makepkg -si --noconfirm");
      runCommand("rm -rf " + tmpDir);

      if (isCommandSuccessful("which yay > /dev/null 2>&1")) {
        std::cout << SUCCESS_COLOR << "'yay' installed successfully.\n"
                  << RESET_COLOR;
      } else {
        std::cerr
            << ERROR_COLOR
            << "Failed to install 'yay'. AUR packages will not be available.\n"
            << RESET_COLOR;
      }
    } else {
      std::cout
          << INPUT_COLOR
          << "Proceeding without 'yay'. AUR packages will not be available.\n"
          << RESET_COLOR;
    }
  }
}

void ensureFlatpakInstalled() {
  if (!isCommandSuccessful("which flatpak > /dev/null 2>&1")) {
    std::cout << INPUT_COLOR
              << "Flatpak is not installed. Do you want to install it to "
                 "search for Flatpak packages? (y/n): "
              << RESET_COLOR;
    char choice;
    std::cin >> choice;
    if (choice == 'y' || choice == 'Y') {
      installPackage("flatpak", "--needed");
      if (isCommandSuccessful("which flatpak > /dev/null 2>&1")) {
        std::cout << SUCCESS_COLOR << "Flatpak installed successfully.\n"
                  << RESET_COLOR;
        if (!isCommandSuccessful(
                "flatpak remote-list | grep flathub > /dev/null 2>&1")) {
          std::cout << INPUT_COLOR
                    << "Adding Flathub repository to Flatpak...\n"
                    << RESET_COLOR;
          runCommand("sudo flatpak remote-add --if-not-exists flathub "
                     "https://flathub.org/repo/flathub.flatpakrepo");
        }
      } else {
        std::cerr << ERROR_COLOR
                  << "Failed to install Flatpak. Flatpak packages will not be "
                     "available.\n"
                  << RESET_COLOR;
      }
    } else {
      std::cout << INPUT_COLOR
                << "Proceeding without Flatpak. Flatpak packages will not be "
                   "available.\n"
                << RESET_COLOR;
    }
  }
}

// Search for Packages
std::vector<PackageStruct> searchForPackages(const std::string &packageName) {
  std::vector<PackageStruct> matchingPackages;

  std::string pacmanCommand = "pacman -Ss " + packageName;
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen(pacmanCommand.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result += buffer.data();
    }
    pclose(pipe);
    parsePacmanYayResults(result, matchingPackages, "pacman");
  }

  std::string yayCommand =
      "yay -Ss \"" + packageName +
      "\" | awk '"
      "/aur\\// {"
      "  split($1, a, \"/\");"
      "  package = a[2];"
      "  version = $2;"
      "  getline;"
      "  description = $0;"
      "  printf \"Package: %s\\nVersion: %s\\nDescription: %s\\n\\n\", "
      "package, version, description;"
      "}'";
  result.clear();
  pipe = popen(yayCommand.c_str(), "r");
  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      result += buffer.data();
    }
    pclose(pipe);
    parseYayResults(result, matchingPackages);
  }

  fetchFlatpakDetails(packageName, matchingPackages);

  return matchingPackages;
}

void downloadPackage() {
  const int ITEMS_PER_PAGE = 10;
  std::string packageName;

  while (true) {
    clearScreen();
    std::cout << MENU_COLOR
              << "=== Package Search and Download ===" << RESET_COLOR << "\n\n";
    std::cout << INPUT_COLOR
              << "Enter the package name you want to search for\n"
              << "(or enter 'q' to return to the main menu): " << RESET_COLOR;

    std::getline(std::cin, packageName);

    if (packageName == "q" || packageName == "Q") {
      std::cout << INPUT_COLOR << "Returning to the main menu...\n"
                << RESET_COLOR;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      return;
    }

    auto matchingPackages = searchForPackages(packageName);

    if (matchingPackages.empty()) {
      std::cout << ERROR_COLOR
                << "No matching packages found for: " << packageName << "\n"
                << RESET_COLOR;
      std::cout << "Press Enter to try again...";
      std::cin.get();
      continue;
    }

    int currentPage = 0;
    int totalPages =
        (matchingPackages.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

    while (true) {
      clearScreen();
      std::cout << MENU_COLOR << "=== Search Results (Page "
                << (currentPage + 1) << " of " << totalPages
                << ") ===" << RESET_COLOR << "\n\n";

      int startIdx = currentPage * ITEMS_PER_PAGE;
      int endIdx = std::min(startIdx + ITEMS_PER_PAGE,
                            static_cast<int>(matchingPackages.size()));

      for (int i = startIdx; i < endIdx; ++i) {
        const auto &pkg = matchingPackages[i];
        bool installed = isPackageInstalled(pkg.name);
        const char *color = installed ? SUCCESS_COLOR : OPTION_COLOR;

        std::cout << (i + 1) << ". " << color << pkg.name << RESET_COLOR
                  << " : " << pkg.version << " (" << MENU_COLOR
                  << pkg.sourceOfPackage << RESET_COLOR << ")\n"
                  << "\t" << pkg.description << "\n";

        if (installed) {
          std::cout << "\t" << SUCCESS_COLOR << "[installed]" << RESET_COLOR
                    << "\n";
        }
        std::cout << "\n";
      }

      std::cout << INPUT_COLOR
                << "Enter package numbers to install (comma-separated),\n"
                << "n for next page, p for previous page, or q to go back: "
                << RESET_COLOR;

      std::string input;
      std::getline(std::cin, input);

      if (input == "q" || input == "Q") {
        break;
      }

      if (input == "n" || input == "N") {
        currentPage = (currentPage + 1) % totalPages;
        continue;
      }

      if (input == "p" || input == "P") {
        currentPage = (currentPage - 1 + totalPages) % totalPages;
        continue;
      }

      std::vector<PackageStruct> selectedPackages;
      std::stringstream ss(input);
      std::string item;
      while (std::getline(ss, item, ',')) {
        try {
          int index = std::stoi(item) - 1;
          if (index >= 0 && index < static_cast<int>(matchingPackages.size())) {
            selectedPackages.push_back(matchingPackages[index]);
          } else {
            std::cout << ERROR_COLOR << "Invalid index: " << (index + 1)
                      << ". Skipping.\n"
                      << RESET_COLOR;
          }
        } catch (const std::invalid_argument &) {
          std::cout << ERROR_COLOR << "Invalid input: " << item
                    << ". Skipping.\n"
                    << RESET_COLOR;
        }
      }

      if (selectedPackages.empty()) {
        std::cout << ERROR_COLOR
                  << "No valid packages selected. Press Enter to continue...\n"
                  << RESET_COLOR;
        std::cin.get();
        continue;
      }

      clearScreen();
      std::cout << MENU_COLOR << "=== Installing Packages ===" << RESET_COLOR
                << "\n\n";
      for (const auto &pkg : selectedPackages) {
        std::cout << INPUT_COLOR << "Installing " << pkg.name << "...\n"
                  << RESET_COLOR;
        installPackage(pkg.name, "--needed");
        std::cout << "\n";
      }

      std::cout << SUCCESS_COLOR
                << "Installation complete. Press Enter to continue...\n"
                << RESET_COLOR;
      std::cin.get();
      break;
    }
  }
}

void askForSudoPassword() {
  std::cout << INPUT_COLOR << "Entering Package Installation Mode...\n"
            << RESET_COLOR;
  if (std::system("sudo -v") != 0) {
    std::cerr << ERROR_COLOR << "Failed to authenticate with sudo. Exiting...\n"
              << RESET_COLOR;
    std::exit(1);
  }
}


// Main menu and input handling
std::vector<std::string> getSimpleMenuDescriptions() {
  return {"Setup Shell (Zsh)",
          "Install Developer Tools",
          "Setup Gaming",
          "Install LunarVim",
          "Install Doom Emacs",
          "Install Terminals",
          "Search & Download a Package [not for yay currently]",
          "Setup Yay (AUR Helper)",
          "Setup Flatpak",
          "Exit"};
}

std::vector<std::string> getDetailedMenuDescriptions() {
  /* CAN BE IMPLEMENTED LATER */
  return getSimpleMenuDescriptions();
}

void displayMenu(const std::vector<MenuItem> &menuItems) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    clearScreen();

    int contentHeight = menuItems.size() + 6;
    int verticalPadding = (w.ws_row - contentHeight) / 2;

    for (int i = 0; i < verticalPadding; ++i) {
        std::cout << "\n";
    }

    printHeader("Arch Linux Setup Menu");

    for (size_t i = 0; i < menuItems.size(); ++i) {
        std::cout << GRUVBOX_YELLOW << " [" << (i + 1) << "] " << RESET_COLOR 
                  << GRUVBOX_FG << menuItems[i].description << RESET_COLOR << "\n";
    }

    printSeparator();
    printPrompt("Choose an option (1-" + std::to_string(menuItems.size()) + "), or [q] to quit");
}

void handleMenuChoice(const std::vector<MenuItem> &menuItems, int choice) {
  if (choice < 1 || choice > static_cast<int>(menuItems.size())) {
    std::cout << ERROR_COLOR << "Invalid option! Please try again.\n"
              << RESET_COLOR;
  } else {
    menuItems[choice - 1].action();
    printSeparator();
  }
}

// YAY
void setupYay() {
  if (isPackageInstalled("yay")) {
    std::cout << SUCCESS_COLOR << "Yay is already installed.\n" << RESET_COLOR;
    return;
  }

  std::cout << INPUT_COLOR << "Installing yay (AUR helper)...\n" << RESET_COLOR;

  // Install yay dependencies
  installPackage("base-devel", "--needed");
  installPackage("git", "--needed");

  // Clone and install yay from the AUR
  std::string tmpDir = "/tmp/yay_install";
  runCommand("git clone https://aur.archlinux.org/yay.git " + tmpDir);
  runCommand("cd " + tmpDir + " && makepkg -si --noconfirm");
  runCommand("rm -rf " + tmpDir);

  if (isPackageInstalled("yay")) {
    std::cout << SUCCESS_COLOR << "Yay installed successfully.\n"
              << RESET_COLOR;
  } else {
    std::cerr << ERROR_COLOR << "Failed to install yay.\n" << RESET_COLOR;
  }
}

// Menus
void setupShellMenu() {
    std::vector<std::pair<std::string, std::function<void()>>> options = {
        {"Setup Zsh and dependencies", setupShell},
        {"Configure Starship theme", setupStarshipTheme}
    };

    while (true) {
        clearScreen();
        printHeader("Setup Shell (Zsh)");

        for (size_t i = 0; i < options.size(); ++i) {
            std::cout << GRUVBOX_YELLOW << " [" << (i + 1) << "] " << RESET_COLOR 
                      << GRUVBOX_FG << options[i].first << RESET_COLOR << "\n";
        }

        printSeparator();
        printPrompt("Choose an option (1-" + std::to_string(options.size()) + "), or [q] to go back");

        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "q" || choice == "Q") {
            return;
        }

        try {
            int choiceNum = std::stoi(choice);
            if (choiceNum > 0 && choiceNum <= static_cast<int>(options.size())) {
                clearScreen();
                options[choiceNum - 1].second(); // Execute the chosen function
                std::cout << "\nPress Enter to continue...";
                std::cin.get();
            } else {
                throw std::out_of_range("Invalid choice");
            }
        } catch (const std::exception&) {
            std::cout << ERROR_COLOR << "Invalid choice. Please try again.\n" << RESET_COLOR;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void developerSetupMenu() {
  singleActionMenuTemplate("Install Developer Tools", "Install developer tools",
                           developerSetup);
}

void gamingSetupMenu() {
  singleActionMenuTemplate("Setup Gaming", "Set up gaming environment",
                           gamingSetup);
}

void setupLVimMenu() {
  singleActionMenuTemplate("Setup LVim", "Setup LunarVim", setupLVim);
}

void setupDoomEmacsMenu() {
  singleActionMenuTemplate("Setup Doom-Emacs", "Setup Doom Emacs",
                           setupDoomEmacs);
}

void setupTerminalMenu() {
  std::vector<std::pair<std::string, std::function<void()>>> options = {
      {"Install WezTerm", setupWezTerm}, {"Install Kitty", setupKitty}};
  colorizedMenuTemplate("Install Terminals", options);
}

void setupYayMenu() {
  singleActionMenuTemplate("Setup Yay", "Setup Yay", setupYay);
}

void setupFlatpakMenu() {
  singleActionMenuTemplate("Setup Flatpak", "Setup Flatpak", setupFlatpak);
}

void showMainMenuAndHandleInput() {
  std::vector<std::pair<std::string, std::function<void()>>> options = {
      {"Setup Shell (Zsh)", setupShellMenu},
      {"Install Developer Tools", developerSetupMenu},
      {"Setup Gaming", gamingSetupMenu},
      {"Install LunarVim", setupLVimMenu},
      {"Install Doom Emacs", setupDoomEmacsMenu},
      {"Install Terminals", setupTerminalMenu},
      {"Search & Download a Package", downloadPackage},
      {"Setup Yay (AUR Helper)", setupYayMenu},
      {"Setup Flatpak", setupFlatpakMenu}};
  colorizedMenuTemplate("Arch Linux Setup Menu", options);
}

int main(int argc, char *argv[]) {
  std::cout << GRUVBOX_BG << GRUVBOX_FG; // Set background and foreground colors
  parseFlags(argc, argv);
  askForSudoPassword();
  showMainMenuAndHandleInput();
  std::cout << RESET_COLOR;
  return 0;
}
