#pragma once

#include <QString>

class QWidget;

// DJNote: the one result dialog of every NML flow (import, multi-playlist
// import, re-read, export).

namespace mixxx {
namespace nml {

struct NmlSummaryParts {
    QString headline; // the counts, always visible
    QString details;  // the long lists, scrollable
};

/// Splits a summary at its first blank line: the first paragraph is the
/// headline, everything after it the details.
NmlSummaryParts splitNmlSummary(const QString& text);

/// Tallest the dialog may get: half the screen, but never below 300 px.
int nmlSummaryMaxHeight(int screenHeight);

/// Shows `text` with the headline on top and the details in a read-only,
/// selectable, scrollable area; `warning` picks the icon. Short texts get a
/// compact dialog.
void showNmlSummary(
        QWidget* pParent, const QString& title, const QString& text, bool warning = false);

} // namespace nml
} // namespace mixxx
