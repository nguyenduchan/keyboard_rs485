/*
 * Modbus RTU master (FW-12..15): hàng đợi ghi ưu tiên + một khe hỏi vòng, timeout/thử lại,
 * mã lỗi exception. Thuần logic, truyền qua rs485.h nên test được trên PC.
 */
#ifndef MB_MASTER_H
#define MB_MASTER_H
#include <stdint.h>
#include <stdbool.h>

typedef enum { MBM_OK = 0, MBM_TIMEOUT, MBM_CRC, MBM_EXC, MBM_FRAME } mbm_status_t;

typedef void (*mbm_cb_t)(void *ctx, mbm_status_t st, uint8_t exc, const uint8_t *data, uint8_t dlen);

typedef struct {
    uint8_t  fn;            /* 1,2,3,4 đọc; 5 ghi cuộn dây; 6 ghi 1 thanh ghi; 16 ghi nhiều thanh ghi (tối đa 2) */
    uint16_t addr;
    uint16_t qty;           /* đọc: số lượng; FC05: 0/1; FC06: bỏ qua; FC16: số thanh ghi (1..2) */
    uint16_t data[2];       /* dữ liệu ghi */
    mbm_cb_t cb;
    void    *ctx;
} mbm_req_t;

typedef struct {
    uint16_t ok, timeouts, crc_err, exceptions;
    uint8_t  last_err;      /* mbm_status_t của lỗi gần nhất */
    uint8_t  last_exc;
} mbm_stats_t;

void mbm_init(uint8_t station, uint16_t timeout_ms, uint8_t retries);
bool mbm_submit_urgent(const mbm_req_t *r);     /* ưu tiên, hàng đợi MB_URGENT_DEPTH; false nếu đầy */
bool mbm_submit_bg(const mbm_req_t *r);         /* một khe hỏi vòng; false nếu đang có yêu cầu nền chưa xong */
bool mbm_bg_free(void);
bool mbm_idle(void);                            /* không có giao dịch nào đang chạy hay chờ */
bool mbm_comm_ok(void);                         /* giao dịch gần nhất có phản hồi hợp lệ (kể cả exception) */
const mbm_stats_t *mbm_stats(void);
void mbm_poll(uint32_t now);

#endif
