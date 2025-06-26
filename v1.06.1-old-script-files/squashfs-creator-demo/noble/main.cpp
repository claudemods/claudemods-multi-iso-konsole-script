#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>
#include <map>
#include <sstream>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

const std::string PASSWORD = "";

void print_banner() {
    std::string version_message = "Claudemods SquashFS Creator v1.03 This Could Take a While!";

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
    std::ostringstream command_stream;
    for (const auto& arg : command) {
        command_stream << arg << " ";
    }
    std::string command_string = command_stream.str();

    int status = system(command_string.c_str());
    if (status != 0) {
        throw std::runtime_error("Command failed with status " + std::to_string(status) + ": " + command_string);
    }
}

void capture_and_display_rsync_output(const std::vector<std::string>& command) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        throw std::runtime_error("Pipe creation failed");
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(pipefd[0]); // Close reading end in the child
        dup2(pipefd[1], STDOUT_FILENO); // Send stdout to the pipe
        close(pipefd[1]); // This descriptor is no longer needed

        std::vector<char*> args;
        for (const auto& arg : command) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);

        execvp(args[0], args.data());
        std::cerr << "\033[31mError executing command: " << args[0] << "\033[0m" << std::endl;
        exit(1);
    } else if (pid > 0) {
        // Parent process
        close(pipefd[1]); // Close the writing end of the pipe in the parent

        char buffer[128];
        while (true) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(pipefd[0], &read_fds);

            struct timeval timeout;
            timeout.tv_sec = 1; // 1 second timeout
            timeout.tv_usec = 0;

            int select_result = select(pipefd[0] + 1, &read_fds, nullptr, nullptr, &timeout);
            if (select_result == -1) {
                throw std::runtime_error("Select failed");
            } else if (select_result == 0) {
                // Timeout occurred, no data available
                continue;
            } else {
                ssize_t count = read(pipefd[0], buffer, sizeof(buffer));
                if (count == 0) {
                    break; // End of file
                } else if (count < 0) {
                    throw std::runtime_error("Read from pipe failed");
                } else {
                    std::string output(buffer, count);
                    std::cout << "\033[31m" << output << "\033[31m";
                    std::cout.flush(); // Ensure immediate output
                }
            }
        }

        close(pipefd[0]); // Close the reading end of the pipe

        int status;
        waitpid(pid, &status, 0);
        if (status != 0) {
            throw std::runtime_error("Command failed with status " + std::to_string(status));
        }
    } else {
        throw std::runtime_error("Fork failed");
    }
}

void create_squashfs_image(const std::string& output_image, const std::string& clone_dir, const std::vector<std::string>& exclude_dirs, const std::string& compression) {
    std::vector<std::string> command = {"sudo", "mksquashfs", clone_dir, output_image,
        "-comp", compression, "-Xbcj", "x86", "-b", "1M",
        "-no-duplicates", "-no-recovery", "-always-use-fragments",
        "-wildcards", "-xattrs"};

        if (compression == "xz") {
            // Use -Xdict-size to achieve maximum compression
            command.push_back("-Xdict-size");
            command.push_back("100%"); // Set dictionary size to 100% of the block size
        }

        for (const auto& dir : exclude_dirs) {
            std::string relative_dir = dir;
            while (relative_dir.front() == '/') {
                relative_dir.erase(relative_dir.begin());
            }
            command.push_back("-e");
            command.push_back(relative_dir);
        }

        std::cout << "Creating SquashFS image: " << fs::path(output_image).filename() << std::endl;
        run_command(command);
}

void clone_system(const std::string& clone_dir, const std::vector<std::string>& exclude_dirs) {
    fs::create_directories(clone_dir);

    std::vector<std::string> command = {"sudo", "rsync", "-aHAX", "--numeric-ids", "--info=progress2",
        "--include", "dev", "--include", "proc", "--include", "tmp", "--include", "sys",
        "--include", "run", "--include", "media", "--include", "usr", // Include the /usr directory
        "--exclude", "dev/*", "--exclude", "proc/*", "--exclude", "tmp/*", "--exclude", "sys/*",
        "--exclude", "run/*", "--exclude", "media/*", "--exclude", "clone_system_temp"};

        for (const auto& dir : exclude_dirs) {
            // Remove leading slashes to make the paths relative
            std::string relative_dir = dir;
            while (relative_dir.front() == '/') {
                relative_dir.erase(relative_dir.begin());
            }
            command.push_back("--exclude");
            command.push_back(relative_dir);
        }
        command.push_back("/");
        command.push_back(clone_dir);
        std::cout << "\033[31mCloning system to: " << clone_dir << "\033[0m" << std::endl;
        capture_and_display_rsync_output(command);
}

void delete_clone_system_temp() {
    std::string command = "sudo rm -rf /opt/claudemods-iso-konsole-script/cloned_system_temp";
    run_command({command});
    std::cout << "cloned_system_temp directory deleted" << std::endl;
}

void delete_squashfs_image() {
    std::string command = "sudo rm -f /opt/claudemods-iso-konsole-script/Supported-Distros/Ubuntu/build-image-noble/live/filesystem.squashfs";
    run_command({command});
    std::cout << "SquashFS image deleted" << std::endl;
}

bool check_password(const std::string& provided_password) {
    return provided_password == PASSWORD;
}

void delay_and_return_to_menu(int seconds) {
    std::cout << "Returning to main menu in " << seconds << " seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

void display_success_message_and_delay(const std::string& message, int seconds) {
    std::cout << message << std::endl;
    delay_and_return_to_menu(seconds);
}

int main(int argc, char* argv[]) {
    try {
        if (argc > 1) {
            std::string provided_password = argv[1];
            if (!check_password(provided_password)) {
                std::cerr << "Incorrect password. Exiting." << std::endl;
                return 1;
            }
        } else {
            std::string provided_password;
            std::cout << "Enter password: ";
            std::cin >> provided_password;
            if (!check_password(provided_password)) {
                std::cerr << "Incorrect password. Exiting." << std::endl;
                return 1;
            }
        }

        print_banner();

        fs::path script_dir = fs::current_path();
        fs::path build_dir = "/opt/claudemods-iso-konsole-script/Supported-Distros/Ubuntu/build-image-noble/live";
        fs::create_directories(build_dir);

        std::string squashfs_image = (build_dir / "filesystem.squashfs").string();

        std::vector<std::string> exclude_dirs = {
            "var/lib/docker", "cloned_system_temp", "etc/fstab", "etc/mtab",
            "etc/udev/rules.d/70-persistent-cd.rules", "etc/udev/rules.d/70-persistent-net.rules"
        };

        std::string clone_dir = "/opt/claudemods-iso-konsole-script/cloned_system_temp";

        while (true) {
            std::cout << "\033[34mSelect an option:\033[0m" << std::endl;
            std::cout << "\033[34m1. Light compression (lzo)\033[0m" << std::endl;
            std::cout << "\033[34m2. Medium compression (lz4)\033[0m" << std::endl;
            std::cout << "\033[34m3. Max compression (xz)\033[0m" << std::endl;
            std::cout << "\033[34m4. Delete cloned_system_temp\033[0m" << std::endl;
            std::cout << "\033[34m5. Delete SquashFS image\033[0m" << std::endl;
            std::cout << "\033[34m6. Go back to main menu\033[0m" << std::endl;
            int choice;
            std::cin >> choice;

            std::string compression;
            switch (choice) {
                case 1:
                    compression = "lzo";
                    break;
                case 2:
                    compression = "lz4";
                    break;
                case 3:
                    compression = "xz";
                    break;
                case 4:
                    delete_clone_system_temp();
                    delay_and_return_to_menu(5);
                    continue;
                case 5:
                    delete_squashfs_image();
                    delay_and_return_to_menu(5);
                    continue;
                case 6:
                    run_command({"ruby", "/opt/claudemods-iso-konsole-script/demo.rb"});
                    continue;
                default:
                    std::cerr << "Invalid choice. Returning to main menu." << std::endl;
                    delay_and_return_to_menu(5);
                    continue;
            }

            clone_system(clone_dir, exclude_dirs);

            create_squashfs_image(squashfs_image, clone_dir, exclude_dirs, compression);

            display_success_message_and_delay("SquashFS image created successfully.", 5);

            break;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
