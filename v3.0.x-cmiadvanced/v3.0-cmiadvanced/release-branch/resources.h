#ifndef RESOURCES_H
#define RESOURCES_H

#include <string>
#include <fstream>
#include <cstdlib>

// External symbols for the 3 embedded zip files
extern char _binary_build_image_arch_img_zip_start[];
extern char _binary_build_image_arch_img_zip_end[];
extern char _binary_calamares_files_zip_start[];
extern char _binary_calamares_files_zip_end[];
extern char _binary_claudemods_zip_start[];
extern char _binary_claudemods_zip_end[];

class ResourceManager {
public:
    static bool extractEmbeddedZip(const std::string& username) {
        std::string configDir = "/home/" + username + "/.config/cmi";
        std::string calamaresTargetDir = configDir + "/calamares-files";

        // 1. Extract build-image-arch-img.zip
        std::ofstream f1(configDir + "/build-image-arch-img.zip", std::ios::binary);
        f1.write(_binary_build_image_arch_img_zip_start,
                 _binary_build_image_arch_img_zip_end - _binary_build_image_arch_img_zip_start);
        f1.close();

        std::string extractCmd1 = "cd " + configDir + " && unzip -o build-image-arch-img.zip >/dev/null 2>&1";
        if (system(extractCmd1.c_str()) != 0) return false;
        system(("rm -f " + configDir + "/build-image-arch-img.zip").c_str());

        // 2. Extract calamares-files.zip
        std::ofstream f2(configDir + "/calamares-files.zip", std::ios::binary);
        f2.write(_binary_calamares_files_zip_start,
                 _binary_calamares_files_zip_end - _binary_calamares_files_zip_start);
        f2.close();

        std::string extractCmd2 = "cd " + configDir + " && unzip -o calamares-files.zip >/dev/null 2>&1";
        if (system(extractCmd2.c_str()) != 0) return false;
        system(("rm -f " + configDir + "/calamares-files.zip").c_str());

        // 3. Extract claudemods.zip to calamares-files
        std::ofstream f3(configDir + "/claudemods.zip", std::ios::binary);
        f3.write(_binary_claudemods_zip_start,
                 _binary_claudemods_zip_end - _binary_claudemods_zip_start);
        f3.close();

        std::string extractCmd3 = "cd " + configDir + " && unzip -o claudemods.zip -d " + calamaresTargetDir + " >/dev/null 2>&1";
        if (system(extractCmd3.c_str()) != 0) return false;
        system(("rm -f " + configDir + "/claudemods.zip").c_str());

        return true;
    }

    static bool extractCalamaresResources(const std::string& username) {
        return extractEmbeddedZip(username);
    }
};

#endif
