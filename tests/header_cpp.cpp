/* The public header must compile as C++17 and expose stable version data. */
#include "maelys/json.h"

static_assert(MAELYS_JSON_ABI_VERSION == 1u, "unexpected ABI");
static_assert(MAELYS_JSON_VERSION_MAJOR == 0, "unexpected major version");
static_assert(MAELYS_JSON_MAXIMUM_DEPTH >= MAELYS_JSON_DEFAULT_MAXIMUM_DEPTH,
    "ceiling below default");

int main() {
    maelys_json_document_t *document = nullptr;
    maelys_json_error_t error{};
    const char text[] = "{\"a\":1}";
    if (maelys_json_document_parse(text, sizeof(text) - 1, MAELYS_JSON_PROFILE_RFC8259,
            nullptr, &document, &error) != MAELYS_JSON_OK) {
        return 1;
    }
    maelys_json_document_release(document);
    return 0;
}
