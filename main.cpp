#include <sys/types.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif
#include <filesystem>
#include <microhttpd.h>
#include <stdio.h>
#include <string.h>
#include <string_view>

#define PORT 8888

namespace fs = std::filesystem;

static std::wstring walk_get_file_path(const char* const url)
{
    const size_t len = strlen(url);
    if (std::string_view(url, len).find_first_of(":\\?$") != std::string_view::npos) return {};
    if (*url != '/') return {};

    std::wstring p = L"\\\\?\\" + fs::current_path().native();
    const size_t start = p.size();
    p.append(url, url + len);
    size_t cur = 0;

    do
    {
        p[start + cur] = L'\\';
        ++cur;
        if (cur == len)
        {
            // empty last element
            return p;
        }
        const auto v = (const char*)memchr(url + cur, '/', len - cur);

        const auto el_end = (v == NULL ? len : v - url);
        const std::string_view sv(url + cur, el_end - cur);

        if (sv.empty()) return {};
        if (sv[0] == L'.') return {};
        cur = el_end;
    } while (cur != len);

    return p;
}

struct CloseFile
{
    FILE* f;

    ~CloseFile() { fclose(f); }
};

static const char* mime_type(std::wstring_view wsv)
{
    if (wsv.ends_with(L".html"))
        return "text/html";
    else if (wsv.ends_with(L".wasm"))
        return "application/wasm";
    else
        return NULL;
}

static enum MHD_Result answer_to_connection(void* cls,
                                            struct MHD_Connection* connection,
                                            const char* url,
                                            const char* method,
                                            const char* version,
                                            const char* upload_data,
                                            size_t* upload_data_size,
                                            void** con_cls)
{
    struct MHD_Response* response;
    enum MHD_Result ret;
    std::wstring file;
    int err = 0;
    (void)cls;              /* Unused. Silent compiler warning. */
    (void)version;          /* Unused. Silent compiler warning. */
    (void)upload_data;      /* Unused. Silent compiler warning. */
    (void)upload_data_size; /* Unused. Silent compiler warning. */
    (void)con_cls;          /* Unused. Silent compiler warning. */

    if (0 != strcmp(method, "GET"))
    {
        goto error;
    }

    printf("url: '%s'\n", url);
    file = walk_get_file_path(url);
    if (file.empty())
    {
        goto error;
    }
    else if (file.back() == L'\\')
    {
        file += L"index.html";
    }
    if (const auto f = _wfopen(file.c_str(), L"rb"))
    {
        CloseFile closer{f};
        if (err = fseek(f, 0, SEEK_END)) goto error;
        const auto sz = _ftelli64(f);
        if (sz < 0) goto error;
        char* const buf = (char*)malloc(sz);
        if (!buf) goto error;
        std::unique_ptr<char> p(buf);
        if (err = fseek(f, 0, SEEK_SET)) goto error;
        if (1 != fread(buf, sz, 1, f)) goto error;

        response = MHD_create_response_from_buffer(sz, (void*)buf, MHD_RESPMEM_MUST_FREE);
        if (!response) goto error;
        if (const auto mime = mime_type(file))
        {
            MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, mime);
        }
        p.release();
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }
    else
    {
        const auto errno_ = errno;
        if (errno_ == ENOENT || errno_ == EISDIR || errno_ == EACCES)
        {
            static const auto response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
            if (!response) abort();
            return MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        }
        printf("%S: %d\n", file.c_str(), errno_);
        goto error;
    }
error:
    const char* page = "<html><body>An error occurred.</body></html>";
    static const auto err_response = MHD_create_response_from_buffer(strlen(page), (void*)page, MHD_RESPMEM_PERSISTENT);
    if (!err_response) abort();
    return MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, err_response);
}
int main(void)
{
    struct MHD_Daemon* daemon;
    daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD | MHD_ALLOW_SUSPEND_RESUME,
                              PORT,
                              NULL,
                              NULL,
                              &answer_to_connection,
                              NULL,
                              MHD_OPTION_END);
    if (NULL == daemon) return 1;
    printf("http://localhost:%d\n", PORT);
    (void)getchar();
    MHD_stop_daemon(daemon);
    return 0;
}
