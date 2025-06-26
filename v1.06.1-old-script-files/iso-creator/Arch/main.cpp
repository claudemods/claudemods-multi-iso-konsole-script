#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <thread> // For sleep functionality
#include <chrono> // For timing
#include <fstream> // For file reading

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

// Function to display a single progress bar
void progress_bar() {
    std::cout << DARK_GREEN << "Final Step Started" << RESET << std::endl;
    int progress = 0;
    while (progress <= 100) {
        std::cout << DARK_GREEN << "\r[";
        int pos = progress / 2; // 50 characters for 100%
        for (int i = 0; i < 50; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << progress << "%" << std::flush;

        // Update progress based on timing
        if (progress < 25) {
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 5% every 5 seconds
            progress += 5;
        } else if (progress < 50) {
            std::this_thread::sleep_for(std::chrono::seconds(10)); // 25% to 50% in 30 seconds
            progress += 5;
        } else if (progress < 75) {
            std::this_thread::sleep_for(std::chrono::seconds(15)); // 50% to 75% in 1 minute 30 seconds
            progress += 5;
        } else if (progress < 100) {
            std::this_thread::sleep_for(std::chrono::seconds(30)); // 75% to 100% in 3 minutes
            progress += 5;
        } else {
            break;
        }
    }
    std::cout << DARK_GREEN << "\nFinal Step Finished" << RESET << std::endl;
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

// Function to read a file and return its content as a string
std::string read_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        error_box("File Error", "Unable to open file: " + file_path);
        return "";
    }
    std::string content;
    std::getline(file, content); // Read the first line only
    file.close();
    // Remove trailing newline characters
    content.erase(content.find_last_not_of("\n\r") + 1);
    return content;
}

// Function to create an ISO using predefined settings
void create_iso() {
    // Read ISO name from file
    std::string iso_name = read_file("/opt/claudemods-iso-konsole-script/Supported-Distros/Arch/Scripts/isoname.txt");
    if (iso_name.empty()) {
        error_box("Input Error", "ISO name cannot be empty.");
        return;
    }

    // Read build location from file
    std::string build_image_dir = read_file("/opt/claudemods-iso-konsole-script/Supported-Distros/Arch/Scripts/Arch-Image-location.txt");
    if (build_image_dir.empty()) {
        error_box("Input Error", "Build location cannot be empty.");
        return;
    }

    // Prompt user for ISO output directory
    std::string output_dir = prompt("Enter the output directory for the ISO (e.g., /home/user/isos):");
    if (output_dir.empty()) {
        error_box("Input Error", "Output directory cannot be empty.");
        return;
    }

    std::string isotag = prompt("Enter the ISO tag (e.g., 2024):");
    if (isotag.empty()) {
        error_box("Input Error", "ISO tag cannot be empty.");
        return;
    }

    progress_dialog("Creating ISO...");

    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    std::ostringstream oss;
    oss << output_dir << "/" << iso_name << "_amd64_" << std::put_time(now, "%Y-%m-%d_%H%M") << ".iso";
    std::string iso_file_name = oss.str();

    // Construct the xorriso command
    std::ostringstream xorriso_oss;
    xorriso_oss << "sudo xorriso -as mkisofs -o " << iso_file_name
    << " -V " << isotag
    << " -iso-level 3 -isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin -c isolinux/boot.cat "
    << "-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot "
    << "-e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat " << build_image_dir;

    std::string xorriso_command = xorriso_oss.str();

    // Set console color to green before running the command
    std::cout << DARK_GREEN;

    // Print the command being executed
    std::cout << "Executing command: " << xorriso_command << std::endl;

    // Run the xorriso command
    int exit_code = system(xorriso_command.c_str());

    // Reset console color
    std::cout << RESET;

    if (exit_code != 0) {
        std::cout << RED << "Command Execution Error: Command failed with exit code " << exit_code << "." << RESET << std::endl;
    } else {
        std::cout << DARK_GREEN << "ISO creation completed successfully." << RESET << std::endl;
    }

    // Show single progress bar after ISO creation
    progress_bar();

    message_box("Success", "ISO creation completed.");
}

// Function to run the ISO in QEMU
void run_iso_in_qemu() {
    std::string iso_path = prompt("Enter the full path to the ISO file:");
    if (iso_path.empty()) {
        error_box("Input Error", "ISO path cannot be empty.");
        return;
    }

    std::string qemu_command = "qemu-system-x86_64 -cdrom " + iso_path;

    // Set console color to green before running the command
    std::cout << DARK_GREEN;

    // Print the command being executed
    std::cout << "Executing command: " << qemu_command << std::endl;

    // Run the QEMU command
    int exit_code = system(qemu_command.c_str());

    // Reset console color
    std::cout << RESET;

    if (exit_code != 0) {
        std::cout << RED << "Command Execution Error: Command failed with exit code " << exit_code << "." << RESET << std::endl;
    }
}

// Main function to display the menu and handle user choices
void main_menu() {
    std::string ascii_art = std::string(RED) +
    "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n" +
    "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n" +
    "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n" +
    "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n" +
    "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n" +
    "░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░░╚════╝░╚═════╝░╚════╝░\n" +
    RESET;

    std::string script_title = std::string(BLUE) + "ClaudeMods Arch ISO Creator Script! v1.02" + RESET;

    std::cout << ascii_art << std::endl;
    std::cout << script_title << std::endl;

    while (true) {
        std::cout << BLUE << "1. Create ISO" << RESET << std::endl;
        std::cout << BLUE << "2. Run ISO in QEMU" << RESET << std::endl;
        std::cout << BLUE << "3. Exit" << RESET << std::endl;

        std::string choice = prompt("Choose an option: ");

        if (choice == "1") {
            create_iso();
        } else if (choice == "2") {
            run_iso_in_qemu();
        } else if (choice == "3") {
            break;
        } else {
            error_box("Input Error", "Invalid choice. Please select a valid option.");
        }
    }
}

int main() {
    main_menu();
    return 0;
}
