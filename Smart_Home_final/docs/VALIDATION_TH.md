# ผลตรวจชุด Smart_Home_final

วันที่ 9 ตุลาคม 2026; ฐาน source: Smart Home 1.4 R3

| การตรวจ | ผล |
|---|---|
| เทียบ token C/H กับต้นฉบับโดยตัดเฉพาะ comments/whitespace | ผ่าน 25 ไฟล์; string literals และคำสั่งไม่เปลี่ยน |
| `./build.ps1` model NTC เริ่มต้น | ผ่าน ARM GCC flags `-Wall -Wextra -Werror` |
| ELF size | text 8148, data 80, bss 4056 bytes |
| `./live_status.ps1 -SelfTest` | ผ่าน fragmented/complete/invalid/legacy frames, mapping, sensor labels, GPIO diagnostics และ stale status |
| ARM emulator regression | **ยังไม่ได้รันสำเร็จ**: import Unicorn ไม่พร้อม และ pip ติดตั้ง unicorn==2.1.4 ไม่สำเร็จในสภาพแวดล้อมนี้ |
| CubeIDE Debug/Release | ปรับ metadata และ libm แล้ว; **ไม่ได้เปิด IDE เพื่อ build/debug จริง** |
| Flash และฮาร์ดแวร์ | **ไม่ได้ทำ** |
| MISRA compliance | **ไม่ได้รับรอง** ต้องตรวจด้วย check sheet และ analyzer |

ผล firmware ใหม่อยู่ `Firmware/Smart_Home_final.elf`, `.hex`, `.map` ไม่ได้นำ binary เก่ามาเปลี่ยนชื่อ ผล build ผ่านยืนยันการคอมไพล์และลิงก์ในเส้นทาง PowerShell เท่านั้น ไม่ยืนยันฮาร์ดแวร์หรือข้อจำกัด runtime ที่มีอยู่เดิม

ชุด ZIP ไม่รวม tests/.deps, tests/build, object files และ Debug/Release เพื่อให้รับไป build ใหม่ได้สะอาด ประวัติรุ่นใน docs/history เป็นข้อมูลเดิม ไม่ใช่หลักฐานว่ารันการทดสอบเหล่านั้นซ้ำในชุด final
