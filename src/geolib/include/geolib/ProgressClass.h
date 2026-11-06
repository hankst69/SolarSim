#pragma once

#include <string>
#include <memory>

namespace geo {

    class ProgressClass {
    public:
        virtual ~ProgressClass() = default;
        virtual void progress(int percent, std::string msg) = 0;
    };

    using ProgressClassPtr = std::shared_ptr<ProgressClass>;

} // namespace geo
