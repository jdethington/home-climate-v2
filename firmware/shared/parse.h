// firmware/shared/parse.h — downlink command parsing helpers (issue #24).
//
// Pure C string functions, no Arduino dependency — shared here (not in
// transport.cpp) so the native Unity tests can include them directly.
// The node firmware uses them to parse downlink JSON (OTA + rename);
// the shapes are fixed, so substring extraction is enough.

#pragma once

#include <cstring>

// Extract the string value of "key": "..." from a flat JSON-ish buffer.
// Returns true if found and the value fit in out (refuses to truncate).
inline bool extractStringValue(const char *buf, const char *key, char *out, size_t outLen)
{
    const char *k = strstr(buf, key);
    if (k == nullptr)
        return false;
    const char *open = strchr(k + strlen(key), '"'); // opening quote of the value
    if (open == nullptr)
        return false;
    const char *end = strchr(open + 1, '"'); // closing quote
    if (end == nullptr)
        return false;
    size_t n = (size_t)(end - (open + 1));
    if (n == 0 || n >= outLen)
        return false;
    memcpy(out, open + 1, n);
    out[n] = '\0';
    return true;
}

// A node_id is valid if it is indoor/outdoor + digits only (same rule
// as the portal and the Pi's naming endpoint — don't trust the wire).
inline bool validNodeId(const char *id)
{
    const char *num = nullptr;
    if (strncmp(id, "indoor", 6) == 0)
        num = id + 6;
    else if (strncmp(id, "outdoor", 7) == 0)
        num = id + 7;
    else
        return false;
    if (*num == '\0')
        return false;
    for (const char *p = num; *p != '\0'; p++)
    {
        if (*p < '0' || *p > '9')
            return false;
    }
    return true;
}
