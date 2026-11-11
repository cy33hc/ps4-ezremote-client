#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "remote_pool.h"
#include "actions.h"

namespace
{
    std::mutex pool_mutex;
    std::map<std::string, std::vector<RemoteClient *>> pool_idle;
    std::map<RemoteClient *, std::string> pool_key;

    // Enough to overlap a couple of operations without exhausting the console's
    // session slots.
    const size_t POOL_MAX_PER_SITE = 6;

    // Live (checked out) connections per site: beyond this, callers wait so a
    // burst of requests cannot exhaust the server's session limit.
    // Transfers can hold a slot for minutes; keep enough headroom that
    // browsing and short commands are never starved.
    const int POOL_MAX_CHECKED_OUT = 8;
    std::condition_variable pool_cv;
    std::map<std::string, int> pool_checked_out;

    std::string PoolKey(const RemoteSettings &settings)
    {
        return std::string(settings.server) + "|" + settings.username + "|" +
               std::to_string(settings.type) + "|" + settings.http_server_type;
    }
}

namespace RemotePool
{
    static RemoteClient *AcquireLocked(const RemoteSettings &settings, const std::string &key);

    RemoteClient *Acquire(const RemoteSettings &settings)
    {
        const std::string key = PoolKey(settings);

        {
            std::unique_lock<std::mutex> lock(pool_mutex);
            // Bounded wait: a leaked connection must not deadlock the site.
            if (!pool_cv.wait_for(lock, std::chrono::seconds(60),
                    [&]{ return pool_checked_out[key] < POOL_MAX_CHECKED_OUT; }))
                return nullptr;
            pool_checked_out[key]++;
        }

        RemoteClient *client = AcquireLocked(settings, key);

        if (client == nullptr)
        {
            std::lock_guard<std::mutex> lock(pool_mutex);
            if (--pool_checked_out[key] <= 0)
                pool_checked_out.erase(key);
            pool_cv.notify_all();
        }

        return client;
    }

    static RemoteClient *AcquireLocked(const RemoteSettings &settings, const std::string &key)
    {
        while (true)
        {
            RemoteClient *candidate = nullptr;
            {
                std::lock_guard<std::mutex> lock(pool_mutex);
                std::vector<RemoteClient *> &idle = pool_idle[key];
                if (!idle.empty())
                {
                    candidate = idle.back();
                    idle.pop_back();
                }
            }

            if (candidate == nullptr)
                break;

            // The server may have dropped the session while it was idle.
            if (candidate->IsConnected())
                return candidate;

            candidate->Quit();
            delete candidate;
            std::lock_guard<std::mutex> lock(pool_mutex);
            pool_key.erase(candidate);
        }

        RemoteClient *client = Actions::CreateRemoteClient(const_cast<RemoteSettings *>(&settings));
        if (client == nullptr)
            return nullptr;

        if (!client->Connect(settings.server, settings.username, settings.password))
        {
            client->Quit();
            delete client;
            return nullptr;
        }

        {
            std::lock_guard<std::mutex> lock(pool_mutex);
            pool_key[client] = key;
        }
        return client;
    }

    void Release(RemoteClient *client, bool healthy)
    {
        if (client == nullptr)
            return;

        std::string key;
        {
            std::lock_guard<std::mutex> lock(pool_mutex);
            std::map<RemoteClient *, std::string>::iterator it = pool_key.find(client);
            if (it != pool_key.end())
                key = it->second;
        }

        bool pooled = false;
        // An empty key means the pool was cleared while this caller was busy:
        // the connection belongs to a closed session, so close it.
        if (!key.empty() && healthy && client->IsConnected())
        {
            std::lock_guard<std::mutex> lock(pool_mutex);
            std::vector<RemoteClient *> &idle = pool_idle[key];
            if (idle.size() < POOL_MAX_PER_SITE)
            {
                idle.push_back(client);
                pooled = true;
            }
        }

        if (!pooled)
        {
            client->Quit();
            delete client;
            std::lock_guard<std::mutex> lock(pool_mutex);
            pool_key.erase(client);
        }

        std::lock_guard<std::mutex> lock(pool_mutex);
        if (!key.empty() && --pool_checked_out[key] <= 0)
            pool_checked_out.erase(key);
        pool_cv.notify_all();
    }

    std::shared_ptr<RemoteClient> AcquireShared(const RemoteSettings &settings)
    {
        RemoteClient *client = Acquire(settings);
        if (client == nullptr)
            return nullptr;

        return std::shared_ptr<RemoteClient>(client, [](RemoteClient *released)
        {
            Release(released, true);
        });
    }

    void ClearFor(const RemoteSettings &settings)
    {
        const std::string key = PoolKey(settings);
        std::vector<RemoteClient *> closing;

        {
            std::lock_guard<std::mutex> lock(pool_mutex);
            std::vector<RemoteClient *> &idle = pool_idle[key];
            closing.swap(idle);
            pool_idle.erase(key);

            for (size_t i = 0; i < closing.size(); i++)
                pool_key.erase(closing[i]);
        }

        for (size_t i = 0; i < closing.size(); i++)
        {
            closing[i]->Quit();
            delete closing[i];
        }
    }

    void Clear()
    {
        std::map<std::string, std::vector<RemoteClient *>> idle;
        {
            std::lock_guard<std::mutex> lock(pool_mutex);
            idle.swap(pool_idle);
        }

        {
            std::lock_guard<std::mutex> lock(pool_mutex);
            pool_key.clear();
        }

        for (std::map<std::string, std::vector<RemoteClient *>>::iterator it = idle.begin(); it != idle.end(); ++it)
        {
            for (size_t i = 0; i < it->second.size(); i++)
            {
                it->second[i]->Quit();
                delete it->second[i];
            }
        }
    }
}
