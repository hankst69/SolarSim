#pragma once

#include <string>

namespace app {

/// Qt-independent helper to download `url` into the local file `targetPath`
/// using Mongoose. Blocking, thread-safe, no event loop required.
/// Follows redirects, optionally sending HTTP Basic auth credentials.
bool fetchUrlMongoose(const std::string& url, const std::string& targetPath,
                      const std::string& userName, const std::string& password);

} // namespace app
