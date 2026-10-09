/*
 * Khối cấu hình runtime ("file chạy") — cấu trúc nhị phân little-endian, cố định kích thước,
 * có số phiên bản + CRC32. PC app biên dịch dự án thành khối này rồi nạp xuống (docs/RUNTIME_FORMAT.md).
 * Mọi địa chỉ Modbus ở đây là địa chỉ THÔ, bắt đầu từ 0 (PC app đã quy đổi theo hãng PLC).
 */
#ifndef RTCFG_H
#define RTCFG_H
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define RT_MAGIC            0x54524B50u     /* byte 'P','K','R','T' */
#define RT_SCHEMA           1u

/* ---- loại đối tượng ---- */
enum { RT_OBJ_NONE = 0, RT_OBJ_SHOW, RT_OBJ_INPUT, RT_OBJ_LABEL, RT_OBJ_BIT };
/* ---- vùng nhớ PLC (kèm lệnh Modbus) ---- */
enum { RT_AREA_HOLD = 0, RT_AREA_INPUT, RT_AREA_COIL, RT_AREA_DISC };      /* 03/06/16, 04, 01/05, 02 */
/* ---- kiểu dữ liệu ---- */
enum { RT_DT_U16 = 0, RT_DT_I16, RT_DT_U32, RT_DT_I32, RT_DT_BCD16, RT_DT_BCD32, RT_DT_F32, RT_DT_COUNT };
/* ---- thứ tự word/byte cho 32 bit; 0 = theo mặc định chung ---- */
enum { RT_ORD_INHERIT = 0, RT_ORD_ABCD, RT_ORD_CDAB, RT_ORD_BADC, RT_ORD_DCBA };
/* ---- cờ đối tượng ---- */
#define RT_OF_WIDE          0x01u   /* giá trị ghép cả 8 số (dùng đối tượng trên, bỏ đối tượng dưới) */
#define RT_OF_VERIFY        0x02u   /* đọc lại xác nhận sau khi ghi */
#define RT_OF_ZEROS         0x04u   /* hiện số 0 đứng đầu */
#define RT_BIT_NONE         0xFFu

/* ---- phím chức năng ---- */
enum { RT_FA_NONE = 0, RT_FA_MOMENTARY, RT_FA_TOGGLE, RT_FA_SET, RT_FA_RESET, RT_FA_WRITE,
       RT_FA_GOTO, RT_FA_NEXT, RT_FA_PREV, RT_FA_COUNT };
enum { RT_FAREA_COIL = 0, RT_FAREA_HOLDBIT, RT_FAREA_HOLDWORD };
/* ---- LED ---- */
enum { RT_LM_OFF = 0, RT_LM_STEADY, RT_LM_BLINK, RT_LM_COMM };
#define RT_LF_INVERT        0x01u
/* ---- cảnh báo ---- */
#define RT_AF_BEEP          0x01u
#define RT_AF_LEDBLINK      0x02u
#define RT_AF_ACK           0x04u   /* cần xác nhận (ESC) -> ghi 1 vào cuộn dây ack_addr */
/* ---- chung ---- */
enum { RT_MODE_MASTER = 0, RT_MODE_SLAVE = 1 };
#define RT_CF_VERIFY        0x01u   /* đọc lại sau MỌI lệnh ghi */
#define RT_CF_EXTKEYS       0x02u   /* dùng 8 phím mở rộng K3 (SW17..SW24) */
#define RT_BF_CLICK         0x01u
#define RT_BF_ERROR         0x02u
#define RT_BF_ALARM         0x04u

#pragma pack(push, 1)
typedef struct {
    uint8_t  type;          /* RT_OBJ_*  */
    uint8_t  area;          /* RT_AREA_* */
    uint16_t addr;          /* địa chỉ Modbus thô (0..65535) */
    uint8_t  dtype;         /* RT_DT_*   */
    uint8_t  order;         /* RT_ORD_*  */
    uint8_t  decimals;      /* 0..3 chữ số thập phân hiển thị */
    uint8_t  access;        /* 0 tự do, 1 mật khẩu vận hành, 2 mật khẩu kỹ thuật (khi sửa) */
    uint8_t  bit;           /* 0..15 = bit trong thanh ghi, 0xFF = cả thanh ghi / cuộn dây */
    uint8_t  flags;         /* RT_OF_* */
    uint8_t  label[4];      /* nhãn 7 đoạn (ASCII, bit7 = chấm); RT_OBJ_BIT: nhãn khi bit = 1 */
    int32_t  min;           /* giới hạn nhập (đơn vị hiển thị); RT_OBJ_BIT: nhãn khi bit = 0 (4 ký tự) */
    int32_t  max;
    int16_t  gain;          /* hệ số x1000 giữa giá trị PLC và giá trị hiển thị (0 = 1000) */
} rt_obj_t;                 /* 24 byte */

typedef struct {
    rt_obj_t top, bot;
    uint8_t  access;        /* quyền xem trang: 0/1/2 như đối tượng */
    uint8_t  rsvd;
} rt_page_t;                /* 50 byte */

typedef struct {
    uint8_t  action;        /* RT_FA_* */
    uint8_t  area;          /* RT_FAREA_* */
    uint16_t addr;
    uint8_t  bit;           /* với RT_FAREA_HOLDBIT */
    uint16_t value;         /* RT_FA_WRITE: giá trị ghi; RT_FA_GOTO: số trang */
    uint8_t  access;
} rt_fkey_t;                /* 8 byte */

typedef struct {
    uint8_t  area;          /* RT_AREA_* (đọc) */
    uint16_t addr;
    uint8_t  bit;
    uint8_t  mode;          /* RT_LM_* */
    uint8_t  flags;         /* RT_LF_* */
    uint8_t  rsvd[2];
} rt_led_t;                 /* 8 byte */

typedef struct {
    uint8_t  area;
    uint16_t addr;
    uint8_t  bit;           /* 0xFF = khác 0 là báo */
    uint8_t  flags;         /* RT_AF_* */
    uint8_t  led;           /* 0 = không, 1..8 = LED nhấp nháy khi báo */
    uint16_t ack_addr;      /* cuộn dây xác nhận */
} rt_alarm_t;               /* 8 byte */

typedef struct {
    uint8_t  mode;          /* RT_MODE_* */
    uint8_t  plc_profile;   /* chỉ để PC app ghi nhớ hồ sơ PLC */
    uint8_t  plc_id;        /* master: trạm PLC 1..247 */
    uint8_t  kbd_id;        /* slave: địa chỉ của bàn phím 1..247 */
    uint8_t  baud_idx;      /* 0:2400 1:4800 2:9600 3:19200 4:38400 5:57600 6:115200 */
    uint8_t  parity;        /* 0 không, 1 chẵn, 2 lẻ */
    uint8_t  stop;          /* 1 / 2 */
    uint8_t  order;         /* thứ tự word mặc định 1..4 */
    uint16_t timeout_ms;    /* 20..5000 */
    uint8_t  retries;       /* 0..5 */
    uint16_t poll_ms;       /* nghỉ giữa hai chu kỳ hỏi, 0..5000 */
    uint8_t  flags;         /* RT_CF_* */
    uint8_t  brightness;    /* 0..7 */
    uint8_t  beep;          /* RT_BF_* */
    uint16_t idle_s;        /* về trang RUN + đăng xuất sau N giây (0 = không) */
    uint8_t  run_page;
    uint8_t  page_count;    /* 0..32 */
    uint8_t  rsvd[2];
} rt_comm_t;                /* 22 byte */

typedef struct {
    uint16_t pwd_user;      /* 0 = không mật khẩu; 1..9999 */
    uint16_t pwd_tech;
    uint8_t  flags;
    uint8_t  rsvd[3];
} rt_sec_t;                 /* 8 byte */

typedef struct {
    uint32_t magic;
    uint16_t schema;
    uint16_t length;        /* = sizeof(rt_blob_t) */
    uint16_t revision;      /* PC app tăng mỗi lần nạp */
    uint16_t flags;
    rt_comm_t  comm;
    rt_sec_t   sec;
    rt_page_t  page[RT_MAX_PAGES];
    rt_fkey_t  fkey[RT_MAX_FKEYS];
    rt_led_t   led[RT_MAX_LEDS];
    rt_alarm_t alarm[RT_MAX_ALARMS];
    uint32_t crc32;         /* CRC-32 của mọi byte đứng trước */
} rt_blob_t;
#pragma pack(pop)

_Static_assert(sizeof(rt_obj_t) == 24, "rt_obj_t");
_Static_assert(sizeof(rt_page_t) == 50, "rt_page_t");
_Static_assert(sizeof(rt_fkey_t) == 8, "rt_fkey_t");
_Static_assert(sizeof(rt_led_t) == 8, "rt_led_t");
_Static_assert(sizeof(rt_alarm_t) == 8, "rt_alarm_t");
_Static_assert(sizeof(rt_comm_t) == 22, "rt_comm_t");
_Static_assert(sizeof(rt_sec_t) == 8, "rt_sec_t");
_Static_assert(sizeof(rt_blob_t) == 1918, "rt_blob_t");

#define RT_BLOB_SIZE        ((uint16_t)sizeof(rt_blob_t))

/* ---- mã lỗi kiểm tra ---- */
enum {
    RT_OK = 0, RT_E_MAGIC, RT_E_SCHEMA, RT_E_LENGTH, RT_E_CRC, RT_E_COMM, RT_E_PAGE,
    RT_E_FKEY, RT_E_LED, RT_E_ALARM, RT_E_SEC
};

void     rtcfg_defaults(rt_blob_t *b);      /* cấu hình xuất xưởng: slave, địa chỉ 1, 9600 8N1 */
void     rtcfg_seal(rt_blob_t *b);          /* đặt magic/schema/length + tính CRC */
uint32_t rtcfg_crc(const rt_blob_t *b);
uint8_t  rtcfg_validate(const rt_blob_t *b);        /* RT_OK hoặc RT_E_* */
uint32_t rtcfg_baud(uint8_t baud_idx);

/* tiện ích dùng chung */
uint8_t  rt_nregs(const rt_obj_t *o);       /* số thanh ghi cần đọc: 1 hoặc 2 */
bool     rt_is_writable(const rt_obj_t *o);

#endif
