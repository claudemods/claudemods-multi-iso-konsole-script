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
    std::string version_message = "Claudemods SquashFS Creator v1.05.1 This Will Take a While!";

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

void run_command(const std::vector<std::string>& command) {
    std::ostringstream command_stream;
    for (const auto& arg : command) {
        command_stream << arg << " ";
    }
    std::string command_string = command_stream.str();

    // Suppress the command output
    int status = system((command_string + " > /dev/null 2>&1").c_str());
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
                    std::cout << "\033[32m" << output << "\033[0m"; // Output in dark green
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

void create_squashfs_image(const std::string& output_image, const std::string& clone_dir, const std::vector<std::string>& exclude_dirs) {
    std::cout << "\033[32m"; // Set text color to dark green

    std::vector<std::string> command = {"sudo", "mksquashfs", clone_dir, output_image,
        "-comp", "xz", "-b", "1M",
        "-no-duplicates", "-no-recovery", "-always-use-fragments",
        "-wildcards", "-xattrs"};

        // Use -Xdict-size to achieve maximum compression
        command.push_back("-Xdict-size");
        command.push_back("100%"); // Set dictionary size to 100% of the block size

        // Use -Xbcj to apply byte code filtering for x86 architecture
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

        std::cout << "Creating SquashFS image: " << fs::path(output_image).filename() << std::endl;

        // Construct the command string
        std::ostringstream command_stream;
        for (const auto& arg : command) {
            command_stream << arg << " ";
        }
        std::string command_string = command_stream.str();

        // Run the command using system
        int status = system(command_string.c_str());
        if (status != 0) {
            throw std::runtime_error("Command failed with status " + std::to_string(status) + ": " + command_string);
        }

        std::cout << "\033[0m"; // Reset text color to default
}

void clone_system(const std::string& clone_dir, const std::vector<std::string>& exclude_dirs) {
    fs::create_directories(clone_dir);

    // Part 1: Rsync everything except /usr
    std::vector<std::string> command_part1 = {"sudo", "rsync", "-aHAX", "--numeric-ids", "--info=progress2",
        "--include", "dev", "--include", "proc", "--include", "tmp", "--include", "sys",
        "--include", "run", "--include", "media",
        "--exclude", "dev/*", "--exclude", "proc/*", "--exclude", "tmp/*", "--exclude", "sys/*",
        "--exclude", "run/*", "--exclude", "media/*", "--exclude", "clone_system_temp",
        "--exclude", "usr"}; // Exclude /usr in the first pass

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

        // Part 2: Rsync /usr separately using eval for colored output
        std::string rsync_command_part2 = "eval 'printf \"\\e[32m\"; sudo rsync -aHAxSr --numeric-ids --info=progress2 /usr/ " + clone_dir + "/usr; printf \"\\e[0m\"'";
        std::cout << "\033[32mStarting Rsync Cloning Part 2 of 2\033[0m" << std::endl;
        int status = system(rsync_command_part2.c_str()); // Directly execute the eval command
        if (status != 0) {
            throw std::runtime_error("Command failed with status " + std::to_string(status));
        }
        std::cout << "\033[32mRsync Cloning Part 2 of 2 Finished\033[0m" << std::endl;
}

void delete_clone_system_temp(const std::string& clone_dir) {
    std::string command = "sudo rm -rf " + clone_dir;
    run_command({command});
    std::cout << "cloned_system_temp directory deleted: " << clone_dir << std::endl;
}

void delete_squashfs_image(const std::string& build_image_dir) {
    fs::path squashfs_image_path = fs::path(build_image_dir) / "live/filesystem.squashfs";
    std::string command = "sudo rm -f " + squashfs_image_path.string();
    run_command({command});
    std::cout << "SquashFS image deleted: " << squashfs_image_path << std::endl;
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

void clone_and_create_squashfs_in_custom_location(const std::string& clone_dir, const std::vector<std::string>& exclude_dirs, const std::string& build_image_dir) {
    fs::path live_dir = fs::path(build_image_dir) / "live";
    fs::create_directories(live_dir);

    std::string squashfs_image = (live_dir / "filesystem.squashfs").string();

    clone_system(clone_dir, exclude_dirs);
    create_squashfs_image(squashfs_image, clone_dir, exclude_dirs);
    delete_clone_system_temp(clone_dir);
    display_success_message_and_delay("SquashFS image created in custom location and temporary files deleted.", 5);
}

int main(int argc, char* argv[]) {
    try {
        // Check if the script is run as root
        if (geteuid() != 0) {
            std::cerr << "\033[34mThis script requires root. Exiting.\033[0m" << std::endl;
            return 1;
        }

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
        std::vector<std::string> exclude_dirs = {
            "var/lib/docker", "cloned_system_temp", "etc/fstab", "etc/mtab",
            "etc/udev/rules.d/70-persistent-cd.rules", "etc/udev/rules.d/70-persistent-net.rules"
        };

        while (true) {
            std::cout << "\033[34mSelect an option:\033[0m" << std::endl;
            std::cout << "\033[34m1. Clone and Create Squashfs in Custom Location (Delete Clone_System_Temp)\033[0m" << std::endl;
            std::cout << "\033[34m2. Delete SquashFS image\033[0m" << std::endl;
            std::cout << "\033[34m3. Go back to main menu\033[0m" << std::endl;
            int choice;
            std::cin >> choice;

            switch (choice) {
                case 1: {
                    std::string custom_build_image_dir;
                    std::cout << "\033[34mEnter the path to the build-image-debian folder: \033[0m";
                    std::cin >> custom_build_image_dir;

                    // Extract the base directory (e.g., /usr) from the custom_build_image_dir
                    fs::path base_dir = fs::path(custom_build_image_dir).parent_path();
                    std::string custom_clone_dir = (base_dir / "cloned_system_temp").string();

                    clone_and_create_squashfs_in_custom_location(custom_clone_dir, exclude_dirs, custom_build_image_dir);
                    break;
                }
                case 2: {
                    std::string build_image_dir;
                    std::cout << "\033[34mEnter the path to the build-image-debian folder: \033[0m";
                    std::cin >> build_image_dir;

                    delete_squashfs_image(build_image_dir);
                    delay_and_return_to_menu(5);
                    continue;
                }
                case 3:
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
