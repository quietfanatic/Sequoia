#pragma once

#include "../util/json.h"
#include "../model/page.h"

namespace control {

void message_from_page (model::PageID, const json::Value&);

} // namespace control
