#include "preferences/dialog/scrollinglinesprefsgroup.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>

#include "moc_scrollinglinesprefsgroup.cpp"

using mixxx::scrollinglinelayout::LineSettings;
using mixxx::scrollinglinelayout::LineWidthPx;
using mixxx::scrollinglinelayout::OpacityPercent;

namespace {

void addExplanation(QGridLayout* pGrid, int& row, const QString& explanation) {
    auto* pExplanation = new QLabel(explanation);
    pExplanation->setWordWrap(true);
    pExplanation->setToolTip(explanation);
    QFont font = pExplanation->font();
    font.setItalic(true);
    pExplanation->setFont(font);
    pGrid->addWidget(pExplanation, row++, 0, 1, 3);
}

} // namespace

ScrollingLinesPrefsGroup::ScrollingLinesPrefsGroup(
        const QList<QWidget*>& upstreamBeatGridControls, QWidget* pParent)
        : QGroupBox(tr("Beat, bar and step lines"), pParent),
          m_upstreamBeatGridControls(upstreamBeatGridControls) {
    setObjectName(QStringLiteral("scrollingLinesGroupBox"));
    auto* pGrid = new QGridLayout(this);
    pGrid->setColumnStretch(1, 1);
    int row = 0;

    m_pEnabledCheckBox = new QCheckBox(tr("DJNote beat, bar and step lines"));
    m_pEnabledCheckBox->setObjectName(QStringLiteral("scrollingLinesEnabledCheckBox"));
    const QString enabledExplanation =
            tr("Draw bar lines on each downbeat and beat lines on the other beats "
               "of the scrolling waveforms, with the settings below. Off: the "
               "lines are drawn as in Mixxx, with the beat grid opacity above.");
    m_pEnabledCheckBox->setToolTip(enabledExplanation);
    pGrid->addWidget(m_pEnabledCheckBox, row++, 0, 1, 3);
    addExplanation(pGrid, row, enabledExplanation);

    m_barOpacity = addOpacityRow(pGrid,
            row,
            tr("Bar line opacity"),
            QStringLiteral("scrollingBarLineOpacity"),
            tr("Opacity of the line on each bar's first beat (downbeat)."));
    m_pBarWidthSpinBox = addWidthRow(pGrid,
            row,
            tr("Bar line width"),
            QStringLiteral("scrollingBarLineWidth"),
            tr("Width of the bar lines, in pixels."));
    m_barColor = addColorRow(pGrid,
            row,
            tr("Bar line colour"),
            QStringLiteral("scrollingBarLine"),
            tr("Colour of the bar lines: the skin's beat colour, or a colour "
               "of your choice."),
            &LineSettings::barColor);
    m_beatOpacity = addOpacityRow(pGrid,
            row,
            tr("Beat line opacity"),
            QStringLiteral("scrollingBeatLineOpacity"),
            tr("Opacity of the lines on the other beats."));
    m_pBeatWidthSpinBox = addWidthRow(pGrid,
            row,
            tr("Beat line width"),
            QStringLiteral("scrollingBeatLineWidth"),
            tr("Width of the beat lines, in pixels."));
    m_beatColor = addColorRow(pGrid,
            row,
            tr("Beat line colour"),
            QStringLiteral("scrollingBeatLine"),
            tr("Colour of the beat lines: the skin's beat colour, or a colour "
               "of your choice."),
            &LineSettings::beatColor);

    showSettings(m_settings);

    connect(m_pEnabledCheckBox,
            &QCheckBox::toggled,
            this,
            &ScrollingLinesPrefsGroup::slotControlChanged);
    for (const OpacityControls& controls : {m_barOpacity, m_beatOpacity}) {
        connect(controls.pSpinBox,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                &ScrollingLinesPrefsGroup::slotControlChanged);
    }
    for (QSpinBox* pSpinBox : {m_pBarWidthSpinBox, m_pBeatWidthSpinBox}) {
        connect(pSpinBox,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                &ScrollingLinesPrefsGroup::slotControlChanged);
    }
}

QSpinBox* ScrollingLinesPrefsGroup::addWidthRow(QGridLayout* pGrid,
        int& row,
        const QString& name,
        const QString& objectNamePrefix,
        const QString& explanation) {
    auto* pSpinBox = new QSpinBox();
    pSpinBox->setObjectName(objectNamePrefix + QStringLiteral("SpinBox"));
    pSpinBox->setRange(LineWidthPx::kMin, LineWidthPx::kMax);
    pSpinBox->setSuffix(tr(" px"));
    pSpinBox->setToolTip(explanation);
    auto* pLabel = new QLabel(name);
    pLabel->setBuddy(pSpinBox);
    pLabel->setToolTip(explanation);
    pGrid->addWidget(pLabel, row, 0);
    pGrid->addWidget(pSpinBox, row, 1, Qt::AlignLeft);
    ++row;
    return pSpinBox;
}

// objectNamePrefix + "ColorButton" / "SkinColorButton" / "ColorDialog".
ScrollingLinesPrefsGroup::ColorControls ScrollingLinesPrefsGroup::addColorRow(
        QGridLayout* pGrid,
        int& row,
        const QString& name,
        const QString& objectNamePrefix,
        const QString& explanation,
        ColorMember pColor) {
    ColorControls controls;
    controls.pColorButton = new QPushButton();
    controls.pColorButton->setObjectName(objectNamePrefix + QStringLiteral("ColorButton"));
    controls.pColorButton->setToolTip(explanation);
    controls.pSkinColorButton = new QPushButton(tr("Skin colour"));
    controls.pSkinColorButton->setObjectName(
            objectNamePrefix + QStringLiteral("SkinColorButton"));
    controls.pSkinColorButton->setToolTip(tr("Go back to the skin's beat colour."));
    controls.pDialog = new QColorDialog(this);
    controls.pDialog->setObjectName(objectNamePrefix + QStringLiteral("ColorDialog"));
    auto* pLabel = new QLabel(name);
    pLabel->setBuddy(controls.pColorButton);
    pLabel->setToolTip(explanation);
    pGrid->addWidget(pLabel, row, 0);
    pGrid->addWidget(controls.pColorButton, row, 1, Qt::AlignLeft);
    pGrid->addWidget(controls.pSkinColorButton, row, 2);
    ++row;

    connect(controls.pColorButton, &QPushButton::clicked, this, [this, pColor]() {
        openColorDialog(pColor);
    });
    connect(controls.pSkinColorButton, &QPushButton::clicked, this, [this, pColor]() {
        setColor(pColor, std::nullopt);
    });
    connect(controls.pDialog,
            &QColorDialog::colorSelected,
            this,
            [this, pColor](const QColor& color) {
                if (color.isValid()) {
                    setColor(pColor, color.toRgb());
                }
            });
    return controls;
}

ScrollingLinesPrefsGroup::ColorControls& ScrollingLinesPrefsGroup::colorControls(
        ColorMember pColor) {
    return pColor == &LineSettings::barColor ? m_barColor : m_beatColor;
}

void ScrollingLinesPrefsGroup::openColorDialog(ColorMember pColor) {
    QColorDialog* pDialog = colorControls(pColor).pDialog;
    pDialog->setCurrentColor((m_settings.*pColor).value_or(QColor(Qt::white)));
    pDialog->open();
}

void ScrollingLinesPrefsGroup::setColor(ColorMember pColor, const std::optional<QColor>& color) {
    m_settings.*pColor = color;
    showColor(colorControls(pColor), color);
    emit settingsChanged(m_settings);
}

void ScrollingLinesPrefsGroup::showColor(
        const ColorControls& controls, const std::optional<QColor>& color) {
    if (color) {
        QPixmap swatch(16, 16);
        swatch.fill(*color);
        controls.pColorButton->setIcon(QIcon(swatch));
        controls.pColorButton->setText(color->name(QColor::HexRgb));
    } else {
        controls.pColorButton->setIcon(QIcon());
        controls.pColorButton->setText(tr("Skin colour"));
    }
    controls.pSkinColorButton->setEnabled(color.has_value());
}

ScrollingLinesPrefsGroup::OpacityControls ScrollingLinesPrefsGroup::addOpacityRow(
        QGridLayout* pGrid,
        int& row,
        const QString& name,
        const QString& objectNamePrefix,
        const QString& explanation) {
    OpacityControls controls;
    controls.pSlider = new QSlider(Qt::Horizontal);
    controls.pSlider->setObjectName(objectNamePrefix + QStringLiteral("Slider"));
    controls.pSlider->setRange(0, 100);
    controls.pSpinBox = new QSpinBox();
    controls.pSpinBox->setObjectName(objectNamePrefix + QStringLiteral("SpinBox"));
    controls.pSpinBox->setRange(0, 100);
    controls.pSpinBox->setSuffix(QStringLiteral("%"));
    // The spin box carries the value; the slider follows it both ways.
    connect(controls.pSlider, &QSlider::valueChanged, controls.pSpinBox, &QSpinBox::setValue);
    connect(controls.pSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            controls.pSlider,
            &QSlider::setValue);

    auto* pLabel = new QLabel(name);
    pLabel->setBuddy(controls.pSlider);
    for (QWidget* pWidget : {static_cast<QWidget*>(pLabel),
                 static_cast<QWidget*>(controls.pSlider),
                 static_cast<QWidget*>(controls.pSpinBox)}) {
        pWidget->setToolTip(explanation);
    }
    pGrid->addWidget(pLabel, row, 0);
    pGrid->addWidget(controls.pSlider, row, 1);
    pGrid->addWidget(controls.pSpinBox, row, 2);
    ++row;
    return controls;
}

void ScrollingLinesPrefsGroup::showSettings(const LineSettings& settings) {
    m_settings = settings;
    {
        const QSignalBlocker blockEnabled(m_pEnabledCheckBox);
        m_pEnabledCheckBox->setChecked(settings.enabled);
    }
    const auto show = [](const OpacityControls& controls, OpacityPercent opacity) {
        const QSignalBlocker blockSlider(controls.pSlider);
        const QSignalBlocker blockSpinBox(controls.pSpinBox);
        controls.pSlider->setValue(opacity.percent());
        controls.pSpinBox->setValue(opacity.percent());
    };
    show(m_barOpacity, settings.barOpacity);
    show(m_beatOpacity, settings.beatOpacity);
    for (const auto& [pSpinBox, width] : {std::pair{m_pBarWidthSpinBox, settings.barWidth},
                 std::pair{m_pBeatWidthSpinBox, settings.beatWidth}}) {
        const QSignalBlocker blockSpinBox(pSpinBox);
        pSpinBox->setValue(width.px());
    }
    showColor(m_barColor, settings.barColor);
    showColor(m_beatColor, settings.beatColor);
    updateUpstreamControls();
}

void ScrollingLinesPrefsGroup::slotControlChanged() {
    m_settings.enabled = m_pEnabledCheckBox->isChecked();
    m_settings.barOpacity = OpacityPercent(m_barOpacity.pSpinBox->value());
    m_settings.beatOpacity = OpacityPercent(m_beatOpacity.pSpinBox->value());
    m_settings.barWidth = LineWidthPx(m_pBarWidthSpinBox->value());
    m_settings.beatWidth = LineWidthPx(m_pBeatWidthSpinBox->value());
    updateUpstreamControls();
    emit settingsChanged(m_settings);
}

void ScrollingLinesPrefsGroup::updateUpstreamControls() {
    for (QWidget* pControl : std::as_const(m_upstreamBeatGridControls)) {
        pControl->setEnabled(!m_settings.enabled);
    }
}
