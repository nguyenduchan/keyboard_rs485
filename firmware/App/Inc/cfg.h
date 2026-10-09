/* Bản cấu hình đang chạy (RAM) và thao tác nạp / lưu flash. */
#ifndef CFG_H
#define CFG_H
#include "rtcfg.h"
#include "storage.h"

const rt_blob_t *cfg(void);
rt_blob_t       *cfg_mut(void);

/* Nạp từ flash. true = có bản hợp lệ; false = dùng cấu hình xuất xưởng (cfg_load_status() cho biết lý do). */
bool     cfg_load(void);
uint8_t  cfg_load_status(void);         /* 0 = tốt, 1 = chưa có (flash trống), 2 = hỏng */
void     cfg_factory(void);             /* đặt lại mặc định trong RAM (chưa lưu) */
/* Tính lại CRC, kiểm tra rồi lưu. Trả về 0 = OK; 1..10 = RT_E_*; 0x10 + st_result_t = lỗi flash. */
uint8_t  cfg_save(void);
/* Như cfg_save nhưng KHÔNG tính lại CRC: kiểm tra khối do PC gửi đúng như đã nhận (phát hiện lỗi truyền). */
uint8_t  cfg_apply(void);
bool     cfg_dirty(void);               /* đã sửa trong RAM mà chưa lưu */
void     cfg_touch(void);               /* đánh dấu đã sửa */

#endif
