#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <fstream>

// ANSI escape codes for colors
#define RED "\e[31m"
#define DARK_GREEN "\e[32m"
#define BLUE "\e[34m"
#define RESET "\e[0m"

// Function to prompt for user input
std::string prompt(const std::string& prompt_text) {
    std::cout << BLUE << prompt_text << RESET << " ";
    std::string input;
    std::getline(std::cin, input);
    return input;
}

// Function to display a progress dialog (simulated)
void progress_dialog(const std::string& message) {
    std::cout << DARK_GREEN << message << RESET << std::endl;
}

// Function to display a message box (simulated)
void message_box(const std::string& title, const std::string& message) {
    std::cout << DARK_GREEN << title << RESET << std::endl;
    std::cout << DARK_GREEN << message << RESET << std::endl;
}

// Function to display an error message box (simulated)
void error_box(const std::string& title, const std::string& message) {
    std::cout << RED << title << RESET << std::endl;
    std::cout << RED << message << RESET << std::endl;
}

// Function to read a file and return its content
std::string read_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        error_box("File Error", "Unable to open file: " + file_path);
        return "";
    }
    std::string content;
    std::getline(file, content);
    file.close();
    return content;
}

// Function to create an ISO
void create_iso(const std::string& iso_name, const std::string& build_image_dir) {
    if (iso_name.empty()) {
        error_box("Input Error", "ISO name cannot be empty.");
        return;
    }

    if (build_image_dir.empty()) {
        error_box("Input Error", "Build image directory cannot be empty.");
        return;
    }

    progress_dialog("Creating ISO...");

    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    std::ostringstream oss;
    oss << iso_name << "_amd64_" << std::put_time(now, "%Y-%m-%d_%H%M") << ".iso";
    std::string iso_file_name = oss.str();

    std::ostringstream xorriso_oss;
    xorriso_oss << "eval 'printf \"\\e[32m\"; sudo xorriso -as mkisofs -o " << std::quoted(iso_file_name)
    << " -V 2024 -iso-level 3 -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin -c isolinux/boot.cat "
    << "-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot "
    << "-e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat " << std::quoted(build_image_dir)
    << "; printf \"\\e[0m\"'";

    std::string xorriso_command = xorriso_oss.str();

    std::cout << DARK_GREEN;
    int exit_code = system(xorriso_command.c_str());
    std::cout << RESET;

    if (exit_code != 0) {
        std::cout << "Command Execution Error: Command '" << xorriso_command << "' failed with exit code " << exit_code << "." << std::endl;
    }

    message_box("Success", "ISO creation completed.");
}

int main() {
    // Read ISO name from file
    std::string iso_name_file = "/opt/claudemods-iso-konsole-script/Supported-Distros/Debian/Scripts/isoname.txt";
    std::string iso_name = read_file(iso_name_file);
    if (iso_name.empty()) {
        error_box("Error", "Failed to read ISO name from file.");
        return 1;
    }

    // Read build location from file
    std::string build_location_file = "/opt/claudemods-iso-konsole-script/Supported-Distros/Debian/Scripts/Debian-Image-location.txt";
    std::string build_location = read_file(build_location_file);
    if (build_location.empty()) {
        error_box("Error", "Failed to read build location from file.");
        return 1;
    }

    // Ask user if they want to use the predefined build location
    std::string use_predefined = prompt("Do you want to use the predefined build location? (" + build_location + ") [yes/no]:");
    if (use_predefined != "yes" && use_predefined != "no") {
        error_box("Input Error", "Invalid choice. Please enter 'yes' or 'no'.");
        return 1;
    }

    if (use_predefined == "no") {
        build_location = prompt("Enter the custom build location:");
        if (build_location.empty()) {
            error_box("Input Error", "Custom build location cannot be empty.");
            return 1;
        }
    }

    // Create the ISO
    create_iso(iso_name, build_location);

    return 0;
}
