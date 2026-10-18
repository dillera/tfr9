#include "globals.h"
#include "network.h"

char url[128];
/**
 * @brief Http response buffer
 */
static char response_buffer[2048];

#define URL_BASE "N:HTTPS://FUJINET.ONLINE/8bitnews/news.php"

#define ARTICLES_URL_SPEC "%s?t=lf&ps=%dx%d&l=%u&p=%u&c=%s"
#define ARTICLE_URL_SPEC  "%s?t=lf&ps=%dx%d&p=%u&a=%lu"

void setup_url(bool articles)
{
    if (articles)
    {
        // The url for fetching the list of articles
         sprintf(url, ARTICLES_URL_SPEC,
            URL_BASE,
            textMode,
            rows,
            articles_per_page,
            articles_page,
            topic);
    }
    else
    {
        // The url for fetching a specific article
        sprintf(url, ARTICLE_URL_SPEC,
            URL_BASE,
            textMode - 1,
            rows,
            article_page,
            article_id);
    }
}

char *fetch_data(bool articles)
{
    setup_url(articles);

    uint16_t bytesWaiting;
    uint8_t connected;
    uint8_t error;
    unsigned int buf_offset = 0;

    memset(response_buffer, 0, sizeof(response_buffer));

    if (network_open(url, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE) != FN_ERR_OK)
        return response_buffer;
    network_status(url, &bytesWaiting, (uint8_t *) &connected, &error);

    while(error == 1 && bytesWaiting > 0)
    {
        if (buf_offset + bytesWaiting > sizeof(response_buffer) - 1)
            bytesWaiting = sizeof(response_buffer) - 1 - buf_offset;
        if (bytesWaiting == 0)
            break;

        network_read(url, (byte *)&response_buffer[0+buf_offset], bytesWaiting);
        buf_offset += bytesWaiting;
        network_status(url, &bytesWaiting, (uint8_t *) &connected, &error);

        if (strlen(fetching_buf) < 79)   // fetching_buf holds 80
            strcat(fetching_buf, ".");

        show_fetching_msg(false, articles);
    }

    network_close(url);

    return response_buffer;
}
