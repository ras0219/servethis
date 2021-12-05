#include <sys/types.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif
#include <microhttpd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PORT 8888

#ifndef _WIN32
typedef char path_char;
#define PATH_SEP '/'
#define PATH(X) X
#define FOPEN_PATH(P, M) fopen((P), M)
#define PATH_FMT_SPEC "%s"
#define FTELL64(F) ftello64(F)
#else
typedef wchar_t path_char;
#define PATH_SEP L'\\'
#define PATH(X) L##X
#define FOPEN_PATH(P, M) _wfopen((P), L##M)
#define PATH_FMT_SPEC "%S"
#define FTELL64(F) _ftelli64(F)
#endif

#define PATH_BUF_LEN 1024
struct PathBuffer
{
    size_t prefix_size;
    path_char buf[PATH_BUF_LEN];
};

// @returns code units written to buf, -1 on error
static ssize_t walk_get_file_path(const char* const url, path_char* const buf, const size_t buf_len)
{
    const size_t len = strlen(url);
    if (len > buf_len) return -1;
    if (*url != '/') return -1;
    for (size_t i = 0; i < 4; ++i)
    {
        if (memchr(url, (":\\?$")[i], len))
        {
            return -1;
        }
    }

    for (size_t i = 1; i < len; ++i)
    {
        buf[i] = url[i];
    }

    size_t cur = 0;
    do
    {
#ifdef _WIN32
        buf[cur] = '\\';
#endif
        ++cur;
        if (cur == len)
        {
            // empty last element
            return len;
        }
        if (url[cur] == '.' || url[cur] == '/') return -1;
        const char* const v = (const char*)memchr(url + cur, '/', len - cur);
        cur = v == NULL ? len : v - url;
    } while (cur != len);

    return len;
}

// @returns mime type string if detected, NULL on error
static const char* mime_type(const path_char* p, size_t const len)
{
    if (len > 5)
    {
        if (0 == memcmp(PATH(".html"), p + len - 5, 5 * sizeof(path_char))) return "text/html";
        if (0 == memcmp(PATH(".wasm"), p + len - 5, 5 * sizeof(path_char))) return "application/wasm";
    }
    return NULL;
}

#define DEFAULT_FILE_LEN 10
const path_char s_default_file[DEFAULT_FILE_LEN + 1] = PATH("index.html");

struct MHD_Response* s_error_response;
struct MHD_Response* s_404_response;

static enum MHD_Result answer_to_connection(void* cls,
                                            struct MHD_Connection* connection,
                                            const char* url,
                                            const char* method,
                                            const char* version,
                                            const char* upload_data,
                                            size_t* upload_data_size,
                                            void** con_cls)
{
    enum MHD_Result ret;
    struct MHD_Response* response = NULL;
    int err;
    struct PathBuffer* const buf = (struct PathBuffer*)cls;
    FILE* f = NULL;
    char* file_buf = NULL;

    (void)version;          /* Unused. Silent compiler warning. */
    (void)upload_data;      /* Unused. Silent compiler warning. */
    (void)upload_data_size; /* Unused. Silent compiler warning. */
    (void)con_cls;          /* Unused. Silent compiler warning. */

    if (0 != strcmp(method, "GET")) goto error;

    printf("url: '%s'\n", url);

    ssize_t path_len =
        walk_get_file_path(url, buf->buf + buf->prefix_size, PATH_BUF_LEN - 1 - DEFAULT_FILE_LEN - buf->prefix_size);
    if (path_len == -1) goto error;
    path_len += buf->prefix_size;
    if (buf->buf[path_len - 1] == PATH_SEP)
    {
        memcpy(buf->buf + path_len, s_default_file, DEFAULT_FILE_LEN * sizeof(path_char));
        path_len += DEFAULT_FILE_LEN;
    }
    buf->buf[path_len] = 0;
    f = FOPEN_PATH(buf->buf, "rb");
    if (f)
    {
        if (err = fseek(f, 0, SEEK_END)) goto error;
        const int64_t sz = FTELL64(f);
        if (sz < 0) goto error;
        file_buf = (char*)malloc(sz);
        if (!file_buf) goto error;
        if (err = fseek(f, 0, SEEK_SET)) goto error;
        if (1 != fread(file_buf, sz, 1, f)) goto error;

        response = MHD_create_response_from_buffer(sz, (void*)file_buf, MHD_RESPMEM_MUST_FREE);
        if (!response) goto error;
        // file_buf is now owned by MHD
        file_buf = NULL;
        const char* const mime = mime_type(buf->buf, path_len);
        if (mime)
        {
            if (MHD_NO == MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, mime)) goto error;
        }
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        goto cleanup;
    }
    else
    {
        const int errno_ = errno;
        if (errno_ == ENOENT || errno_ == EISDIR || errno_ == EACCES)
        {
            ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, s_404_response);
            goto cleanup;
        }
        printf(PATH_FMT_SPEC ": %d\n", buf->buf, errno_);
        goto error;
    }

error:
    ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, s_error_response);

cleanup:
    if (file_buf) free(file_buf);
    if (f) fclose(f);
    if (response) MHD_destroy_response(response);
    return ret;
}
#ifndef WIN32
static void catcher(int sig) {}

static void ignore_sigpipe()
{
    struct sigaction oldsig;
    struct sigaction sig;

    sig.sa_handler = &catcher;
    sigemptyset(&sig.sa_mask);
#ifdef SA_INTERRUPT
    sig.sa_flags = SA_INTERRUPT; /* SunOS */
#else
    sig.sa_flags = SA_RESTART;
#endif
    if (0 != sigaction(SIGPIPE, &sig, &oldsig))
        fprintf(stderr, "Failed to install SIGPIPE handler: %s\n", strerror(errno));
}
#endif

int main(void)
{
#ifndef WIN32
    ignore_sigpipe();
#endif
    struct PathBuffer buf;
#ifdef _WIN32
    memcpy(buf.buf, L"\\\\?\\", 8);
    int written = GetCurrentDirectoryW(ARRAYSIZE(buf.buf) - 4, buf.buf + 4);
    if (written == 0) abort();
    buf.prefix_size = written + 4;
#else
#error TODO
#endif
    // Ensure enough reserved size
    if (buf.prefix_size + DEFAULT_FILE_LEN + 1 >= PATH_BUF_LEN) abort();

    // Create static responses
    s_404_response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
    if (!s_404_response) abort();
    const char* error_page = "<html><body>An error occurred.</body></html>";
    s_error_response = MHD_create_response_from_buffer(strlen(error_page), (void*)error_page, MHD_RESPMEM_PERSISTENT);
    if (!s_error_response) abort();

    struct MHD_Daemon* daemon;
    daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD | MHD_ALLOW_SUSPEND_RESUME,
                              PORT,
                              NULL,
                              NULL,
                              &answer_to_connection,
                              &buf,
                              MHD_OPTION_END);
    if (NULL == daemon) return 1;
    printf("http://localhost:%d\n", PORT);
    (void)getchar();
    MHD_stop_daemon(daemon);
    return 0;
}
