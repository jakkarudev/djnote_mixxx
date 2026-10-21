#include "library/savedheaderstate.h"

#include <QByteArray>
#include <string>

#include "library/trackmodel.h"

namespace {

// Same setting as WTrackTableViewHeader.
const QString kHeaderStateSetting = QStringLiteral("header_state_pb");

} // namespace

SavedHeaderState::SavedHeaderState(TrackModel* pModel)
        : m_pModel(pModel),
          m_saved(false) {
    const QString saved = m_pModel->getModelSetting(kHeaderStateSetting);
    if (saved.isEmpty()) {
        return;
    }
    const QByteArray bytes = QByteArray::fromBase64(saved.toLatin1());
    m_saved = m_state.ParseFromArray(bytes.constData(), static_cast<int>(bytes.size()));
    if (!m_saved) {
        m_state.Clear();
    }
}

int SavedHeaderState::visualIndexOfName(const QString& name) const {
    const std::string columnName = name.toStdString();
    for (int vi = 0; vi < m_state.header_state_size(); ++vi) {
        if (m_state.header_state(vi).column_name() == columnName) {
            return vi;
        }
    }
    return -1;
}

int SavedHeaderState::visualIndexOfLogicalIndex(int logicalIndex) const {
    for (int vi = 0; vi < m_state.header_state_size(); ++vi) {
        if (m_state.header_state(vi).logical_index() == logicalIndex) {
            return vi;
        }
    }
    return -1;
}

void SavedHeaderState::appendColumn(int logicalIndex, int size, bool hidden) {
    auto* pHeader = m_state.add_header_state();
    pHeader->set_hidden(hidden);
    pHeader->set_size(size);
    pHeader->set_logical_index(logicalIndex);
    pHeader->set_visual_index(m_state.header_state_size() - 1);
}

void SavedHeaderState::insertColumn(
        int visualIndex, int logicalIndex, int size, const QString& name) {
    const int savedColumnCount = m_state.header_state_size();
    for (int vi = 0; vi < savedColumnCount; ++vi) {
        auto* pHeader = m_state.mutable_header_state(vi);
        if (pHeader->logical_index() >= logicalIndex) {
            pHeader->set_logical_index(pHeader->logical_index() + 1);
        }
    }
    if (m_state.sort_indicator_shown() && m_state.sort_indicator_section() >= logicalIndex) {
        m_state.set_sort_indicator_section(m_state.sort_indicator_section() + 1);
    }

    auto* pHeader = m_state.add_header_state();
    pHeader->set_hidden(false);
    pHeader->set_size(size);
    pHeader->set_logical_index(logicalIndex);
    if (!name.isEmpty()) {
        pHeader->set_column_name(name.toStdString());
    }
    for (int vi = savedColumnCount; vi > visualIndex; --vi) {
        m_state.mutable_header_state()->SwapElements(vi, vi - 1);
    }
    for (int vi = 0; vi < m_state.header_state_size(); ++vi) {
        m_state.mutable_header_state(vi)->set_visual_index(vi);
    }
}

void SavedHeaderState::moveColumn(int fromLogicalIndex, int toLogicalIndex) {
    const auto moved = [fromLogicalIndex, toLogicalIndex](int logicalIndex) {
        if (logicalIndex == fromLogicalIndex) {
            return toLogicalIndex;
        }
        if (fromLogicalIndex < logicalIndex && logicalIndex <= toLogicalIndex) {
            return logicalIndex - 1;
        }
        if (toLogicalIndex <= logicalIndex && logicalIndex < fromLogicalIndex) {
            return logicalIndex + 1;
        }
        return logicalIndex;
    };
    for (int vi = 0; vi < m_state.header_state_size(); ++vi) {
        auto* pHeader = m_state.mutable_header_state(vi);
        pHeader->set_logical_index(moved(pHeader->logical_index()));
    }
    if (m_state.sort_indicator_shown()) {
        m_state.set_sort_indicator_section(moved(m_state.sort_indicator_section()));
    }
}

void SavedHeaderState::setName(int visualIndex, const QString& name) {
    m_state.mutable_header_state(visualIndex)->set_column_name(name.toStdString());
}

void SavedHeaderState::save() {
    const std::string bytes = m_state.SerializeAsString();
    m_pModel->setModelSetting(kHeaderStateSetting,
            QString(QByteArray(bytes.data(), static_cast<int>(bytes.size())).toBase64()));
    m_saved = true;
}
