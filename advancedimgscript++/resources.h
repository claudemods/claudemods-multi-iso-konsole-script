#ifndef RESOURCES_H
#define RESOURCES_H

#include <string>
#include <cstdlib>

class ResourceManager {
public:
    static bool extractEmbeddedZip(const std::string& username) {
        std::string configDir = "/home/" + username + "/.config/cmi";
        std::string calamaresTargetDir = configDir + "/calamares-files";

        // Create directories first
        system(("mkdir -p " + configDir).c_str());
        system(("mkdir -p " + calamaresTargetDir).c_str());

        // 1. Handle build-image-arch-img.zip - COPY AND EXTRACT
        std::string copyCmd1 = "cp build-image-arch-img.zip " + configDir + "/";
        if (system(copyCmd1.c_str()) != 0) {
            return false;
        }

        std::string extractCmd1 = "cd " + configDir + " && unzip -o build-image-arch-img.zip >/dev/null 2>&1";
        if (system(extractCmd1.c_str()) != 0) {
            return false;
        }
        system(("rm -f " + configDir + "/build-image-arch-img.zip").c_str());

        // 2. Handle calamares-files.zip - COPY AND EXTRACT
        std::string copyCmd2 = "cp calamares-files.zip " + configDir + "/";
        if (system(copyCmd2.c_str()) != 0) {
            return false;
        }

        std::string extractCmd2 = "cd " + configDir + " && unzip -o calamares-files.zip >/dev/null 2>&1";
        if (system(extractCmd2.c_str()) != 0) {
            return false;
        }
        system(("rm -f " + configDir + "/calamares-files.zip").c_str());

        // 3. Handle claudemods.zip - COPY AND EXTRACT to calamares-files
        std::string copyCmd3 = "cp claudemods.zip " + configDir + "/";
        if (system(copyCmd3.c_str()) != 0) {
            return false;
        }

        std::string extractCmd3 = "cd " + configDir + " && unzip -o claudemods.zip -d " + calamaresTargetDir + " >/dev/null 2>&1";
        if (system(extractCmd3.c_str()) != 0) {
            return false;
        }
        system(("rm -f " + configDir + "/claudemods.zip").c_str());

        return true;
    }

    static bool extractCalamaresResources(const std::string& username) {
        return extractEmbeddedZip(username);
    }
};

#endif // RESOURCES_H
