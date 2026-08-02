#pragma once
#include <Arduino.h>

namespace BackendApi {

// GET /api/node/sync — refreshes AccessController's cache and the
// door-active flag. Returns false on network/HTTP failure, leaving the
// existing cache untouched so the node keeps enforcing whatever it last
// learned.
bool sync();

// GET /api/node/mode — cheap, high-frequency poll of just door_active +
// door_mode so a guard's mode change (or a hard disable) takes effect in
// seconds without re-fetching the whole credential list.
bool syncMode();

// POST /api/node/logs — uploads a batch of pending events and only acks
// them out of the queue once the server confirms receipt.
bool uploadLogs();

// POST /api/node/enroll — reports a raw card value read while
// enroll_armed was set, for the admin panel's "Leer tarjeta" button.
// Best-effort, fire-and-forget: nothing local depends on it succeeding.
bool reportEnrollment(const String &rawValue, uint8_t bitCount);

// POST /api/node/heartbeat
bool heartbeat();

}  // namespace BackendApi
