/*
 * Modbus RTU slave (thuần C, không đụng phần cứng). Truy cập dữ liệu qua bảng hàm `mbs_ops_t`.
 * Hỗ trợ FC01, 02, 03, 04, 05, 06, 15, 16. Địa chỉ 0 (broadcast): chỉ thực hiện lệnh ghi, không trả lời.
 */
#ifndef MB_SLAVE_H
#define MB_SLAVE_H
#include <stdint.h>
#include <stdbool.h>

#define MB_MAX_ADU      256u
#define MB_EX_FUNCTION  0x01u
#define MB_EX_ADDRESS   0x02u
#define MB_EX_VALUE     0x03u
#define MB_EX_DEVICE    0x04u
#define MB_EX_BUSY      0x06u

typedef struct {
    /* các hàm trả về mã exception (0 = OK) */
    uint8_t (*rd_reg)(uint16_t addr, uint16_t *v, bool input);
    uint8_t (*chk_wr_reg)(uint16_t addr, uint16_t v);   /* kiểm tra trước khi ghi nhiều thanh ghi (không có tác dụng phụ) */
    uint8_t (*wr_reg)(uint16_t addr, uint16_t v);
    uint8_t (*rd_bit)(uint16_t addr, uint8_t *v, bool discrete);
    uint8_t (*wr_bit)(uint16_t addr, uint8_t v);
} mbs_ops_t;

/* Xử lý một khung; trả về độ dài phản hồi (gồm CRC) hoặc 0 nếu không cần trả lời. `resp` >= MB_MAX_ADU byte. */
uint16_t mbs_process(uint8_t my_id, const uint8_t *req, uint16_t len, uint8_t *resp, const mbs_ops_t *ops);

#endif
