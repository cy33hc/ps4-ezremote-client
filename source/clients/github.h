#ifndef EZ_GITHUB_H
#define EZ_GITHUB_H

#include <string>
#include <vector>
#include "http/httplib.h"
#include "clients/remote_client.h"
#include "clients/baseclient.h"
#include "common.h"

class GithubClient : public BaseClient
{
public:
    int Connect(const std::string &url, const std::string &username, const std::string &password);
    std::vector<DirEntry> ListDir(const std::string &path);
    int Size(const std::string &path, int64_t *size);
    int Get(const std::string &outputfile, const std::string &path, uint64_t offset=0);
    int Get(SplitFile *split_file, const std::string &path, uint64_t offset=0);
    int GetRange(const std::string &path, void *buffer, uint64_t size, uint64_t offset);
    int GetRange(const std::string &path, DataSink &sink, uint64_t size, uint64_t offset);
    int Head(const std::string &path, void *buffer, uint64_t len);

private:
    struct GitAsset
    {
        std::string name;
        std::string url;
        DateTime modified;
        uint64_t size;
    };

    struct GitRelease
    {
        std::string name;
        DateTime modified;
    };

    std::vector<GitRelease> m_releases;
    std::map<std::string, std::map<std::string, GitAsset>> m_assets;
    bool releases_parsed = false;
    BaseClient m_client;

    bool ParseReleases();
};

#endif