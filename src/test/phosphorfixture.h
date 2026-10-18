#pragma once

// Reader for the Phosphor test fixtures (*.phfx), see src/test/phosphor/README.md.

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <cstdint>
#include <cstring>
#include <vector>

namespace phosphorfixture {

struct Tier {
    uint32_t n = 1;
    std::vector<int16_t> values;
};

struct LaneCase {
    QString name;
    uint32_t tierN = 1;
    double position = 0;
    double pxPerBar = 0;
    double bpm = 0;
    double playheadX = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t windowMilliColumns = 2000;
    std::vector<uint8_t> status;
    std::vector<int32_t> rowMin;
    std::vector<int32_t> rowMax;
    std::vector<float> levels;  // [width][3]
    std::vector<float> density; // [width][height]
};

struct Fixture {
    bool ok = false;
    uint32_t rate = 0;
    uint32_t channels = 0;
    uint32_t frames = 0;
    std::vector<int16_t> input; // interleaved
    std::vector<Tier> tiers;
    uint32_t colorBlockFrames = 0;
    std::vector<uint16_t> color; // [blocks][3]
    std::vector<LaneCase> lanes;
};

namespace detail {
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
    template<typename T>
    std::vector<T> vec(size_t count) {
        std::vector<T> out(count);
        if (static_cast<size_t>(m_end - m_p) < count * sizeof(T)) {
            m_bad = true;
            return {};
        }
        std::memcpy(out.data(), m_p, count * sizeof(T));
        m_p += count * sizeof(T);
        return out;
    }
    QString str(size_t n) {
        if (static_cast<size_t>(m_end - m_p) < n) {
            m_bad = true;
            return {};
        }
        QString s = QString::fromUtf8(m_p, static_cast<qsizetype>(n));
        m_p += n;
        return s;
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
} // namespace detail

inline Fixture load(const QString& path) {
    Fixture f;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return f;
    }
    const QByteArray bytes = file.readAll();
    detail::Reader top(bytes.constData(), static_cast<size_t>(bytes.size()));
    if (top.str(4) != QLatin1String("PHFX") || top.get<uint32_t>() != 1) {
        return f;
    }
    while (!top.atEnd() && !top.bad()) {
        const QString tag = top.str(4);
        const uint32_t size = top.get<uint32_t>();
        const std::vector<char> payload = top.vec<char>(size);
        if (top.bad()) {
            return f;
        }
        detail::Reader r(payload.data(), payload.size());
        if (tag == QLatin1String("INPT")) {
            f.rate = r.get<uint32_t>();
            f.channels = r.get<uint32_t>();
            f.frames = r.get<uint32_t>();
            f.input = r.vec<int16_t>(static_cast<size_t>(f.frames) * f.channels);
        } else if (tag == QLatin1String("TIER")) {
            Tier t;
            t.n = r.get<uint32_t>();
            t.values = r.vec<int16_t>(r.get<uint32_t>());
            f.tiers.push_back(std::move(t));
        } else if (tag == QLatin1String("COLR")) {
            f.colorBlockFrames = r.get<uint32_t>();
            const uint32_t blocks = r.get<uint32_t>();
            f.color = r.vec<uint16_t>(static_cast<size_t>(blocks) * 3);
        } else if (tag == QLatin1String("LANE")) {
            LaneCase c;
            c.name = r.str(r.get<uint16_t>());
            c.tierN = r.get<uint32_t>();
            c.position = r.get<double>();
            c.pxPerBar = r.get<double>();
            c.bpm = r.get<double>();
            c.playheadX = r.get<double>();
            c.width = r.get<uint32_t>();
            c.height = r.get<uint32_t>();
            c.windowMilliColumns = r.get<uint32_t>();
            c.status = r.vec<uint8_t>(c.width);
            c.rowMin = r.vec<int32_t>(c.width);
            c.rowMax = r.vec<int32_t>(c.width);
            c.levels = r.vec<float>(static_cast<size_t>(c.width) * 3);
            c.density = r.vec<float>(static_cast<size_t>(c.width) * c.height);
            f.lanes.push_back(std::move(c));
        }
        if (r.bad()) {
            return f;
        }
    }
    f.ok = !top.bad();
    return f;
}

} // namespace phosphorfixture
