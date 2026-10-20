#pragma once

// Reader for the Phosphor overview test fixtures (*.phov), see
// src/test/phosphor/README.md.

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

namespace phosphoroverviewfixture {

constexpr int kColumns = 1920;
constexpr int kRows = 62;

struct Settings {
    double brightness = 1.0;
    double levelExponent = 0.7;
    double densityExponent = 0.25;
    double gainLow = 1.0;
    double gainMid = 1.0;
    double gainHigh = 1.0;
};

struct Image {
    QString name;
    uint32_t height = 0;
    uint32_t columnsDone = 0; // columns built so far; the rest are empty
    Settings settings;
    // height rows of kColumns pixels, bytes B, G, R, A (Qt ARGB32 on a little-endian machine)
    std::vector<uint8_t> pixels;
};

struct Fixture {
    bool ok = false;
    uint32_t rate = 0;
    uint32_t channels = 0; // 1: L = R
    uint32_t frames = 0;
    std::vector<int16_t> input; // interleaved
    uint32_t samplesPerColumn = 0;
    std::vector<uint8_t> levels;  // [kColumns][3] round(level * 255), R G B = low mid high
    std::vector<uint8_t> density; // [kColumns][kRows] round(255 * (d / 0.8)^0.25)
    std::vector<Image> images;
    // Display images (what the widget draws), by the name of the Image they are made from:
    // same size as that image, bytes B, G, R, A.
    std::map<QString, std::vector<uint8_t>> display;
};

namespace detail {

// Cursor over a byte range; reads past the end set bad() and return zeros.
class Reader {
  public:
    Reader(const char* p, size_t n)
            : m_p(p), m_end(p + n) {
    }
    template<typename T>
    T get() {
        T v{};
        if (static_cast<size_t>(m_end - m_p) < sizeof(T)) {
            m_bad = true;
            return v;
        }
        std::memcpy(&v, m_p, sizeof(T));
        m_p += sizeof(T);
        return v;
    }
    QByteArray bytes(size_t n) {
        if (static_cast<size_t>(m_end - m_p) < n) {
            m_bad = true;
            return {};
        }
        QByteArray b(m_p, static_cast<qsizetype>(n));
        m_p += n;
        return b;
    }
    QByteArray rest() {
        return bytes(static_cast<size_t>(m_end - m_p));
    }
    bool atEnd() const {
        return m_p >= m_end;
    }
    bool bad() const {
        return m_bad;
    }

  private:
    const char* m_p;
    const char* m_end;
    bool m_bad = false;
};

template<typename T>
inline std::vector<T> unpack(const QByteArray& qCompressed, size_t count) {
    const QByteArray raw = qUncompress(qCompressed);
    if (static_cast<size_t>(raw.size()) != count * sizeof(T)) {
        return {};
    }
    std::vector<T> out(count);
    std::memcpy(out.data(), raw.constData(), raw.size());
    return out;
}

} // namespace detail

inline Fixture load(const QString& path) {
    Fixture f;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return f;
    }
    const QByteArray bytes = file.readAll();
    detail::Reader top(bytes.constData(), static_cast<size_t>(bytes.size()));
    if (top.bytes(4) != "PHOV" || top.get<uint32_t>() != 1) {
        return f;
    }
    while (!top.atEnd() && !top.bad()) {
        const QByteArray tag = top.bytes(4);
        const uint32_t size = top.get<uint32_t>();
        const QByteArray payload = top.bytes(size);
        if (top.bad()) {
            return f;
        }
        detail::Reader r(payload.constData(), static_cast<size_t>(payload.size()));
        if (tag == "INPT") {
            f.rate = r.get<uint32_t>();
            f.channels = r.get<uint32_t>();
            f.frames = r.get<uint32_t>();
            f.input = detail::unpack<int16_t>(r.rest(), static_cast<size_t>(f.frames) * f.channels);
            if (f.input.empty() && f.frames > 0) {
                return f;
            }
        } else if (tag == "COLS") {
            const uint32_t columns = r.get<uint32_t>();
            const uint32_t rows = r.get<uint32_t>();
            f.samplesPerColumn = r.get<uint32_t>();
            if (columns != kColumns || rows != kRows) {
                return f;
            }
            const std::vector<uint8_t> all = detail::unpack<uint8_t>(
                    r.rest(), static_cast<size_t>(columns) * (3 + rows));
            if (all.empty()) {
                return f;
            }
            f.levels.assign(all.begin(), all.begin() + columns * 3);
            f.density.assign(all.begin() + columns * 3, all.end());
        } else if (tag == "IMAG") {
            Image im;
            const uint16_t nameLength = r.get<uint16_t>();
            im.name = QString::fromUtf8(r.bytes(nameLength));
            im.height = r.get<uint32_t>();
            im.columnsDone = r.get<uint32_t>();
            im.settings.brightness = r.get<double>();
            im.settings.levelExponent = r.get<double>();
            im.settings.densityExponent = r.get<double>();
            im.settings.gainLow = r.get<double>();
            im.settings.gainMid = r.get<double>();
            im.settings.gainHigh = r.get<double>();
            im.pixels = detail::unpack<uint8_t>(
                    r.rest(), static_cast<size_t>(im.height) * kColumns * 4);
            if (im.pixels.empty()) {
                return f;
            }
            f.images.push_back(std::move(im));
        } else if (tag == "DISP") {
            const uint16_t nameLength = r.get<uint16_t>();
            const QString name = QString::fromUtf8(r.bytes(nameLength));
            const QByteArray raw = qUncompress(r.rest());
            if (raw.isEmpty()) {
                return f;
            }
            f.display[name].assign(reinterpret_cast<const uint8_t*>(raw.constData()),
                    reinterpret_cast<const uint8_t*>(raw.constData()) + raw.size());
        }
        if (r.bad()) {
            return f;
        }
    }
    f.ok = !top.bad() && !f.levels.empty();
    return f;
}

} // namespace phosphoroverviewfixture
