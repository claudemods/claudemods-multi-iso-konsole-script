#include "config.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

#include <pwd.h>
#include <unistd.h>

namespace Paths {

QString username()
{
    static const QString name = [] {
        struct passwd* pw = getpwuid(getuid());
        return pw ? QString::fromLocal8Bit(pw->pw_name) : qEnvironmentVariable("USER");
    }();
    return name;
}

QString configDir() { return QStringLiteral("/home/") + username() + QStringLiteral("/.config/cmi"); }
QString buildDir() { return configDir() + QStringLiteral("/build-image-arch-img"); }
QString liveOsDir() { return buildDir() + QStringLiteral("/LiveOS"); }
QString configFile() { return configDir() + QStringLiteral("/configuration.txt"); }

} // namespace Paths

QString expandPath(const QString& path)
{
    QString result = path;
    qsizetype pos = result.indexOf(QLatin1Char('~'));
    if (pos >= 0)
        result.replace(pos, 1, QDir::homePath());
    pos = result.indexOf(QStringLiteral("$USER"));
    if (pos >= 0)
        result.replace(pos, 5, Paths::username());
    return result;
}

QString shellQuote(const QString& s)
{
    QString escaped = s;
    escaped.replace(QStringLiteral("'"), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

static const char* flag(bool b) { return b ? "1" : "0"; }

bool saveConfig(const ConfigState& c)
{
    QFile file(Paths::configFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "isoTag=" << c.isoTag << "\n";
    out << "isoName=" << c.isoName << "\n";
    out << "outputDir=" << c.outputDir << "\n";
    out << "vmlinuzPath=" << c.vmlinuzPath << "\n";
    out << "cloneDir=" << c.cloneDir << "\n";
    out << "mkinitcpioGenerated=" << flag(c.mkinitcpioGenerated) << "\n";
    out << "mkinitcpioConfigCopied=" << flag(c.mkinitcpioConfigCopied) << "\n";
    out << "grubEdited=" << flag(c.grubEdited) << "\n";
    out << "bootTextEdited=" << flag(c.bootTextEdited) << "\n";
    out << "calamaresBrandingEdited=" << flag(c.calamaresBrandingEdited) << "\n";
    out << "calamares1Edited=" << flag(c.calamares1Edited) << "\n";
    out << "calamares2Edited=" << flag(c.calamares2Edited) << "\n";
    out << "filesExtracted=" << flag(c.filesExtracted) << "\n";
    return true;
}

bool loadConfig(ConfigState& c)
{
    QFile file(Paths::configFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const qsizetype delimiter = line.indexOf(QLatin1Char('='));
        if (delimiter < 0)
            continue;

        const QString key = line.left(delimiter);
        const QString value = line.mid(delimiter + 1);
        const bool on = value == QLatin1String("1");

        if (key == QLatin1String("isoTag")) c.isoTag = value;
        else if (key == QLatin1String("isoName")) c.isoName = value;
        else if (key == QLatin1String("outputDir")) c.outputDir = value;
        else if (key == QLatin1String("vmlinuzPath")) c.vmlinuzPath = value;
        else if (key == QLatin1String("cloneDir")) c.cloneDir = value;
        else if (key == QLatin1String("mkinitcpioGenerated")) c.mkinitcpioGenerated = on;
        else if (key == QLatin1String("mkinitcpioConfigCopied")) c.mkinitcpioConfigCopied = on;
        else if (key == QLatin1String("grubEdited")) c.grubEdited = on;
        else if (key == QLatin1String("bootTextEdited")) c.bootTextEdited = on;
        else if (key == QLatin1String("calamaresBrandingEdited")) c.calamaresBrandingEdited = on;
        else if (key == QLatin1String("calamares1Edited")) c.calamares1Edited = on;
        else if (key == QLatin1String("calamares2Edited")) c.calamares2Edited = on;
        else if (key == QLatin1String("filesExtracted")) c.filesExtracted = on;
    }
    return true;
}
