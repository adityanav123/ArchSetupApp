#pragma once
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <regex>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

/*Structures*/
typedef struct PackageStruct {
  std::string name;
  std::string version;
  std::string description;
  std::string sourceOfPackage;
  PackageStruct(std::string nam, std::string ver, std::string desc,
                std::string src)
      : name(nam), version(ver), description(desc), sourceOfPackage(src) {}
} package;

struct MenuItem {
  std::string description;
  std::function<void()> action;
  std::function<void()> preview;
};

// Constants
constexpr const char *YELLOW_COLOR = "\033[38;5;220m";
constexpr const char *GREEN_COLOR = "\033[38;5;118m";
constexpr const char *BLUE_COLOR = "\033[38;5;39m";
constexpr const char *ORANGE_COLOR = "\033[38;5;208m";

namespace fs = std::filesystem;

// Gruvbox color palette
const char *RESET_COLOR = "\033[0m";
const char *GRUVBOX_BG = "\033[48;2;40;40;40m";
const char *GRUVBOX_FG = "\033[38;2;235;219;178m";
const char *GRUVBOX_RED = "\033[38;2;204;36;29m";
const char *GRUVBOX_GREEN = "\033[38;2;152;151;26m";
const char *GRUVBOX_YELLOW = "\033[38;2;215;153;33m";
const char *GRUVBOX_BLUE = "\033[38;2;69;133;136m";
const char *GRUVBOX_PURPLE = "\033[38;2;177;98;134m";
const char *GRUVBOX_AQUA = "\033[38;2;104;157;106m";
const char *GRUVBOX_ORANGE = "\033[38;2;214;93;14m";

#define MENU_COLOR GRUVBOX_ORANGE
#define OPTION_COLOR GRUVBOX_YELLOW
#define INPUT_COLOR GRUVBOX_GREEN
#define ERROR_COLOR GRUVBOX_RED
#define SUCCESS_COLOR GRUVBOX_AQUA

void clearScreen() { std::cout << "\033[2J\033[H"; }

void printHeader(const std::string &title) {
  std::cout << GRUVBOX_ORANGE << "+--" << std::string(title.length(), '-')
            << "--+" << RESET_COLOR << "\n";
  std::cout << GRUVBOX_ORANGE << "|  " << title << "  |" << RESET_COLOR << "\n";
  std::cout << GRUVBOX_ORANGE << "+--" << std::string(title.length(), '-')
            << "--+" << RESET_COLOR << "\n\n";
}

void printSeparator() {
  std::cout << GRUVBOX_BLUE << std::string(30, '-') << RESET_COLOR << "\n";
}

void printPrompt(const std::string &message) {
  std::cout << GRUVBOX_GREEN << " " << message << ": " << RESET_COLOR;
  std::cout.flush();
}
constexpr const char *MENU_SEPARATOR = "---------------------------------";

// Regex for pacman/yay/flatpak
std::regex pacmanYayPattern(
    R"((\S+)\/(\S+)\s+([\d\.]+-\d+)(\s*\[installed\])?\s*\n\s*(.*))");
std::regex flatpakPattern(R"((\S+)\s+\(([^)]+)\)\s+(.*))");

void displayBackOption() {
  std::cout << "\033[1;1H" << GRUVBOX_FG;
  std::cout << "Press [q] to go back";
  std::cout << RESET_COLOR;
}

void colorizedMenuTemplate(
    const std::string &title,
    const std::vector<std::pair<std::string, std::function<void()>>> &options) {
  while (true) {
    clearScreen();
    printHeader(title);

    for (size_t i = 0; i < options.size(); ++i) {
      std::cout << GRUVBOX_YELLOW << " [" << (i + 1) << "] " << RESET_COLOR
                << GRUVBOX_FG << options[i].first << RESET_COLOR << "\n";
    }

    printSeparator();
    printPrompt("Choose an option (1-" + std::to_string(options.size()) +
                "), or [q] to go back");

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
    } catch (const std::exception &) {
      std::cout << ERROR_COLOR << "Invalid choice. Please try again.\n"
                << RESET_COLOR;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

void singleActionMenuTemplate(const std::string &title,
                              const std::string &actionDescription,
                              std::function<void()> action) {
  clearScreen();
  std::cout << GRUVBOX_BG << GRUVBOX_FG;
  std::cout << MENU_COLOR << "=== " << title << " ===" << RESET_COLOR << "\n\n";
  std::cout << OPTION_COLOR << "1. " << RESET_COLOR << GRUVBOX_FG
            << actionDescription << RESET_COLOR << "\n";

  displayBackOption();
  std::cout << "\033[5;1H";

  std::cout << "\n"
            << INPUT_COLOR
            << "Press Enter to proceed or [q] to go back: " << RESET_COLOR;

  std::string choice;
  std::getline(std::cin, choice);

  if (choice != "q" && choice != "Q") {
    clearScreen();
    action();
    std::cout << "\nPress Enter to continue...";
    std::cin.get();
  }
}

// Function Prototypes
std::vector<std::string> parse_string(const std::string &input, char delimiter);
void parseYayResults(const std::string &result,
                     std::vector<PackageStruct> &matchingPackages);
void parsePacmanYayResults(const std::string &result,
                           std::vector<PackageStruct> &matchingPackages,
                           const std::string &source);

std::string runFlatpakCommand(const std::string &packageName,
                              const std::string &columns);

void fetchFlatpakDetails(const std::string &packageName,
                         std::vector<PackageStruct> &matchingPackages);
void parseFlags(int argc, char *argv[]);
void showProgressBar(int totalSteps);
void runCommand(const std::string &command);
bool isCommandSuccessful(const std::string &command);
bool isPackageInstalled(const std::string &packageName);
bool installPackageWithProgress(const std::string &packageName,
                                const std::string &extraFlags = "");
bool installPackage(const std::string &packageName,
                    const std::string &extraFlags = "");
bool downloadFile(const std::string &url, const std::string &outputFilePath);
bool isFileValid(const std::string &filePath);
void applyConfig(const std::string &gistUrl, const std::string &configPath);
void setupFlatpak();
void setZshAsDefaultShell();
void installTerminal(const std::string &terminalName,
                     const std::string &configUrl = "",
                     const std::string &configPath = "");
void setupWezTerm();
void setupKitty();
void setupTerminal();
void setupStarshipTheme();
void setupShell();
void gamingSetup();
void developerSetup();
void setupLVim();
void setupDoomEmacs();
void ensureYayInstalled();
void ensureFlatpakInstalled();

std::vector<PackageStruct> searchForPackages(const std::string &packageName);
void displayMatchingPackages(
    const std::vector<std::tuple<std::string, std::string, std::string,
                                 std::string, bool>> &matchingPackages);
void downloadPackage();
void askForSudoPassword();
void printSeparator();
std::vector<std::string> getSimpleMenuDescriptions();
std::vector<std::string> getDetailedMenuDescriptions();
void displayMenu(const std::vector<MenuItem> &menuItems);
void handleMenuChoice(const std::vector<MenuItem> &menuItems, int choice);
void setupYay();
void showMainMenuAndHandleInput();