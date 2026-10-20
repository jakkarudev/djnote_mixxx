#include "waveform/phosphor/phosphoroverview.h"

#include <algorithm>
#include <cmath>

#include "util/assert.h"

namespace mixxx::phosphor {

namespace {

// Research 9: |rgb| = W . (r, g, b, b^4) * |levels|^level_exponent * d^0.25.
constexpr double kWeightR = 83.5;
constexpr double kWeightG = 242.0;
constexpr double kWeightB = -37.4;
constexpr double kWeightB4 = 606.1;
constexpr double kAlphaPerGreen = 2.0;

// On-screen colours (research 10): the colour is
// gain * balance * ((p - desat * min(p)) / 255)^exponent per channel, divided by
// its largest channel if that passes 255 so a bright pixel keeps its hue; its
// coverage (alpha) is (coverGain * max(p) / 255)^coverPower, clipped to 1, so a
// column fades from the centre into the background at its edges.
constexpr double kScreenGain = 114.0;
constexpr double kScreenExponent = 2.5;
constexpr double kScreenDesat = 0.32;
constexpr double kScreenBalance[3] = {1.0, 0.79, 1.07}; // R, G, B
constexpr double kScreenCoverGain = 3.6;
constexpr double kScreenCoverPower = 2.18;

inline double clamp255(double v) {
    return std::clamp(v, 0.0, 255.0);
}

} // namespace

QImage overviewImage(const OverviewColumnsView& columns,
        const PhosphorOverviewSettings& settings,
        int height) {
    DEBUG_ASSERT(height > 0);
    // 62 rows, per channel B, G, R, A, as floats.
    std::vector<double> src(static_cast<size_t>(kOverviewRows) * kOverviewColumns * 4, 0.0);
    const int done = std::clamp(columns.columnsDone, 0, kOverviewColumns);
    // A calibrated floor (research 9): about one sample of the two-column window.
    const double halfSample = 0.5 / std::max(1, columns.samplesPerColumn);
    for (int c = 0; c < done; ++c) {
        const uint8_t* density = columns.density + static_cast<size_t>(c) * kOverviewRows;
        int top = -1;
        int bottom = -1;
        for (int r = 0; r < kOverviewRows; ++r) {
            if (density[r] != 0) {
                if (top < 0) {
                    top = r;
                }
                bottom = r;
            }
        }
        if (top < 0) {
            continue;
        }
        const uint8_t* lv8 = columns.levels + static_cast<size_t>(c) * 3;
        if (top == bottom && top == kOverviewCenterRow && lv8[0] == 0 && lv8[1] == 0 &&
                lv8[2] == 0) {
            // Digital silence (judged from the audio, not the gained levels): an
            // opaque black pixel one row above the row map's center, as Traktor
            // draws it (row 30, research 9).
            src[(static_cast<size_t>(kOverviewCenterRow - 1) * kOverviewColumns + c) * 4 + 3] = 255.0;
            continue;
        }
        const double lr = lv8[0] / 255.0 * settings.gainLow;
        const double lg = lv8[1] / 255.0 * settings.gainMid;
        const double lb = lv8[2] / 255.0 * settings.gainHigh;
        const double norm = std::sqrt(lr * lr + lg * lg + lb * lb);
        if (norm <= 0) {
            continue; // all gains at 0: the column is dark
        }
        const double ur = lr / norm;
        const double ug = lg / norm;
        const double ub = lb / norm;
        const double mag = (kWeightR * ur + kWeightG * ug + kWeightB * ub +
                                   kWeightB4 * std::pow(ub, 4)) *
                std::pow(norm, settings.levelExponent) * settings.brightness;
        for (int r = top; r <= bottom; ++r) {
            double* px = &src[(static_cast<size_t>(r) * kOverviewColumns + c) * 4];
            const double d = std::max(kDensityFull *
                                    std::pow(density[r] / 255.0, 1.0 / kDensityStorePower),
                    halfSample);
            const double k = mag * std::pow(d, settings.densityExponent);
            px[0] = clamp255(ub * k);
            px[1] = clamp255(ug * k);
            px[2] = clamp255(ur * k);
            px[3] = clamp255(kAlphaPerGreen * ug * k);
        }
    }

    QImage image(kOverviewColumns, height, QImage::Format_ARGB32);
    std::vector<double> acc(static_cast<size_t>(kOverviewColumns) * 4);
    for (int y = 0; y < height; ++y) {
        // Area average of the source rows covered by output row y.
        const double lo = static_cast<double>(y) * kOverviewRows / height;
        const double hi = lo + static_cast<double>(kOverviewRows) / height;
        std::fill(acc.begin(), acc.end(), 0.0);
        for (int r = static_cast<int>(std::floor(lo)); r < kOverviewRows && r < hi; ++r) {
            const double w = (std::min(hi, r + 1.0) - std::max(lo, static_cast<double>(r))) *
                    height / kOverviewRows;
            if (w <= 0) {
                continue;
            }
            const double* row = &src[static_cast<size_t>(r) * kOverviewColumns * 4];
            for (size_t i = 0; i < acc.size(); ++i) {
                acc[i] += row[i] * w;
            }
        }
        uint8_t* out = image.scanLine(y);
        for (size_t i = 0; i < acc.size(); ++i) {
            out[i] = roundU8(acc[i]);
        }
    }
    return image;
}

QImage overviewDisplayImage(const QImage& overview) {
    const QImage in = overview.convertToFormat(QImage::Format_ARGB32);
    QImage out(in.size(), QImage::Format_ARGB32_Premultiplied);
    const int w = in.width();
    const int h = in.height();
    for (int y = 0; y < h; ++y) {
        const QRgb* src = reinterpret_cast<const QRgb*>(in.constScanLine(y));
        QRgb* dst = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb p = src[x];
            if (p == 0) {
                dst[x] = 0;
                continue;
            }
            const double c[3] = {static_cast<double>(qRed(p)),
                    static_cast<double>(qGreen(p)),
                    static_cast<double>(qBlue(p))};
            const double brightest = std::max({c[0], c[1], c[2]});
            if (brightest <= 0) {
                // Alpha without colour (the silence line): opaque black.
                dst[x] = qRgba(0, 0, 0, qAlpha(p) > 0 ? 255 : 0);
                continue;
            }
            const double grey = kScreenDesat * std::min({c[0], c[1], c[2]});
            double pre[3];
            double top = 0;
            for (int i = 0; i < 3; ++i) {
                const double v = std::max(c[i] - grey, 0.0);
                pre[i] = kScreenGain * kScreenBalance[i] *
                        std::pow(v / 255.0, kScreenExponent) * 255.0;
                top = std::max(top, pre[i]);
            }
            const double scale = top > 255.0 ? 255.0 / top : 1.0;
            const double cover = std::pow(
                    std::min(1.0, kScreenCoverGain * brightest / 255.0), kScreenCoverPower);
            const uint8_t a = roundU8(cover * 255.0);
            if (a == 0) {
                dst[x] = 0;
                continue;
            }
            // The straight colour, rounded as the reference script does, then
            // premultiplied.
            const auto premul = [a](uint8_t v) {
                return static_cast<int>((v * a + 127) / 255);
            };
            dst[x] = qRgba(premul(roundU8(pre[0] * scale)),
                    premul(roundU8(pre[1] * scale)),
                    premul(roundU8(pre[2] * scale)),
                    a);
        }
    }
    return out;
}

int overviewExtentRows(const OverviewColumnsView& columns) {
    const int done = std::clamp(columns.columnsDone, 0, kOverviewColumns);
    int extent = 0;
    for (int c = 0; c < done; ++c) {
        const uint8_t* density = columns.density + static_cast<size_t>(c) * kOverviewRows;
        for (int r = 0; r < kOverviewRows; ++r) {
            if (density[r] != 0) {
                // Row r above the center is 31 - r rows from it, below it r - 30.
                extent = std::max(extent, kOverviewCenterRow - r);
                break;
            }
        }
        for (int r = kOverviewRows - 1; r >= 0; --r) {
            if (density[r] != 0) {
                extent = std::max(extent, r - (kOverviewCenterRow - 1));
                break;
            }
        }
    }
    return extent;
}

PhosphorOverviewData::PhosphorOverviewData(int sampleRate, SINT totalFrames)
        : m_sampleRate(sampleRate),
          m_totalFrames(totalFrames),
          m_samplesPerColumn(static_cast<int>(
                  std::max<SINT>(1, (totalFrames + kOverviewColumns - 1) / kOverviewColumns))),
          m_levels(static_cast<size_t>(kOverviewColumns) * 3, 0),
          m_density(static_cast<size_t>(kOverviewColumns) * kOverviewRows, 0),
          m_completed(0) {
}

OverviewColumnsView PhosphorOverviewData::view() const {
    OverviewColumnsView v;
    v.levels = m_levels.data();
    v.density = m_density.data();
    v.samplesPerColumn = m_samplesPerColumn;
    v.columnsDone = completedColumns();
    return v;
}

namespace {
// Sanity bounds of stored data.
constexpr uint32_t kMaxStoredSampleRate = 1000000;
constexpr uint64_t kMaxStoredFrames = uint64_t(1) << 40;
constexpr int kHeaderBytes = 4 + 8;
constexpr int kLevelBytes = kOverviewColumns * 3;
constexpr int kDensityBytes = kOverviewColumns * kOverviewRows;
} // namespace

QByteArray PhosphorOverviewData::toByteArray() const {
    QByteArray bytes;
    bytes.reserve(kHeaderBytes + kLevelBytes + kDensityBytes);
    for (int i = 0; i < 4; ++i) {
        bytes.append(static_cast<char>((static_cast<uint32_t>(m_sampleRate) >> (8 * i)) & 0xFF));
    }
    for (int i = 0; i < 8; ++i) {
        bytes.append(static_cast<char>((static_cast<uint64_t>(m_totalFrames) >> (8 * i)) & 0xFF));
    }
    bytes.append(reinterpret_cast<const char*>(m_levels.data()), kLevelBytes);
    bytes.append(reinterpret_cast<const char*>(m_density.data()), kDensityBytes);
    return bytes;
}

// static
std::shared_ptr<PhosphorOverviewData> PhosphorOverviewData::fromByteArray(
        const QByteArray& bytes) {
    if (bytes.size() != kHeaderBytes + kLevelBytes + kDensityBytes) {
        return nullptr;
    }
    uint32_t rate = 0;
    uint64_t frames = 0;
    for (int i = 0; i < 4; ++i) {
        rate |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[i])) << (8 * i);
    }
    for (int i = 0; i < 8; ++i) {
        frames |= static_cast<uint64_t>(static_cast<uint8_t>(bytes[4 + i])) << (8 * i);
    }
    if (rate == 0 || rate > kMaxStoredSampleRate || frames == 0 || frames > kMaxStoredFrames) {
        return nullptr;
    }
    auto pData = std::make_shared<PhosphorOverviewData>(
            static_cast<int>(rate), static_cast<SINT>(frames));
    std::copy_n(bytes.constData() + kHeaderBytes,
            kLevelBytes,
            reinterpret_cast<char*>(pData->writableLevels(0)));
    std::copy_n(bytes.constData() + kHeaderBytes + kLevelBytes,
            kDensityBytes,
            reinterpret_cast<char*>(pData->writableDensity(0)));
    pData->setCompletedColumns(kOverviewColumns);
    return pData;
}

} // namespace mixxx::phosphor
