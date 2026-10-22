#pragma once

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QString>

// DJNote: shared helpers for the NML tests (fixtures in src/test/nml-test-data,
// audio from src/test/id3-test-data).

namespace nmltest {

/// Traktor's DIR encoding of an absolute directory: every '/' becomes "/:",
/// with a trailing "/:".
inline QString traktorDir(const QString& dirWithoutVolume) {
    QString dir = dirWithoutVolume;
    if (!dir.endsWith(QLatin1Char('/'))) {
        dir += QLatin1Char('/');
    }
    return dir.replace(QLatin1Char('/'), QStringLiteral("/:"));
}

/// Splits an absolute directory path into Traktor's (VOLUME, DIR).
inline void splitVolumeAndDir(const QString& absoluteDir, QString* pVolume, QString* pDir) {
    QString rest = absoluteDir;
    pVolume->clear();
    if (rest.size() >= 2 && rest.at(1) == QLatin1Char(':')) {
        *pVolume = rest.left(2);
        rest = rest.mid(2);
    }
    *pDir = traktorDir(rest);
}

/// Copies fixture `fixtureName` (from nml-test-data) to `destDir/destName`.
/// If `recordedDir` is non-empty the recorded location (VOLUME/DIR and the
/// playlist keys) is rewritten to that existing directory, otherwise the
/// fixture's recorded folder "nml-fixture-recorded" (which does not exist) is
/// kept. Returns the path of the written file.
inline QString writeFixture(const QDir& testDir,
        const QString& fixtureName,
        const QDir& destDir,
        const QString& destName,
        const QString& recordedDir = QString()) {
    QFile in(testDir.filePath(QStringLiteral("nml-test-data/") + fixtureName));
    EXPECT_TRUE(in.open(QIODevice::ReadOnly));
    QString text = QString::fromUtf8(in.readAll());
    if (!recordedDir.isEmpty()) {
        QString volume;
        QString dir;
        splitVolumeAndDir(recordedDir, &volume, &dir);
        text.replace(QStringLiteral("C:/:nml-fixture-recorded/:"), volume + dir);
        text.replace(QStringLiteral("DIR=\"/:nml-fixture-recorded/:\""),
                QStringLiteral("DIR=\"") + dir + QStringLiteral("\""));
        text.replace(QStringLiteral("VOLUME=\"C:\""),
                QStringLiteral("VOLUME=\"") + volume + QStringLiteral("\""));
    }
    const QString destPath = destDir.filePath(destName);
    QFile out(destPath);
    EXPECT_TRUE(out.open(QIODevice::WriteOnly));
    out.write(text.toUtf8());
    return destPath;
}

/// Copies test audio `id3-test-data/<name>` into destDir. Returns the new path.
inline QString copyAudio(const QDir& testDir, const QString& name, const QDir& destDir) {
    const QString dest = destDir.filePath(name);
    QFile::remove(dest);
    EXPECT_TRUE(QFile::copy(testDir.filePath(QStringLiteral("id3-test-data/") + name), dest));
    return dest;
}

} // namespace nmltest
