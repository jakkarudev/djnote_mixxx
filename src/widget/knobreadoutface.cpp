#include "widget/knobreadoutface.h"

#include <QFontMetrics>
#include <algorithm>

namespace mixxx {
namespace knobreadout {

std::optional<int> faceFontPixelSize(const TextMeasure& measureText, QSize knobSize) {
    const double faceDiameter = std::min(knobSize.width(), knobSize.height());
    for (int pixelSize = kFaceMaxPixelSize; pixelSize >= kFaceMinPixelSize; --pixelSize) {
        const QSizeF textSize = measureText(pixelSize);
        const double width = textSize.width() + 2 * kFaceHorizontalPaddingPx;
        const double height = textSize.height() + 2 * kFaceVerticalPaddingPx;
        // A centred box fits in a circle when its diagonal does.
        if (width * width + height * height <= faceDiameter * faceDiameter) {
            return pixelSize;
        }
    }
    return std::nullopt;
}

std::optional<int> faceFontPixelSize(const QString& text, QFont font, QSize knobSize) {
    return faceFontPixelSize(
            [&text, &font](int pixelSize) {
                font.setPixelSize(pixelSize);
                const QFontMetrics metrics(font);
                return QSizeF(metrics.horizontalAdvance(text), metrics.height());
            },
            knobSize);
}

} // namespace knobreadout
} // namespace mixxx
