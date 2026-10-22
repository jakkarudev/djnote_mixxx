#include "library/nml/nmllabel.h"

#include <QDir>
#include <QFile>

// TagLib's FileName is a wide string on Windows.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace mixxx {
namespace nml {

QString nmlRecordLabelFromFile(const QString& filePath) {
#ifdef _WIN32
    const std::wstring nativePath = QDir::toNativeSeparators(filePath).toStdWString();
    const TagLib::FileRef file(nativePath.c_str());
#else
    const QByteArray nativePath = QFile::encodeName(filePath);
    const TagLib::FileRef file(nativePath.constData());
#endif
    if (file.isNull()) {
        return QString();
    }
    const TagLib::PropertyMap properties = file.file()->properties();
    // TagLib maps TPUB and the MP4 label atom to LABEL.
    for (const char* key : {"LABEL", "ORGANIZATION", "PUBLISHER"}) {
        const TagLib::StringList values = properties[key];
        if (!values.isEmpty()) {
            const QString label =
                    QString::fromUtf8(values.front().toCString(true)).trimmed();
            if (!label.isEmpty()) {
                return label;
            }
        }
    }
    return QString();
}

} // namespace nml
} // namespace mixxx
