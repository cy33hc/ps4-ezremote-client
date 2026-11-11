#ifndef EZREMOTE_REMOTE_POOL_H
#define EZREMOTE_REMOTE_POOL_H

#include "clients/remote_client.h"
#include <memory>

#include "config.h"

// Keeps idle RemoteClient connections per site so worker threads can run
// remote operations concurrently instead of sharing one global client.
namespace RemotePool
{
    // Returns a connected client, or nullptr when the site cannot be reached.
    RemoteClient *Acquire(const RemoteSettings &settings);

    // Returns the client to its idle list; destroy it when the connection is
    // no longer usable (failed transfer, dropped session).
    void Release(RemoteClient *client, bool healthy);

    // Same as Acquire, but the connection returns itself to the pool when the
    // last owner is destroyed; used by streaming responses.
    std::shared_ptr<RemoteClient> AcquireShared(const RemoteSettings &settings);

    // Closes every idle connection, e.g. on disconnect or shutdown.
    void Clear();

    // Closes the idle connections of one site only.
    void ClearFor(const RemoteSettings &settings);
}

#endif
