#ifndef RESOURCES_H
#define RESOURCES_H

#include <string>
#include <cstdlib>

class ResourceManager {
public:
    static bool extractEmbeddedZip(const std::string& username) {
        std::string configDir = "/home/" + username + "/.config/cmi";

        system(("mkdir -p " + configDir).c_str());

        // Copy the zip file from current directory
        std::string copyCmd = "cp build-image-arch-img.zip " + configDir + "/";
        if (system(copyCmd.c_str()) != 0) {
            return false;
        }

        // Extract it
        std::string extractCmd = "cd " + configDir + " && unzip -o build-image-arch-img.zip >/dev/null 2>&1";
        if (system(extractCmd.c_str()) != 0) {
            return false;
        }

        // Clean up
        system(("rm -f " + configDir + "/build-image-arch-img.zip").c_str());

        return true;
    }

    // ADD THIS FUNCTION TO FIX THE ERROR
    static bool extractCalamaresResources(const std::string& username) {
        // Your implementation for Calamares resources extraction
        return extractEmbeddedZip(username); // or whatever logic you need
    }
};

#endif // RESOURCES_H
