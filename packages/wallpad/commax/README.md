# 코맥스 월패드 구성 (검증x)
```
packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents/
    files:
      - packages/wallpad/commax/commax.yaml
```

# 코맥스 월패드 패킷 구조
## 📌 장치별 상세 패킷 정보
---

### 1️⃣ 일괄소등

**상태/응답**
```
[Head:0xA0/A2][ON/OFF][ID][0x00][0x00][고정값:0x17][0x00][checksum]

- Head: 상태(0xA0), 응답(0xA2)
- ON/OFF: ON(0x01), OFF(0x00)
- ID: 장치 번호
- 0x17: 고정값
```
```
상태 ON : 0xA0,0x01,0x01,0x00,0x00,0x17,0x00,[checksum]
상태 OFF: 0xA0,0x00,0x01,0x00,0x00,0x17,0x00,[checksum]
응답 ON : 0xA2,0x01,0x01,0x00,0x00,0x17,0x00,[checksum]
응답 OFF: 0xA2,0x00,0x01,0x00,0x00,0x17,0x00,[checksum]
```

**명령**
```
[Head:0x22][ID][ON/OFF][고정값:0x01][0x00][0x00][0x00][checksum]

- ON/OFF: ON(0x01), OFF(0x00)
```
```
명령 ON : 0x22,0x01,0x01,0x01,0x00,0x00,0x00,[checksum]
명령 OFF: 0x22,0x01,0x00,0x01,0x00,0x00,0x00,[checksum]
```

---

### 2️⃣ 조명

**상태/응답**
```
[Head:0xB0/B1][ON/OFF][ID][0x00][0x00][0x00][0x00][checksum]

- Head: 상태(0xB0), 응답(0xB1)
```
```
상태 ON : 0xB0,0x01,0x01,0x00,0x00,0x00,0x00,[checksum]
상태 OFF: 0xB0,0x00,0x01,0x00,0x00,0x00,0x00,[checksum]
응답 ON : 0xB1,0x01,0x01,0x00,0x00,0x00,0x00,[checksum]
응답 OFF: 0xB1,0x00,0x01,0x00,0x00,0x00,0x00,[checksum]
```

**명령**
```
[Head:0x31][ID][ON/OFF][0x00][0x00][0x00][0x00][checksum]
```
```
명령 ON : 0x31,0x01,0x01,0x00,0x00,0x00,0x00,[checksum]
명령 OFF: 0x31,0x01,0x00,0x00,0x00,0x00,0x00,[checksum]
```

---

### 3️⃣ 보일러

**상태/응답**
```
[Head:0x82/84][상태값][ID][Cur][Tar][0x00][0x00][checksum]

- Head: 상태(0x82), 응답(0x84)
- 상태값: ON(0x81), OFF(0x80), HEAT(0x83)
- Cur: 현재 온도, Tar: 목표 온도
-- 온도값은 Hex 값이 10진수 온도 0x26 => 26도
```
```
상태 ON  : 0x82,0x81,0x01,[Cur],[Tar],0x00,0x00,[checksum]
상태 OFF : 0x82,0x80,0x01,[Cur],[Tar],0x00,0x00,[checksum]
상태 HEAT: 0x82,0x83,0x01,[Cur],[Tar],0x00,0x00,[checksum]
응답 ON  : 0x84,0x81,0x01,[Cur],[Tar],0x00,0x00,[checksum]
응답 OFF : 0x84,0x80,0x01,[Cur],[Tar],0x00,0x00,[checksum]
응답 HEAT: 0x84,0x83,0x01,[Cur],[Tar],0x00,0x00,[checksum]
```

**명령**
```
[Head:0x04][ID][CMD][ON/OFF 또는 Tar][0x00][0x00][0x00][checksum]

- CMD: ON/OFF(0x04), 온도변경(0x03)
- ON/OFF: ON(0x81), OFF(0x00)
```
```
명령 ON  : 0x04,0x01,0x04,0x81,0x00,0x00,0x00,[checksum]
명령 OFF : 0x04,0x01,0x04,0x00,0x00,0x00,0x00,[checksum]
온도 변경: 0x04,0x01,0x03,[Tar],0x00,0x00,0x00,[checksum]
```

---

### 4️⃣ 팬 (FAN)

**상태/응답**
```
[Head:0xF6/F8][상태값][ID][SPD][0x00][0x00][0x00][checksum]

- Head: 상태(0xF6), 응답(0xF8)
- 상태값: OFF(0x00), AUTO(0x02), LOW/MID/HIGH(0x04), NIGHT(0x06)
- SPD: OFF(0x00), LOW(0x01), MID(0x02), HIGH(0x03), ON(0x04)
```
```
상태 OFF : 0xF6,0x00,0x01,0x00,0x00,0x00,0x00,[checksum]
상태 LOW : 0xF6,0x04,0x01,0x01,0x00,0x00,0x00,[checksum]
상태 MID : 0xF6,0x04,0x01,0x02,0x00,0x00,0x00,[checksum]
상태 HIGH: 0xF6,0x04,0x01,0x03,0x00,0x00,0x00,[checksum]
상태 AUTO: 0xF6,0x02,0x01,0x01,0x00,0x00,0x00,[checksum]
상태 NIGHT:0xF6,0x06,0x01,0x01,0x00,0x00,0x00,[checksum]

응답(동일구조, Head만 0xF8로 변경)
```

**명령**
```
[Head:0x78][ID][CMD][SPD][0x00][0x00][0x00][checksum]

- CMD: ON/OFF(0x01), 속도변경(0x02)
```
```
명령 OFF : 0x78,0x01,0x01,0x00,0x00,0x00,0x00,[checksum]
명령 ON  : 0x78,0x01,0x01,0x04,0x00,0x00,0x00,[checksum]
명령 LOW : 0x78,0x01,0x02,0x01,0x00,0x00,0x00,[checksum]
명령 MID : 0x78,0x01,0x02,0x02,0x00,0x00,0x00,[checksum]
명령 HIGH: 0x78,0x01,0x02,0x03,0x00,0x00,0x00,[checksum]
```

---

### 5️⃣ 가스 밸브

**상태/응답**
```
[Head:0x90/91][상태값][상태값][0x00][0x00][0x00][0x00][checksum]

- Head: 상태(0x90), 응답(0x91)
- 상태: ON(0x80,0x80), OFF(0x40,0x40), 
- 응답: OFF(0x88)
```
```
상태 ON : 0x90,0x80,0x80,0x00,0x00,0x00,0x00,[checksum]
상태 OFF: 0x90,0x40,0x40,0x00,0x00,0x00,0x00,[checksum]
응답 OFF: 0x91,0x88,0x88,0x00,0x00,0x00,0x00,[checksum]
```

**명령**
```
[Head:0x11][ID][상태값:OFF(0x80)][0x00][0x00][0x00][0x00][checksum]
```
```
명령 OFF: 0x11,0x01,0x80,0x00,0x00,0x00,0x00,[checksum]
```

---

### 6️⃣ 엘리베이터

**상태**
```
상태 조회: 0x23,0x01,[층수],0x00,0x00,0x00,0x00,[checksum]
```

**명령**
```
호출 명령: 0xA0,0x01,0x01,0x00,0x08,0x15,0x00,[checksum]
```

---

## ⚠️ 체크섬(checksum) 계산 방법

```
checksum = (Byte0 + Byte1 + Byte2 + ... + Byte7) & 0xFF
```