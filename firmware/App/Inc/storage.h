/*
 * Lưu khối cấu hình vào flash theo kiểu A/B (FW-64): mỗi lần lưu ghi sang trang còn lại,
 * chỉ "chốt" (ghi tiêu đề) sau khi toàn bộ dữ liệu đã ghi xong => mất điện giữa chừng vẫn còn bản cũ.
 */
#ifndef STORAGE_H
#define STORAGE_H
#include "rtcfg.h"
#include <stdbool.h>

typedef enum { ST_OK = 0, ST_NONE, ST_ERASE, ST_WRITE, ST_VERIFY } st_result_t;

st_result_t storage_load(rt_blob_t *out);          /* ST_OK nếu có bản hợp lệ (bản mới nhất), ST_NONE nếu không */
st_result_t storage_save(const rt_blob_t *b);      /* khối đã seal + hợp lệ */
uint32_t    storage_seq(void);                     /* số thứ tự bản đang dùng (0 = chưa có) */
bool        storage_blank(void);                   /* cả hai trang còn trống (flash mới xoá) */

#endif
