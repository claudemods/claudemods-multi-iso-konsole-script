#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

void print_banner() {
    std::string version_message = "Claudemods Dwarfs Converter v1.01 This Will Take A while!!!";

    std::cout << "\033[31m" << R"(
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)" << "\033[0m" << std::endl;
std::cout << "\033[31m" << version_message << "\033[0m" << std::endl;
}

void run_command(const std::vector<std::string>& command) {
    std::vector<char*> args;
    for (const auto& arg : command) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execvp(args[0], args.data());
        std::cerr << "Error executing command: " << args[0] << std::endl;
        exit(1);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (status != 0) {
            throw std::runtime_error("Command failed with status " + std::to_string(status));
        }
    } else {
        throw std::runtime_error("Fork failed");
    }
}

void convert_squashfs_to_dwarfs(const std::string& squashfs_path, const std::string& dwarfs_path, int compression_level) {
    std::string temp_dir = "/tmp/squashfs_mount";
    fs::create_directories(temp_dir);

    try {
        std::cout << "Mounting SquashFS image: " << squashfs_path << " to " << temp_dir << std::endl;
        run_command({"sudo", "mount", "-t", "squashfs", squashfs_path, temp_dir});

        std::cout << "Converting SquashFS to Dwarfs: " << temp_dir << " to " << dwarfs_path << " with compression level " << compression_level << std::endl;
        run_command({"sudo", "mkdwarfs", "-i", temp_dir, "-o", dwarfs_path, "-l", std::to_string(compression_level)});
    } catch (const std::exception& e) {
        std::cerr << "Error during conversion: " << e.what() << std::endl;
    }

    std::cout << "Unmounting SquashFS image: " << temp_dir << std::endl;
    run_command({"sudo", "umount", temp_dir});
    fs::remove_all(temp_dir);
}

int main() {
    try {
        print_banner();

        fs::path script_dir = fs::current_path();
        std::string squashfs_path = (script_dir / "filesystem.sfs").string();
        std::string dwarfs_path = (script_dir / "filesystem.dwarfs").string();

        std::cout << "\033[34mSelect a compression level:\033[0m" << std::endl;
        std::cout << "\033[34m1. Level 4\033[0m" << std::endl;
        std::cout << "\033[34m2. Level 5\033[0m" << std::endl;
        std::cout << "\033[34m3. Level 6\033[0m" << std::endl;
        std::cout << "\033[34m4. Level 7\033[0m" << std::endl;
        std::cout << "\033[34m5. Level 8\033[0m" << std::endl;
        std::cout << "\033[34m6. Level 9\033[0m" << std::endl;

        int choice;
        std::cin >> choice;

        std::vector<int> compression_levels = {4, 5, 6, 7, 8, 9};

        if (choice >= 1 && choice <= 6) {
            int compression_level = compression_levels[choice - 1];
            convert_squashfs_to_dwarfs(squashfs_path, dwarfs_path, compression_level);
            std::cout << "Conversion complete: " << dwarfs_path << std::endl;
        } else {
            std::cerr << "Invalid choice. Exiting." << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
