#include "waveform/phosphor/phosphorlanecache.h"

#include <cmath>
#include <thread>

namespace mixxx::phosphor {

namespace {
// Columns kept beyond the requested range, so that small scroll reversals
// don't recompute.
constexpr int64_t kRetainMargin = 256;
} // namespace

PhosphorLaneCache::PhosphorLaneCache(PhosphorDataPointer pData, const LaneParams& params)
        : m_pData(std::move(pData)),
          m_params(params),
          m_totalFramesStamp(m_pData ? m_pData->totalFrames() : 0) {
}

void PhosphorLaneCache::setParams(const LaneParams& params) {
    if (params == m_params) {
        return;
    }
    m_params = params;
    m_columns.clear();
}

int64_t PhosphorLaneCache::columnAtFrame(double frame) const {
    return static_cast<int64_t>(std::floor(frame / m_params.framesPerColumn));
}

double PhosphorLaneCache::centerOfColumn(int64_t k) const {
    return (static_cast<double>(k) + 0.5) * m_params.framesPerColumn;
}

void PhosphorLaneCache::retain(int64_t first, int64_t last) {
    for (auto it = m_columns.begin(); it != m_columns.end();) {
        if (it->first < first - kRetainMargin || it->first > last + kRetainMargin) {
            it = m_columns.erase(it);
        } else {
            ++it;
        }
    }
}

void PhosphorLaneCache::invalidateIfDataChanged() {
    // The track length is trimmed when decoding finishes; a column near the
    // end could have been clipped differently before.
    const SINT total = m_pData->totalFrames();
    if (total != m_totalFramesStamp) {
        m_totalFramesStamp = total;
        m_columns.clear();
    }
}

const LaneColumn* PhosphorLaneCache::column(int64_t k) {
    if (!m_pData) {
        return nullptr;
    }
    invalidateIfDataChanged();
    const auto it = m_columns.find(k);
    if (it != m_columns.end()) {
        return &it->second;
    }
    const double center = centerOfColumn(k);
    if (!isColumnReady(*m_pData, m_params, center)) {
        return nullptr;
    }
    return &m_columns.emplace(k, computeColumn(*m_pData, m_params, center)).first->second;
}

void PendingCacheBuild::start(Job job) {
    auto pState = std::make_shared<State>();
    m_pState = pState;
    std::thread([pState, job = std::move(job)]() {
        pState->result = job();
        pState->ready.store(true, std::memory_order_release);
    }).detach();
}

std::unique_ptr<PhosphorLaneCache> PendingCacheBuild::takeIfReady() {
    if (!m_pState || !m_pState->ready.load(std::memory_order_acquire)) {
        return nullptr;
    }
    auto pResult = std::move(m_pState->result);
    m_pState.reset();
    return pResult;
}

} // namespace mixxx::phosphor
