#pragma once
#include "page.h"

namespace model {
inline namespace page {

PageData* load_mut (PageID id);
PageID create_page (Str url, Str title = ""sv);

} // namespace page
} // namespace model
