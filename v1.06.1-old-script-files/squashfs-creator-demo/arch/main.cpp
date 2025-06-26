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
    std::string version_message = "Claudemods SquashFS Creator v1.05.1 22-01-2025 This Will Take a While!";

    std::cout << "\033[31m" << R"(
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)" << "\033[0m" << std::endl;

std::cout << "\033[34m" << version_message << "\033[0m" << std::endl;
}

void run_command(const std::string& command) {
    int status = system(command.c_str());
    if (status != 0) {
        std::cerr << "\033[31mCommand failed with status " << status << "\033[0m" << std::endl;
        throw std::runtime_error("Command failed with status " + std::to_string(status));
    }
}

void capture_and_display_rsync_output(const std::vector<std::string>& command) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        throw std::runtime_error("Pipe creation failed");
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        std::vector<char*> args;
        for (const auto& arg : command) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);

        execvp(args[0], args.data());
        std::cerr << "\033[31mError executing command: " << args[0] << "\033[0m" << std::endl;
        exit(1);
    } else if (pid > 0) {
        close(pipefd[1]);

        char buffer[128];
        while (true) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(pipefd[0], &read_fds);

            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int select_result = select(pipefd[0] + 1, &read_fds, nullptr, nullptr, &timeout);
            if (select_result == -1) {
                throw std::runtime_error("Select failed");
            } else if (select_result == 0) {
                continue;
            } else {
                ssize_t count = read(pipefd[0], buffer, sizeof(buffer));
                if (count == 0) {
                    break;
                } else if (count < 0) {
                    throw std::runtime_error("Read from pipe failed");
                } else {
                    std::string output(buffer, count);
                    std::cout << "\033[32m" << output << "\033[0m";
                    std::cout.flush();
                }
            }
        }

        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);
        if (status != 0) {
            throw std::runtime_error("Command failed with status " + std::to_string(status));
        }
    } else {
        throw std::runtime_error("Fork failed");
    }
}

void create_squashfs_image(const std::string& output_image, const std::string& clone_dir, const std::vector<std::string>& exclude_dirs) {
    std::cout << "\033[32mCreating SquashFS image: " << fs::path(output_image).filename() << "\033[0m" << std::endl;

    std::vector<std::string> command = {"sudo", "mksquashfs", clone_dir, output_image,
        "-comp", "xz", "-b", "1M",
        "-no-duplicates", "-no-recovery", "-always-use-fragments",
        "-wildcards", "-xattrs"};

        command.push_back("-Xdict-size");
        command.push_back("100%");

        command.push_back("-Xbcj");
        command.push_back("x86");

        for (const auto& dir : exclude_dirs) {
            std::string relative_dir = dir;
            while (relative_dir.front() == '/') {
                relative_dir.erase(relative_dir.begin());
            }
            command.push_back("-e");
            command.push_back(relative_dir);
        }

        std::ostringstream command_stream;
        for (const auto& arg : command) {
            command_stream << arg << " ";
        }
        std::string command_string = command_stream.str();

        std::string full_command = "eval 'printf \"\\e[32m\"; sudo mksquashfs " + clone_dir + " " + output_image +
        " -comp xz -b 1M -no-duplicates -no-recovery -always-use-fragments -wildcards -xattrs " +
        "-Xdict-size 100% -Xbcj x86 -e ";

        for (const auto& dir : exclude_dirs) {
            full_command += dir + " ";
        }

        full_command += "; printf \"\\e[0m\"'";
        run_command(full_command);
}

void clone_system(const std::string& clone_dir, const std::vector<std::string>& exclude_dirs) {
    fs::create_directories(clone_dir);

    std::vector<std::string> command_part1 = {"sudo", "rsync", "-aHAX", "--numeric-ids", "--info=progress2",
        "--include", "dev", "--include", "proc", "--include", "tmp", "--include", "sys",
        "--include", "run",
        "--exclude", "dev/*", "--exclude", "proc/*", "--exclude", "tmp/*", "--exclude", "sys/*",
        "--exclude", "run/*",
        "--exclude", "usr",
        "--exclude", "build-image-arch"}; // Added exclusion for build-image-arch

        for (const auto& dir : exclude_dirs) {
            std::string relative_dir = dir;
            while (relative_dir.front() == '/') {
                relative_dir.erase(relative_dir.begin());
            }
            command_part1.push_back("--exclude");
            command_part1.push_back(relative_dir);
        }
        command_part1.push_back("/");
        command_part1.push_back(clone_dir);

        std::cout << "\033[34mCloning system to: " << clone_dir << "\033[0m" << std::endl;
        std::cout << "\033[32mStarting Rsync Cloning Part 1 of 2\033[0m" << std::endl;
        capture_and_display_rsync_output(command_part1);
        std::cout << "\033[32mRsync Cloning Part 1 of 2 Finished\033[0m" << std::endl;

        std::string rsync_command_part2 = "eval 'printf \"\\e[32m\"; sudo rsync -aHAxSr --numeric-ids --info=progress2 /usr/ " + clone_dir + "/usr; printf \"\\e[0m\"'";
        std::cout << "\033[32mStarting Rsync Cloning Part 2 of 2\033[0m" << std::endl;
        run_command(rsync_command_part2);
        std::cout << "\033[32mRsync Cloning Part 2 of 2 Finished\033[0m" << std::endl;
}

void delete_clone_system_temp(const std::string& clone_dir) {
    std::string command = "sudo rm -rf " + clone_dir;
    run_command(command);
    std::cout << "cloned_system_temp directory deleted: " << clone_dir << std::endl;
}

void delete_squashfs_image(const std::string& image_path) {
    std::string command = "sudo rm -f " + image_path;
    run_command(command);
    std::cout << "SquashFS image deleted: " << image_path << std::endl;
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

std::string read_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }
    std::string content;
    std::getline(file, content);
    file.close();
    return content;
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

        // Updated exclude_dirs vector to include clone_system_temp
        std::vector<std::string> exclude_dirs = {
            "var/lib/docker", "cloned_system_temp", "clone_system_temp", "etc/fstab", "etc/mtab",
            "etc/udev/rules.d/70-persistent-cd.rules", "etc/udev/rules.d/70-persistent-net.rules"
        };

        while (true) {
            std::cout << "\033[34mSelect an option:\033[0m" << std::endl;
            std::cout << "\033[34m1. Clone System and Create SquashFS Image\033[0m" << std::endl;
            std::cout << "\033[34m2. Delete cloned_system_temp\033[0m" << std::endl;
            std::cout << "\033[34m3. Delete SquashFS image\033[0m" << std::endl;
            std::cout << "\033[34m4. Go back to main menu\033[0m" << std::endl;
            int choice;
            std::cin >> choice;

            switch (choice) {
                case 1: {
                    // Read build-image-location from file
                    std::string build_image_location_file = "/opt/claudemods-iso-konsole-script/Supported-Distros/Arch/Scripts/Arch-Image-location.txt";
                    std::string build_image_location = read_file(build_image_location_file);

                    // Read clone directory from file
                    std::string clone_dir_file = "/opt/claudemods-iso-konsole-script/Supported-Distros/Arch/Scripts/CloneDirectory.txt";
                    std::string clone_dir = read_file(clone_dir_file);

                    // Ensure the clone directory ends with /clone_system_temp
                    if (!clone_dir.empty() && clone_dir.back() != '/') {
                        clone_dir += "/";
                    }
                    clone_dir += "clone_system_temp";

                    std::string squashfs_image = build_image_location + "/arch/x86_64/airootfs.sfs";
                    clone_system(clone_dir, exclude_dirs);
                    create_squashfs_image(squashfs_image, clone_dir, exclude_dirs);
                    delete_clone_system_temp(clone_dir);
                    display_success_message_and_delay("SquashFS image created and cloned_system_temp deleted successfully.", 5);
                    break;
                }
                case 2: {
                    std::string clone_dir;
                    std::cout << "Enter the path of the cloned_system_temp directory to delete: ";
                    std::cin >> clone_dir;
                    delete_clone_system_temp(clone_dir);
                    delay_and_return_to_menu(5);
                    continue;
                }
                case 3: {
                    std::string image_path;
                    std::cout << "Enter the path of the SquashFS image to delete: ";
                    std::cin >> image_path;
                    delete_squashfs_image(image_path);
                    delay_and_return_to_menu(5);
                    continue;
                }
                case 4:
                    run_command({"ruby", "/opt/claudemods-iso-konsole-script/demo.rb"});
                    continue;
                default:
                    std::cerr << "Invalid choice. Returning to main menu." << std::endl;
                    delay_and_return_to_menu(5);
                    continue;
            }

            break;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
