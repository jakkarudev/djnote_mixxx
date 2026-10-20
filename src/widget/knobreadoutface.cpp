#include "widget/knobreadoutface.h"

#include <QFontMetrics>
#include <algorithm>

namespace mixxx {
namespace knobreadout {

std::optional<int> faceFontPixelSize(const QString& text, QFont font, QSize knobSize) {
    const double faceDiameter = std::min(knobSize.width(), knobSize.height());
    for (int pixelSize = kFaceMaxPixelSize; pixelSize >= kFaceMinPixelSize; --pixelSize) {
        font.setPixelSize(pixelSize);
        const QFontMetrics metrics(font);
        const double width = metrics.horizontalAdvance(text) + 2 * kFaceHorizontalPaddingPx;
        const double height = metrics.height() + 2 * kFaceVerticalPaddingPx;
        // A centred box fits in a circle when its diagonal does.
        if (width * width + height * height <= faceDiameter * faceDiameter) {
            return pixelSize;
        }
    }
    return std::nullopt;
}

} // namespace knobreadout
} // namespace mixxx
