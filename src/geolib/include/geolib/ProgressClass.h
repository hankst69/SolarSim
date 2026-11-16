#pragma once

#include <string>
#include <memory>

namespace geo {

    class ProgressClass {
    public:
        virtual ~ProgressClass() = default;
        virtual void progress(int percent, std::string msg) = 0;

        void cancel() { m_isCanceled = true; }
        bool isCanceled() const { return m_isCanceled; }
    private:
        bool m_isCanceled = false;
    };

    using ProgressClassPtr = std::shared_ptr<ProgressClass>;

} // namespace geo
