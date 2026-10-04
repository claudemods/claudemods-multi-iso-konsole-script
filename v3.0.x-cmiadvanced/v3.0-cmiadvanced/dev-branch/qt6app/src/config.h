#pragma once

#include <QString>

struct ConfigState {
    QString isoTag;
    QString isoName;
    QString outputDir;
    QString vmlinuzPath;
    QString cloneDir;
    bool mkinitcpioGenerated = false;
    bool mkinitcpioConfigCopied = false;
    bool grubEdited = false;
    bool bootTextEdited = false;
    bool calamaresBrandingEdited = false;
    bool calamares1Edited = false;
    bool calamares2Edited = false;
    bool filesExtracted = false;

    bool isReadyForISO() const
    {
        return !isoTag.isEmpty() && !isoName.isEmpty() && !outputDir.isEmpty() &&
               !vmlinuzPath.isEmpty() && mkinitcpioGenerated && grubEdited;
    }

    bool allCheckboxesChecked() const
    {
        return !isoTag.isEmpty() && !isoName.isEmpty() &&
               !outputDir.isEmpty() && !vmlinuzPath.isEmpty() && !cloneDir.isEmpty() &&
               mkinitcpioGenerated && grubEdited && bootTextEdited &&
               calamaresBrandingEdited && calamares1Edited && calamares2Edited &&
               filesExtracted && mkinitcpioConfigCopied;
    }
};

namespace Paths {
QString username();
QString configDir();   // /home/<user>/.config/cmi
QString buildDir();    // <configDir>/build-image-arch-img
QString liveOsDir();   // <buildDir>/LiveOS
QString configFile();  // <configDir>/configuration.txt
} // namespace Paths

// Expands "~" and "$USER" like the original expandPath().
QString expandPath(const QString& path);
// Single-quotes a string for safe use in a bash command line.
QString shellQuote(const QString& s);

bool loadConfig(ConfigState& config);
bool saveConfig(const ConfigState& config);
