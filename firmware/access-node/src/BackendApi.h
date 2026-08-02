#pragma once

namespace BackendApi {

// GET /api/node/sync — refreshes AccessController's cache and the
// door-active flag. Returns false on network/HTTP failure, leaving the
// existing cache untouched so the node keeps enforcing whatever it last
// learned.
bool sync();

// POST /api/node/logs — uploads a batch of pending events and only acks
// them out of the queue once the server confirms receipt.
bool uploadLogs();

// POST /api/node/heartbeat
bool heartbeat();

}  // namespace BackendApi
